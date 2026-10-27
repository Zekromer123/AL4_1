#include "Ball.h"
#include "MapChip.h"
#include "ParticleSystem.h"
#include <algorithm>
#include <cstdlib>

void Ball::PlayBounceSound()
{
	if (hasBounceSound_ && bounceSound_.pBuffer)
	{
		// Variasi pitch kecil (±6%) agar pantulan beruntun tidak terdengar identik
		float pitch = 0.94f + 0.12f * (static_cast<float>(std::rand()) / RAND_MAX);
		Audio::GetInstance()->SoundPlayWave(bounceSound_, 0.85f, pitch);
	}
}

void Ball::Initialize(Model* cubeModel, Camera* camera)
{
	position_ = { 0.0f, 0.0f, 0.0f };
	velocity_ = { 0.0f, 0.0f, 0.0f };
	isAlive_ = false;
	isSmashHit_ = false;
	isRicocheting_ = false;
	ricochetCount_ = 0;
	comboCount_ = 0;
	bounceCooldown_ = 0.0f;

	sphereCollider_.center = position_;
	sphereCollider_.radius = radius_;
	sphereCollider_.attribute = CollisionAttribute::kBall;
	sphereCollider_.mask = CollisionAttribute::kBat | CollisionAttribute::kGround | CollisionAttribute::kObstacle;

	ballDebug.Initialize();
	ballDebug.SetModel(cubeModel);
	ballDebug.SetCamera(camera);
	ballDebug.worldTransform.scale = { 0.25f, 0.25f, 0.25f };
	ballDebug.GetMaterial()->color = { 0.86f, 0.28f, 0.18f, 1.0f }; // Vermilion ink (hanko red accent)
	ballDebug.GetMaterial()->enableLighting = 1;
	ballDebug.GetMaterial()->lightingType = 2; // Half-Lambert paper lighting
}

void Ball::Toss(const Vector3& originPos)
{
	isAlive_ = true;
	sphereCollider_.enabled = true;
	sphereCollider_.radius = radius_;
	sphereCollider_.mask = CollisionAttribute::kBat | CollisionAttribute::kGround | CollisionAttribute::kObstacle; 
	position_ = originPos;
	sphereCollider_.center = position_;
	velocity_ = { 0.0f, 4.0f, 0.0f }; // Toss upward
	comboCount_ = 0;
	isSmashHit_ = false;
	isRicocheting_ = false;
	ricochetCount_ = 0;
	bounceCooldown_ = 0.0f;
}

void Ball::GotHit(const Vector3& hitDirection)
{
	if (!isAlive_)
	{
		return;
	}

	bool isPerfectTiming = (!isSmashHit_ && std::abs(velocity_.y) < 1.5f);

	// NOTE: combo is now counted when the ball hits an enemy (see OnCollision), not on bat hit

	float currentSpeed = baseSpeed_ + (comboCount_ * speedPerCombo_);
	if (isPerfectTiming)
	{
		currentSpeed *= 1.35f; // Increase speed for perfect timing
	}
	currentSpeed = (std::min)(currentSpeed, maxSpeed_); // Clamp to max speed

	velocity_ = hitDirection.Normalize() * currentSpeed;
	isSmashHit_ = true; // Mark that the ball was hit with a bat
	isRicocheting_ = true; 
	ricochetCount_ = 0;
	bounceCooldown_ = 0.0f;
	sphereCollider_.mask |= CollisionAttribute::kEnemy; 

	// Play bounce / hit sound when player hits the ball
	PlayBounceSound();
}

void Ball::Despawn()
{
	isAlive_ = false;
	isSmashHit_ = false;
	isRicocheting_ = false;
	ricochetCount_ = 0;
	bounceCooldown_ = 0.0f;
	velocity_ = { 0.0f, 0.0f, 0.0f };
	comboCount_ = 0;
	sphereCollider_.enabled = false;
	sphereCollider_.mask = CollisionAttribute::kBat | CollisionAttribute::kGround | CollisionAttribute::kObstacle;
}

void Ball::Update(float deltaTime)
{
	if (!isAlive_) return;

	if (bounceCooldown_ > 0.0f) {
		bounceCooldown_ -= deltaTime;
	}

	if (isRicocheting_)
	{
		// Dalam mode ricochet: 0 gravitasi, proyektil melesat lurus
	}
	else
	{
		// Pasca fase ricochet: gravitasi lembut melengkungkan lintasan bola
		velocity_.y += gravity_ * deltaTime;

		const float maxFallSpeed = -20.0f;
		if (velocity_.y < maxFallSpeed) {
			velocity_.y = maxFallSpeed;
		}

		// Perlambatan gesekan tanah yang halus agar bola tetap menggelinding dan tidak mendadak berhenti beku
		if (std::abs(velocity_.y) < 0.2f) {
			float groundFriction = std::pow(0.995f, deltaTime * 60.0f);
			velocity_.x *= groundFriction;
			// Pertahankan kecepatan rolling minimum agar bola tetap aktif bergerak dan bisa disusul/dipukul kembali
			if (std::abs(velocity_.x) < 1.5f && std::abs(velocity_.x) > 0.01f) {
				velocity_.x = (velocity_.x > 0.0f) ? 1.5f : -1.5f;
			}
		}
		else {
			float airDrag = std::pow(0.998f, deltaTime * 60.0f);
			velocity_.x *= airDrag;
		}
	}

	// Update position based on velocity
	position_ += velocity_ * deltaTime;

	// Update collider position
	sphereCollider_.center = position_;

	ballDebug.worldTransform.translation = position_;
	ballDebug.Update(deltaTime);

	// Check if the ball has fallen below a certain threshold (e.g., ground level)
	if (position_.y < -5.0f) {
		Despawn();
	}
}

void Ball::Draw()
{
	if (!isAlive_) return;

	auto* cmdList = RizeEngine::GetDirectXCommon()->GetCommandList();
	ballDebug.Draw(cmdList);
}

void Ball::OnCollision(ICollidable* other)
{
	if (!isAlive_ || !other)
	{
		return;
	}

	uint32_t targetAttribute = 0;
	if (other->GetColliderShape() == ColliderShape::AABB)
	{
		targetAttribute = other->GetAABBCollider().attribute;
	}
	else if (other->GetColliderShape() == ColliderShape::Sphere)
	{
		targetAttribute = other->GetSphereCollider().attribute;
	}

	if ((targetAttribute & CollisionAttribute::kGround) || (targetAttribute & CollisionAttribute::kObstacle))
	{
		TileBlock* tileBlock = dynamic_cast<TileBlock*>(other);
		if (tileBlock && tileBlock->isDestructible && !tileBlock->isDestroyed)
		{
			tileBlock->isDestroyed = true;
			tileBlock->aabb.enabled = false;
			Vector3 blockCenter = (tileBlock->aabb.min + tileBlock->aabb.max) * 0.5f;
			if (particleSystem_)
			{
				particleSystem_->EmitEnemySparks(blockCenter, velocity_);
			}
		}

		if (tileBlock && (tileBlock->slopeType == 1 || tileBlock->slopeType == 2))
		{
			// Slanted block 45-degree reflection
			// slopeType 1 = Right (/), normal faces (-0.70710678f, 0.70710678f, 0.0f)
			// slopeType 2 = Left (\), normal faces (0.70710678f, 0.70710678f, 0.0f)
			Vector3 normal = (tileBlock->slopeType == 1)
				? Vector3(-0.70710678f, 0.70710678f, 0.0f)
				: Vector3(0.70710678f, 0.70710678f, 0.0f);

			float dot = velocity_.x * normal.x + velocity_.y * normal.y + velocity_.z * normal.z;
			if (dot > 0.0f)
			{
				normal = normal * -1.0f;
				dot = -dot;
			}

			Vector3 reflected = velocity_ - normal * (2.0f * dot);
			float restitution = isRicocheting_ ? 0.98f : 0.85f;
			velocity_ = reflected * restitution;

			if (std::abs(velocity_.y) < 2.5f && normal.y > 0.0f)
			{
				velocity_.y = 3.0f;
			}

			position_ += normal * 0.12f;

			if (bounceCooldown_ <= 0.0f)
			{
				ricochetCount_++;
				bounceCooldown_ = 0.08f;
				if (particleSystem_)
				{
					Vector3 contactPoint = position_ - normal * radius_;
					particleSystem_->EmitBallBounce(contactPoint, normal);
				}
				PlayBounceSound();
				if (ricochetCount_ >= maxRicochetCount_)
				{
					isRicocheting_ = false;
				}
			}

			sphereCollider_.center = position_;
			ballDebug.worldTransform.translation = position_;
			return; // Skip standard AABB resolution
		}

		const AABBCollider& box = other->GetAABBCollider();

		Vector3 closest = ClosestPoint(position_, box);
		Vector3 diff = position_ - closest;
		diff.z = 0.0f;

		Vector3 normal = { 0.0f, 0.0f, 0.0f };
		float diffLen = diff.Length();

		if (diffLen > 1e-4f) {
			if (std::abs(diff.x) > std::abs(diff.y)) {
				normal.x = (diff.x > 0.0f) ? 1.0f : -1.0f;
			}
			else {
				normal.y = (diff.y > 0.0f) ? 1.0f : -1.0f;
			}
		}
		else {
			float penX = (velocity_.x > 0.0f) ? (position_.x + radius_ - box.min.x) : (box.max.x - (position_.x - radius_));
			float penY = (velocity_.y > 0.0f) ? (position_.y + radius_ - box.min.y) : (box.max.y - (position_.y - radius_));

			if (std::abs(velocity_.x) > 0.01f && penX < penY) {
				normal.x = (velocity_.x > 0.0f) ? -1.0f : 1.0f;
			}
			else if (std::abs(velocity_.y) > 0.01f) {
				normal.y = (velocity_.y > 0.0f) ? -1.0f : 1.0f;
			}
			else {
				float distLeft = position_.x - box.min.x;
				float distRight = box.max.x - position_.x;
				float distBottom = position_.y - box.min.y;
				float distTop = box.max.y - position_.y;

				float minDist = (std::min)({ distLeft, distRight, distBottom, distTop });
				if (minDist == distLeft)        normal = { -1.0f,  0.0f, 0.0f };
				else if (minDist == distRight)  normal = { 1.0f,  0.0f, 0.0f };
				else if (minDist == distTop)    normal = { 0.0f,  1.0f, 0.0f };
				else                            normal = { 0.0f, -1.0f, 0.0f };
			}
		}

		if (isRicocheting_)
		{
			const float ricochetRestitution = 0.98f;
			bool didBounce = false;

			if (normal.x != 0.0f) {
				if (normal.x > 0.0f) {
					position_.x = box.max.x + radius_;
					if (velocity_.x < 0.0f) {
						velocity_.x = -velocity_.x * ricochetRestitution;
						didBounce = true;
					}
				}
				else {
					position_.x = box.min.x - radius_;
					if (velocity_.x > 0.0f) {
						velocity_.x = -velocity_.x * ricochetRestitution;
						didBounce = true;
					}
				}
			}

			if (normal.y != 0.0f) {
				if (normal.y > 0.0f) {
					position_.y = box.max.y + radius_;
					if (velocity_.y < 0.0f) {
						velocity_.y = -velocity_.y * ricochetRestitution;
						didBounce = true;
					}
					if (std::abs(velocity_.y) < 2.5f) {
						velocity_.y = 3.0f;
					}
				}
				else {
					position_.y = box.min.y - radius_;
					if (velocity_.y > 0.0f) {
						velocity_.y = -velocity_.y * ricochetRestitution;
						didBounce = true;
					}
					if (std::abs(velocity_.y) < 2.5f) {
						velocity_.y = -3.0f;
					}
				}
			}

			// Debounce pantulan agar seam/pertemuan antar tile blok tidak menghabiskan kuota ricochet dalam 1 frame
			if (didBounce && bounceCooldown_ <= 0.0f) {
				ricochetCount_++;
				bounceCooldown_ = 0.08f; // 80ms jeda hitung pantulan

				if (particleSystem_) {
					Vector3 contactPoint = position_ - normal * radius_;
					particleSystem_->EmitBallBounce(contactPoint, normal);
				}
				PlayBounceSound();

				if (ricochetCount_ >= maxRicochetCount_) {
					isRicocheting_ = false;
					// PENTING: isSmashHit_ TIDAK dimatikan! Kolisi dengan musuh tetap aktif sampai player recall / reset!
				}
			}
		}
		else
		{
			// Pasca ricochet: Pantulan dinamis yang tetap hidup & menggelinding (TIDAK mendadak berhenti/mengerem mati)
			const float restitution = 0.82f;
			bool didBounce = false;

			if (normal.x != 0.0f) {
				if (normal.x > 0.0f) {
					position_.x = box.max.x + radius_;
					if (velocity_.x < 0.0f) {
						velocity_.x = -velocity_.x * restitution;
						if (std::abs(velocity_.x) > 1.2f) didBounce = true;
					}
				}
				else {
					position_.x = box.min.x - radius_;
					if (velocity_.x > 0.0f) {
						velocity_.x = -velocity_.x * restitution;
						if (std::abs(velocity_.x) > 1.2f) didBounce = true;
					}
				}
			}

			if (normal.y != 0.0f) {
				if (normal.y > 0.0f) {
					position_.y = box.max.y + radius_;
					if (velocity_.y < 0.0f) {
						velocity_.y = -velocity_.y * restitution;
						if (std::abs(velocity_.y) > 1.2f) didBounce = true;
					}
					// Jika getaran vertikal sudah sangat kecil, biarkan menggelinding rata di tanah
					if (std::abs(velocity_.y) < 0.35f) {
						velocity_.y = 0.0f;
					}
					// Sedikit redaman horizontal per pantulan, tidak drastis
					velocity_.x *= 0.96f;
				}
				else {
					position_.y = box.min.y - radius_;
					if (velocity_.y > 0.0f) {
						velocity_.y = -velocity_.y * restitution;
						if (std::abs(velocity_.y) > 1.2f) didBounce = true;
					}
				}
			}
			if (didBounce && bounceCooldown_ <= 0.0f) {
				bounceCooldown_ = 0.10f;
				if (particleSystem_) {
					Vector3 contactPoint = position_ - normal * radius_;
					particleSystem_->EmitBallBounce(contactPoint, normal);
				}
				PlayBounceSound();
			}
		}

		sphereCollider_.center = position_;
		ballDebug.worldTransform.translation = position_;
	}
	else if (targetAttribute & CollisionAttribute::kEnemy)
	{
		comboCount_++; // enemy hit -> combo up
		
		float currentSpeed = (std::min)(baseSpeed_ + (comboCount_ * speedPerCombo_), maxSpeed_);
		if (velocity_.Length() > 1e-3f) {
			velocity_ = velocity_.Normalize() * currentSpeed;
		}

		// Sesuai permintaan: Menabrak musuh menyegarkan mode ricochet (terus melaju kencang & belum terefek gravitasi)
		isRicocheting_ = true;
		ricochetCount_ = 0;
		bounceCooldown_ = 0.05f;
		PlayBounceSound();
	}
}
