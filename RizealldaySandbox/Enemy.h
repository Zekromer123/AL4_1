#pragma once
#include <RizeEngine.h>
#include "3d/CollisionManager.h"
#include "3d/Collider.h"
#include "3d/Object3d.h"
#include "math/Vector3.h"
#include "math/Vector4.h"

class ParticleSystem;

enum class EnemyState
{
	Walking,  // patrolling, can hurt the player and can be hit by the ball
	Launched, // hit by the ball: knocked out of the arena (no terrain collision), blinking red-white
	Dead      // finished, to be removed by GameManager
};

class Enemy : public ICollidable
{
public:
	~Enemy() override = default;

	// groundModel = daruma, flyingModel = chochin lantern
	void Initialize(Model* groundModel, Model* flyingModel, Camera* camera, const Vector3& spawnPos, bool isFlying = false);
	void Update(float deltaTime);
	void Draw();

	//Collision (AABB)
	ColliderShape GetColliderShape() const override { return ColliderShape::AABB; }
	const AABBCollider& GetAABBCollider() const override { return hurtBox_; }
	void OnCollision(ICollidable* other) override;

	void SetParticleSystem(ParticleSystem* ps) { particleSystem_ = ps; }
	ParticleSystem* GetParticleSystem() const { return particleSystem_; }

	bool IsFlying() const { return isFlying_; }
	void SetIsFlying(bool isFlying) { isFlying_ = isFlying; }

	//Status
	// Only register the enemy to CollisionManager while this is true
	bool IsCollidable() const { return state_ == EnemyState::Walking; }
	bool IsDead() const { return state_ == EnemyState::Dead; }
	EnemyState GetState() const { return state_; }
	const Vector3& GetPosition() const { return position_; }

	void GotHit(const Vector3& ballPos, const Vector3& impactVelocity = Vector3(0.0f, 0.0f, 0.0f));
	void ResolveTerrain(const AABBCollider& box); // stand on ground / turn around at walls
	void SyncCollider();
	void UpdateFlash(float deltaTime);

	// Size (half extents). position_ is the FEET (bottom center), same as Player
	static constexpr float kHalfWidth = 0.3f;
	static constexpr float kHalfHeight = 0.4f;

	// Movement
	static constexpr float kWalkSpeed = 1.5f;
	static constexpr float kPatrolRange = 0.6f; // patrol +- from spawn X
	static constexpr float kGravity = -20.0f;
	static constexpr float kMaxFallSpeed = 30.0f;

	// Knockback (Smash Bros style launch)
	static constexpr float kKnockbackX = 3.0f;
	static constexpr float kKnockbackY = 7.0f;
	static constexpr float kKnockbackZ = -12.0f;
	static constexpr float kLaunchDuration = 1.0f; // seconds until removed
	static constexpr float kKillY = -8.0f;         // removed if below this
	static constexpr float kFlashInterval = 0.06f; // red-white blink speed

	Vector3 position_ = { 0.0f, 0.0f, 0.0f };
	Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };
	float moveDir_ = -1.0f; // -1 = left, +1 = right
	float patrolMinX_ = 0.0f;
	float patrolMaxX_ = 0.0f;

	EnemyState state_ = EnemyState::Walking;
	float launchTimer_ = 0.0f;
	float flashTimer_ = 0.0f;

	AABBCollider hurtBox_{};
	ParticleSystem* particleSystem_ = nullptr;
	Object3d obj_;

	// Flying Enemy
	bool isFlying_ = false;
	float spawnY_ = 0.0f;
	float flyTimer_ = 0.0f;
	float flySpeed_ = 2.2f;
	float flyAmplitude_ = 0.75f;

	// Idle animation: daruma rocks side to side, lantern sways
	float wobbleTimer_ = 0.0f;
};
