#include "Player.h"
#include <RizeEngine.h>
#include "Ball.h"
#include "MapChip.h"
#include "input/Input.h"
#include "DrawHelper.h"
#include <algorithm>
#include <cmath>
#include "ParticleSystem.h"

namespace {
	const float kPi = 3.14159265f;

	// ---- player.obj (slime) ----
	// Model space: x,z in [-1, 1], y in [-0.162, 1.0] (flat-ish bottom at y = -0.162, pivot at sphere center)
	const float kModelBottomY = 0.162096f;
	// Model::LoadOBJ mirrors X (right-handed -> left-handed), so the slime's front (+X in player.obj)
	// ends up pointing LEFT in the game. PI turns it to face right.
	// If the face still points the wrong way, try 0, PI/2 or -PI/2.
	const float kFrontYawOffset = 3.14159265f;
	// World size = model size * scale: width = 2 * kBaseScaleXZ, height = 1.162 * kBaseScaleY
	const float kBaseScaleXZ = 0.28f; // ~0.56 wide
	const float kBaseScaleY = 0.50f;  // ~0.58 tall (hurtbox is 0.6)

	// ---- punch timeline (total = swing duration 0.45s) ----
	const float kSwingDuration = 0.45f;
	const float kWindupTime = 0.08f; // shrink (crouch) before the jump
	const float kLungeEnd = 0.28f;   // jump + attack until here, then landing recovery

	// ---- damage bounce ----
	const float kDamageAnimTime = 0.7f;

	float Lerp(float a, float b, float t) { return a + (b - a) * t; }
	float EaseOut(float t) { return 1.0f - (1.0f - t) * (1.0f - t); }
}

void Player::Initialize(Model* slimeModel, Camera* camera)
{
	// Initialize player properties
	position_ = { 0.0f, 0.0f, 0.0f };
	velocity_ = { 0.0f, 0.0f, 0.0f };
	isGrounded_ = false;
	isFacingRight_ = true;
	state_ = PlayerState::idle;
	swingTimer_ = 0.0f;
	tossCooldown_ = 0.0f;
	hp_ = maxHP_;
	invincibleTimer_ = 0.0f;
	knockbackTimer_ = 0.0f;
	footstepDustTimer_ = 0.0f;
	// Initialize hurt box (height: 0.6f, halfHeight: 0.3f, halfWidth: 0.2f)
	hurtBox_.attribute = CollisionAttribute::kPlayer;
	hurtBox_.mask = CollisionAttribute::kEnemy | CollisionAttribute::kGround | CollisionAttribute::kObstacle;
	hurtBox_.enabled = true;
	hurtBox_.SetCenterSize(position_ + Vector3(0.0f, 0.3f, 0.0f), Vector3(0.2f, 0.3f, 0.4f));
	// Initialize punch hitbox (Slime Punch System, keeping kBat internally for engine compatibility)
	punchHitbox_.player = this;
	punchHitbox_.isActive = false;
	punchHitbox_.collider.attribute = CollisionAttribute::kBat;
	punchHitbox_.collider.mask = CollisionAttribute::kBall;
	punchHitbox_.collider.enabled = false;
	punchHitbox_.collider.SetCenterSize(position_, Vector3(0.4f, 0.4f, 0.4f));

	// Slime visual state
	animTime_ = 0.0f;
	damageAnimTimer_ = kDamageAnimTime; // no bounce at start
	poseSY_ = 1.0f;
	poseLean_ = 0.0f;
	visualYaw_ = kFrontYawOffset;
	finalSY_ = 1.0f;
	finalLean_ = 0.0f;
	visualOffset_ = { 0.0f, 0.0f, 0.0f };

	// Initialize slime model (player.obj + player.mtl + slimeTex.png)
	playerObj_.Initialize();
	playerObj_.SetModel(slimeModel);
	playerObj_.SetCamera(camera);
	playerObj_.GetMaterial()->color = { 1.0f, 1.0f, 1.0f, 1.0f }; // natural texture color
	playerObj_.GetMaterial()->enableLighting = 1;
	playerObj_.GetMaterial()->lightingType = 2; // Half-Lambert paper diffuse
	ApplySlimeTransform();
	playerObj_.Update(0.0f);
}

void Player::Update(float deltaTime, Ball* ball)
{
	Input* input = RizeEngine::GetInput();
	punchHitbox_.ball = ball;

	//Horizontal movement (input is ignored briefly while being knocked back)
	float moveSpeed = 5.0f;
	if (knockbackTimer_ > 0.0f)
	{
		knockbackTimer_ -= deltaTime;
	}
	else
	{
		velocity_.x = 0.0f;

		if (input->PushKey(DIK_A) || input->PushKey(DIK_LEFT))
		{
			velocity_.x = -moveSpeed;
			isFacingRight_ = false;
		}
		if (input->PushKey(DIK_D) || input->PushKey(DIK_RIGHT))
		{
			velocity_.x = moveSpeed;
			isFacingRight_ = true;
		}
	}

	if ((input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_W) || input->TriggerKey(DIK_UP)) && isGrounded_)
	{
		velocity_.y = 10.5f; // Jump velocity (clears 4-5 tile heights comfortably)
		isGrounded_ = false;
		state_ = PlayerState::jumping;
	}

	// Apply gravity
	bool wasGrounded = isGrounded_;

	velocity_.y -= 20.0f * deltaTime; // Gravity acceleration
	isGrounded_ = false;

	position_ += velocity_ * deltaTime;
	position_.z = 0.0f; // Keep player on the 2d plane

	//Actions: toss ball
	if (tossCooldown_ > 0.0f)
	{
		tossCooldown_ -= deltaTime;
	}
	if (input->TriggerKey(DIK_J) && tossCooldown_ <= 0.0f)
	{
		if (ball && !ball->isAlive())
		{
			Vector3 tossPos = position_ + Vector3(isFacingRight_ ? 0.3f : -0.3f, 0.45f, 0.0f);
			ball->Toss(tossPos);
			state_ = PlayerState::tossing;
			tossCooldown_ = 0.5f;
		}
	}
	//Slime Punch Attack
	if (input->TriggerKey(DIK_K) && swingTimer_ <= 0.0f) {
		swingTimer_ = 0.45f;
		punchHitbox_.isActive = true;
		punchHitbox_.collider.enabled = true;
		state_ = PlayerState::hitting;
	}
	if (swingTimer_ > 0.0f) {
		swingTimer_ -= deltaTime;
		if (swingTimer_ <= 0.20f) {
			punchHitbox_.isActive = false;
			punchHitbox_.collider.enabled = false;
		}
		if (swingTimer_ <= 0.0f) {
			swingTimer_ = 0.0f;
			state_ = wasGrounded ? (std::abs(velocity_.x) > 0.1f ? PlayerState::running : PlayerState::idle) : PlayerState::jumping;
		}
	}
	//Recall Ball
	if (input->TriggerKey(DIK_R))
	{
		if (ball && ball->isAlive())
		{
			Vector3 ballPos = ball->GetPosition();
			Vector3 playerPos = position_;

			if (particleSystem_) {
				particleSystem_->EmitBallRecallBurst(ballPos);
				particleSystem_->EmitPlayerRecallAbsorb(playerPos, ballPos);
			}

			ball->Despawn();
			state_ = PlayerState::idle;
		}
	}

	// Update State Animation
	if (swingTimer_ <= 0.0f) {
		if (!wasGrounded) {
			state_ = PlayerState::jumping;
		}
		else if (std::abs(velocity_.x) > 0.1f) {
			state_ = PlayerState::running;
		}
		else {
			state_ = PlayerState::idle;
		}
	}

	// Footstep dust emission (Feature 2)
	if (state_ == PlayerState::running && wasGrounded && std::abs(velocity_.x) > 0.1f) {
		footstepDustTimer_ -= deltaTime;
		if (footstepDustTimer_ <= 0.0f) {
			footstepDustTimer_ = 0.10f;
			if (particleSystem_) {
				particleSystem_->EmitFootstepDust(Vector3(position_.x, position_.y, -0.1f), isFacingRight_);
			}
		}
	}
	else {
		footstepDustTimer_ = 0.0f;
	}

	//synchronize hurtbox and punch hitbox position with player position
	float halfWidth = 0.2f;
	float halfHeight = 0.3f;
	//Player hitboxSize (height 0.6f)
	hurtBox_.SetCenterSize(
		position_ + Vector3(0.0f, halfHeight, 0.0f),//middle part
		Vector3(halfWidth, halfHeight, 0.4f)//hitboxSize
	);
	Vector3 punchCenter = position_ + Vector3(isFacingRight_ ? 0.35f : -0.35f, 0.25f, 0.0f);
	punchHitbox_.collider.SetCenterSize(punchCenter, Vector3(0.35f, 0.35f, 0.4f));

	// Damage state + white blink while invincible
	if (hp_ <= 0)
	{
		state_ = PlayerState::dead;
	}
	else if (knockbackTimer_ > 0.0f)
	{
		state_ = PlayerState::damaged;
	}

	if (invincibleTimer_ > 0.0f)
	{
		invincibleTimer_ -= deltaTime;
	}
	if (invincibleTimer_ > 0.0f && (static_cast<int>(invincibleTimer_ / 0.08f) % 2) == 0)
	{
		// White flash: over-bright + unlit so the slime texture washes out to white
		playerObj_.GetMaterial()->color = { 4.0f, 4.0f, 4.0f, 1.0f };
		playerObj_.GetMaterial()->enableLighting = 0;
	}
	else
	{
		playerObj_.GetMaterial()->color = { 1.0f, 1.0f, 1.0f, 1.0f }; // natural texture color
		playerObj_.GetMaterial()->enableLighting = 1;
	playerObj_.GetMaterial()->lightingType = 2; // Half-Lambert paper diffuse
	}

	// Keep player within horizontal map left boundary
	if (position_.x < 0.45f) {
		position_.x = 0.45f;
		if (velocity_.x < 0.0f) {
			velocity_.x = 0.0f;
		}
	}

	// Slime pose (idle / run / punch / damage) -> transform
	UpdateSlimeVisual(deltaTime, wasGrounded);
	playerObj_.Update(deltaTime);
}

void Player::SetPosition(const Vector3& pos)
{
	position_ = pos;
	const float halfWidth = 0.2f;
	const float halfHeight = 0.3f;
	hurtBox_.SetCenterSize(position_ + Vector3(0.0f, halfHeight, 0.0f), Vector3(halfWidth, halfHeight, 0.4f));

	ApplySlimeTransform();
	playerObj_.Update(0.0f);
}

void Player::OnCollision(ICollidable* other)
{
	if (!other)
	{
		return;
	}
	uint32_t targetAttr = 0;
	if (other->GetColliderShape() == ColliderShape::AABB) {
		targetAttr = other->GetAABBCollider().attribute;
	}

	// Collision with Ground or Obstacle
	if ((targetAttr & CollisionAttribute::kGround) || (targetAttr & CollisionAttribute::kObstacle)) {
		const AABBCollider& box = other->GetAABBCollider();

		TileBlock* tile = dynamic_cast<TileBlock*>(other);
		bool hasTop = tile ? tile->hasTop : true;
		bool hasBottom = tile ? tile->hasBottom : true;
		bool hasLeft = tile ? tile->hasLeft : true;
		bool hasRight = tile ? tile->hasRight : true;

		const float halfWidth = 0.2f;
		const float halfHeight = 0.3f;
		const float playerHeight = 0.6f;

		// Hitbox player bounds
		float playerMinX = position_.x - halfWidth;
		float playerMaxX = position_.x + halfWidth;
		float playerMinY = position_.y;
		float playerMaxY = position_.y + playerHeight;

		// Overlap distances
		float overlapX1 = playerMaxX - box.min.x; // moving right into left face of box
		float overlapX2 = box.max.x - playerMinX; // moving left into right face of box
		float overlapX = (std::min)(overlapX1, overlapX2);

		float overlapY1 = playerMaxY - box.min.y; // penetration into bottom of box (ceiling)
		float overlapY2 = box.max.y - playerMinY; // penetration into top of box (floor)
		float overlapY = (std::min)(overlapY1, overlapY2);

		if (overlapX <= 0.0f || overlapY <= 0.0f) return;

		bool canLandOnTop = hasTop && (overlapY2 < overlapY1) && (velocity_.y <= 0.0f);
		bool canHitCeiling = hasBottom && (overlapY1 < overlapY2);

		bool resolveY = false;
		if (canLandOnTop && (overlapY2 < 0.12f || overlapY2 < overlapX)) {
			resolveY = true;
		}
		else if (canHitCeiling && (overlapY1 < 0.12f || overlapY1 < overlapX)) {
			resolveY = true;
		}

		if (resolveY) {
			if (overlapY1 < overlapY2) {
				position_.y = box.min.y - playerHeight;
				if (velocity_.y > 0.0f) {
					velocity_.y = 0.0f;
				}
			}
			else {
				position_.y = box.max.y;
				velocity_.y = 0.0f;
				isGrounded_ = true;
				if (tile && tile->isMoving && tile->moveType == 1) {
					position_.x += tile->deltaPos.x;
				}
			}
		}
		else {
			// Wall collision (resolve horizontally)
			if (hasRight && !hasLeft) {
				position_.x = box.max.x + halfWidth;
				if (velocity_.x < 0.0f) {
					velocity_.x = 0.0f;
				}
			}
			else if (hasLeft && !hasRight) {
				position_.x = box.min.x - halfWidth;
				if (velocity_.x > 0.0f) {
					velocity_.x = 0.0f;
				}
			}
			else if (overlapX1 < overlapX2) {
				if (hasLeft) {
					position_.x = box.min.x - halfWidth;
					if (velocity_.x > 0.0f) {
						velocity_.x = 0.0f;
					}
				}
			}
			else {
				if (hasRight) {
					position_.x = box.max.x + halfWidth;
					if (velocity_.x < 0.0f) {
						velocity_.x = 0.0f;
					}
				}
			}
		}

		// Keep player within horizontal map left boundary
		if (position_.x < 0.45f) {
			position_.x = 0.45f;
			if (velocity_.x < 0.0f) {
				velocity_.x = 0.0f;
			}
		}

		hurtBox_.SetCenterSize(
			position_ + Vector3(0.0f, halfHeight, 0.0f),
			Vector3(halfWidth, halfHeight, 0.4f)
		);
	}
	// Damage and knockback from enemy
	if (targetAttr & CollisionAttribute::kEnemy)
	{
		const AABBCollider& enemyBox = other->GetAABBCollider();
		float enemyCenterX = (enemyBox.min.x + enemyBox.max.x) * 0.5f;
		TakeDamage(1, enemyCenterX);
	}
}

void Player::TakeDamage(int damage, float sourceX)
{
	// Ignore while invincible (i-frames) or already dead
	if (invincibleTimer_ > 0.0f || hp_ <= 0)
	{
		return;
	}

	hp_ = (std::max)(0, hp_ - damage);
	invincibleTimer_ = 1.0f; // i-frames + blink duration
	damageAnimTimer_ = 0.0f; // start slime bounce

	// Knockback away from the enemy
	float dir = (position_.x >= sourceX) ? 1.0f : -1.0f;
	velocity_.x = dir * 6.0f;
	velocity_.y = 5.0f;
	knockbackTimer_ = 0.2f;
	isGrounded_ = false;

	state_ = (hp_ <= 0) ? PlayerState::dead : PlayerState::damaged;
}

void Player::UpdateSlimeVisual(float deltaTime, bool grounded)
{
	animTime_ += deltaTime;
	damageAnimTimer_ += deltaTime;

	const float dir = isFacingRight_ ? 1.0f : -1.0f;

	// ---- target pose per phase (vertical scale multiplier, forward lean) ----
	float targetSY = 1.0f;
	float targetLean = 0.0f;
	float smoothRate = 12.0f; // how fast the pose follows the target
	float osc = 0.0f;         // continuous wobble added on top
	Vector3 offset = { 0.0f, 0.0f, 0.0f };

	if (swingTimer_ > 0.0f)
	{
		// PUNCH: shrink (crouch) -> jump forward + attack -> landing squash -> back to normal
		const float e = kSwingDuration - swingTimer_; // elapsed time of the swing
		smoothRate = 40.0f;                           // snappy

		if (e < kWindupTime)
		{
			// 1) shrink briefly before jumping
			const float t = e / kWindupTime;
			targetSY = Lerp(1.0f, 0.55f, t);
			targetLean = -0.15f * t; // lean back a little (anticipation)
		}
		else if (e < kLungeEnd)
		{
			// 2) jump forward and strike: stretch tall, lean forward, arc up and forward (visual only)
			const float u = (e - kWindupTime) / (kLungeEnd - kWindupTime);
			const float arc = std::sin(kPi * u);
			targetSY = 1.0f + 0.25f * arc;
			targetLean = 0.35f * arc;
			offset = { dir * 0.30f * arc, 0.20f * arc, 0.0f };
		}
		else
		{
			// 3) landing squash, then recover
			const float w = (e - kLungeEnd) / (kSwingDuration - kLungeEnd);
			targetSY = Lerp(0.75f, 1.0f, EaseOut(w));
		}
	}
	else if (!grounded)
	{
		// JUMP: stretch while rising, a bit less while falling
		targetSY = (velocity_.y > 0.0f) ? 1.18f : 1.08f;
	}
	else if (std::abs(velocity_.x) > 0.1f)
	{
		// RUN: flatten + lean forward, small bobbing
		targetSY = 0.82f;
		targetLean = 0.28f;
		osc = 0.05f * std::sin(animTime_ * 16.0f);
	}
	else
	{
		// IDLE: breathing (taller/shorter = bigger/smaller)
		osc = 0.07f * std::sin(animTime_ * 3.2f);
	}

	// ---- smooth towards the target ----
	const float k = 1.0f - std::exp(-smoothRate * deltaTime);
	poseSY_ += (targetSY - poseSY_) * k;
	poseLean_ += (targetLean - poseLean_) * k;

	// Turn around smoothly when changing direction (right = 0, left = PI)
	const float targetYaw = (isFacingRight_ ? 0.0f : kPi) + kFrontYawOffset;
	visualYaw_ += (targetYaw - visualYaw_) * (1.0f - std::exp(-18.0f * deltaTime));

	float sy = poseSY_ + osc;
	float roll = 0.0f;

	// ---- DAMAGE: slime gets squashed, then bounces (damped spring) and jiggles ----
	if (damageAnimTimer_ < kDamageAnimTime)
	{
		const float t = damageAnimTimer_;
		sy *= 1.0f - 0.5f * std::exp(-5.0f * t) * std::cos(2.0f * kPi * 3.0f * t);
		roll = 0.25f * std::exp(-6.0f * t) * std::sin(2.0f * kPi * 4.0f * t);
	}

	finalSY_ = (std::max)(0.35f, (std::min)(sy, 1.6f));
	finalLean_ = dir * poseLean_ + roll;
	visualOffset_ = offset;

	ApplySlimeTransform();
}

void Player::ApplySlimeTransform()
{
	// Volume preservation: taller -> thinner, shorter -> wider
	const float sxz = 1.0f / std::sqrt(finalSY_);

	playerObj_.worldTransform.scale = { kBaseScaleXZ * sxz, kBaseScaleY * finalSY_, kBaseScaleXZ * sxz };
	// Yaw = facing, Roll (Z) = lean/wobble in the screen plane
	playerObj_.worldTransform.rotation = { 0.0f, visualYaw_, finalLean_ };

	// Keep the flat bottom of the slime on the ground even while squashing/stretching
	const float footOffset = kModelBottomY * kBaseScaleY * finalSY_;
	playerObj_.worldTransform.translation = position_ + Vector3(0.0f, footOffset, 0.0f) + visualOffset_;
}

void Player::SetHoldBall()
{
	state_ = PlayerState::idle;
}

void Player::Draw() {
	// Re-apply here so the position corrected by collision this frame is drawn right away
	ApplySlimeTransform();
	playerObj_.Update(0.0f);

	auto* cmdList = RizeEngine::GetDirectXCommon()->GetCommandList();
	playerObj_.Draw(cmdList);
}

void Player::PunchHitBox::OnCollision(ICollidable* other)
{
	if (!isActive || !other || !player || !ball) return;

	uint32_t targetAttr = 0;
	if (other->GetColliderShape() == ColliderShape::Sphere) {
		targetAttr = other->GetSphereCollider().attribute;
	}
	else if (other->GetColliderShape() == ColliderShape::AABB) {
		targetAttr = other->GetAABBCollider().attribute;
	}

	if (targetAttr & CollisionAttribute::kBall) {
		float dirY = 0.0f;
		Input* input = RizeEngine::GetInput();
		if (input->PushKey(DIK_W) || input->PushKey(DIK_UP)) {
			dirY = 0.55f;
		}
		else if (input->PushKey(DIK_S) || input->PushKey(DIK_DOWN)) {
			dirY = -0.35f;
		}
		else {
			// Jika posisi Y bola dan pukulan player sejajar, bola melesat lurus
			Vector3 punchCenter = (collider.min + collider.max) * 0.5f;
			float diffY = ball->GetPosition().y - punchCenter.y;
			if (std::abs(diffY) < 0.08f) {
				dirY = 0.0f; // Pantulan lurus horizontal
			}
			else {
				// Miring sesuai sudut ketinggian bola relatif terhadap pukulan
				dirY = std::clamp(diffY * 1.2f, -0.45f, 0.45f);
			}
		}

		Vector3 hitDir = player->IsFacingRight()
			? Vector3(1.0f, dirY, 0.0f)
			: Vector3(-1.0f, dirY, 0.0f);

		ball->GotHit(hitDir);
		isActive = false;
		collider.enabled = false;

		if (player && player->GetParticleSystem()) {
			Vector3 contactPoint = ClosestPoint(ball->GetPosition(), collider);
			contactPoint.z = -0.1f;
			player->GetParticleSystem()->EmitPunchHitBurst(contactPoint);
		}
	}
}
