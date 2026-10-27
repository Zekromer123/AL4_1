#include "Enemy.h"
#include "Ball.h"
#include "MapChip.h"
#include "ParticleSystem.h"
#include <algorithm>
#include <cmath>

void Enemy::Initialize(Model* groundModel, Model* flyingModel, Camera* camera, const Vector3& spawnPos, bool isFlying)
{
	isFlying_ = isFlying || MapChip::IsFlyingSpawn(spawnPos);
	spawnY_ = spawnPos.y;
	flyTimer_ = 0.0f;

	position_ = spawnPos;
	position_.z = 0.0f;
	velocity_ = { 0.0f, 0.0f, 0.0f };
	moveDir_ = -1.0f;
	patrolMinX_ = spawnPos.x - kPatrolRange;
	patrolMaxX_ = spawnPos.x + kPatrolRange;

	state_ = EnemyState::Walking;
	launchTimer_ = 0.0f;
	flashTimer_ = 0.0f;

	// Collider: enemy body. Collides with player, ball and terrain
	hurtBox_.attribute = CollisionAttribute::kEnemy;
	hurtBox_.mask = CollisionAttribute::kPlayer | CollisionAttribute::kBall | CollisionAttribute::kGround | CollisionAttribute::kObstacle;
	hurtBox_.enabled = true;
	SyncCollider();

	// Model space of daruma.obj / lantern.obj: y in [-1, 1], radius <= 1, face looks at -Z (camera)
	obj_.Initialize();
	obj_.SetModel(isFlying_ ? flyingModel : groundModel);
	obj_.SetCamera(camera);
	obj_.worldTransform.scale = { kHalfWidth, kHalfHeight, kHalfWidth };
	obj_.worldTransform.rotation = { 0.0f, 0.0f, 0.0f };
	wobbleTimer_ = spawnPos.x; // desync the idle animation between enemies
	UpdateFlash(0.0f);
	obj_.worldTransform.translation = position_ + Vector3(0.0f, kHalfHeight, 0.0f);
	obj_.Update(0.0f);
}

void Enemy::Update(float deltaTime)
{
	switch (state_)
	{
	case EnemyState::Walking:
		if (isFlying_)
		{
			// Flying enemy: oscillates smoothly UP and DOWN, no gravity
			flyTimer_ += deltaTime;
			position_.y = spawnY_ + std::sin(flyTimer_ * flySpeed_) * flyAmplitude_;
			velocity_ = { 0.0f, 0.0f, 0.0f };
			position_.z = 0.0f;
		}
		else
		{
			// Patrol left-right around the spawn point
			velocity_.x = moveDir_ * kWalkSpeed;
			velocity_.y += kGravity * deltaTime;
			velocity_.y = (std::max)(velocity_.y, -kMaxFallSpeed);
			position_ += velocity_ * deltaTime;
			position_.z = 0.0f;

			if (position_.x >= patrolMaxX_)
			{
				position_.x = patrolMaxX_;
				moveDir_ = -1.0f;
			}
			else if (position_.x <= patrolMinX_)
			{
				position_.x = patrolMinX_;
				moveDir_ = 1.0f;
			}
		}

		// Idle animation
		wobbleTimer_ += deltaTime;
		obj_.worldTransform.rotation.z = isFlying_
			? std::sin(wobbleTimer_ * 1.6f) * 0.18f  // lantern sways on its string
			: std::sin(wobbleTimer_ * 9.0f) * 0.12f; // daruma rocks while walking
		break;

	case EnemyState::Launched:
		// Flying out of the arena: ignores terrain, only gravity
		velocity_.y += kGravity * deltaTime;
		position_ += velocity_ * deltaTime;
		launchTimer_ += deltaTime;

		// tumbling effect
		obj_.worldTransform.rotation.z += 8.0f * deltaTime;
		obj_.worldTransform.rotation.x += 6.0f * deltaTime;

		if (launchTimer_ >= kLaunchDuration || position_.y < kKillY || position_.z <= -12.0f)
		{
			state_ = EnemyState::Dead;
		}
		break;

	case EnemyState::Dead:
		return;
	}

	SyncCollider();
	UpdateFlash(deltaTime);
}

void Enemy::Draw()
{
	if (state_ == EnemyState::Dead) return;

	// Sync here so the position corrected by collision this frame is drawn right away
	obj_.worldTransform.translation = position_ + Vector3(0.0f, kHalfHeight, 0.0f);
	obj_.Update(0.0f);

	auto* cmdList = RizeEngine::GetDirectXCommon()->GetCommandList();
	obj_.Draw(cmdList);
}

void Enemy::OnCollision(ICollidable* other)
{
	if (!other || state_ != EnemyState::Walking)
	{
		return;
	}

	// Hit by ball (hanya terdeteksi jika bola sudah dipukul bat, karena mask kEnemy bola hanya aktif setelah dipukul bat)
	if (other->GetColliderShape() == ColliderShape::Sphere)
	{
		const SphereCollider& sphere = other->GetSphereCollider();
		if (sphere.attribute & CollisionAttribute::kBall)
		{
			Ball* ball = dynamic_cast<Ball*>(other);
			Vector3 ballVel = ball ? ball->GetVelocity() : Vector3(0.0f, 0.0f, 0.0f);
			GotHit(sphere.center, ballVel);
		}
		return;
	}

	// Terrain
	if (other->GetColliderShape() == ColliderShape::AABB)
	{
		if (isFlying_)
		{
			return; // Flying enemy ignores terrain collisions
		}

		const AABBCollider& box = other->GetAABBCollider();
		if ((box.attribute & CollisionAttribute::kGround) || (box.attribute & CollisionAttribute::kObstacle))
		{
			ResolveTerrain(box);
		}
		// Player contact is handled in Player::OnCollision (damage + knockback)
	}
}

void Enemy::GotHit(const Vector3& ballPos, const Vector3& impactVelocity)
{
	// Knock away from the ball, up and to the side, then gravity pulls it out of the arena
	float dirX = (position_.x >= ballPos.x) ? 1.0f : -1.0f;
	velocity_ = { dirX * kKnockbackX, kKnockbackY, kKnockbackZ };

	state_ = EnemyState::Launched;
	launchTimer_ = 0.0f;
	flashTimer_ = 0.0f;
	hurtBox_.enabled = false; // no more collision (no double hit / no damage to player)

	if (particleSystem_)
	{
		Vector3 impactPoint = ClosestPoint(ballPos, hurtBox_);
		impactPoint.z = -0.1f;
		particleSystem_->EmitEnemySparks(impactPoint, impactVelocity);
	}
}

void Enemy::ResolveTerrain(const AABBCollider& box)
{
	const float height = kHalfHeight * 2.0f;

	float minX = position_.x - kHalfWidth;
	float maxX = position_.x + kHalfWidth;
	float minY = position_.y;
	float maxY = position_.y + height;

	float overlapX1 = maxX - box.min.x; // touching the left side of the box
	float overlapX2 = box.max.x - minX; // touching the right side of the box
	float overlapX = (std::min)(overlapX1, overlapX2);

	float overlapY1 = maxY - box.min.y; // touching the bottom of the box (ceiling)
	float overlapY2 = box.max.y - minY; // touching the top of the box (floor)
	float overlapY = (std::min)(overlapY1, overlapY2);

	if (overlapX <= 0.0f || overlapY <= 0.0f) return;

	bool isSteppingOnFloor = (overlapY2 < 0.08f) && (overlapY2 < overlapY1) && (velocity_.y <= 0.0f);

	if (isSteppingOnFloor || (overlapY < overlapX))
	{
		if (overlapY1 < overlapY2)
		{
			position_.y = box.min.y - height;
			if (velocity_.y > 0.0f)
			{
				velocity_.y = 0.0f;
			}
		}
		else
		{
			if (velocity_.y <= 0.0f)
			{
				position_.y = box.max.y;
				velocity_.y = 0.0f;
			}
		}
	}
	else
	{
		// Hit a wall: push out and turn around
		if (overlapX1 < overlapX2)
		{
			position_.x = box.min.x - kHalfWidth;
			if (moveDir_ > 0.0f) moveDir_ = -1.0f;
		}
		else
		{
			position_.x = box.max.x + kHalfWidth;
			if (moveDir_ < 0.0f) moveDir_ = 1.0f;
		}
	}

	SyncCollider();
}

void Enemy::SyncCollider()
{
	hurtBox_.SetCenterSize(
		position_ + Vector3(0.0f, kHalfHeight, 0.0f),
		Vector3(kHalfWidth, kHalfHeight, 0.4f)
	);
}

void Enemy::UpdateFlash(float deltaTime)
{
	// Texture carries the colors, so the base tint is white
	const Vector4 baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	const int baseLighting = 1;

	bool white = false;
	if (state_ == EnemyState::Launched)
	{
		// Blink texture <-> white after being hit
		flashTimer_ += deltaTime;
		white = (static_cast<int>(flashTimer_ / kFlashInterval) % 2) == 0;
	}

	if (white)
	{
		obj_.GetMaterial()->color = { 4.0f, 4.0f, 4.0f, 1.0f };
		obj_.GetMaterial()->enableLighting = 0;
	}
	else
	{
		obj_.GetMaterial()->color = baseColor;
		obj_.GetMaterial()->enableLighting = baseLighting;
		obj_.GetMaterial()->lightingType = 2; // Half-Lambert paper diffuse
	}
}
