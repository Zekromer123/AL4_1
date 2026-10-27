#pragma once
#include "3d/CollisionManager.h"
#include "3d/Object3d.h"
#include "3d/Collider.h"
#include "3d/Model.h"
#include "math/Vector3.h"

class Ball;
class ParticleSystem;

enum class PlayerState 
{
	idle,
	running,
	jumping,
	tossing,
	hitting,
	damaged,
	dead
};

class Player : public ICollidable
{
public:
	void Initialize(Model* slimeModel, Camera* camera);
	void Initialize(Model* slimeModel, Model* cubeModel, Camera* camera) { (void)cubeModel; Initialize(slimeModel, camera); }
	void Update(float deltaTime, Ball* ball);
	void Draw();

	//Collision
	ColliderShape GetColliderShape() const override { return ColliderShape::AABB; }
	const AABBCollider& GetAABBCollider() const override { return hurtBox_; }
	void OnCollision(ICollidable* other) override;

	//Player Status
	PlayerState GetState() const { return state_; }
	void SetHoldBall();
	const Vector3& GetPosition() const { return position_; }
	void SetPosition(const Vector3& pos);
	bool IsFacingRight() const { return isFacingRight_; }

	//HP / damage
	int GetHP() const { return hp_; }
	int GetMaxHP() const { return maxHP_; }
	bool IsDead() const { return hp_ <= 0; }
	bool IsInvincible() const { return invincibleTimer_ > 0.0f; }
	void TakeDamage(int damage, float sourceX);

	// Punch HitBox (Slime Punch System)
	ICollidable* GetPunchHitBox() { return &punchHitbox_; }
	bool IsPunchHitBoxActive() const { return punchHitbox_.isActive; }

	// Engine & caller compatibility aliases
	ICollidable* GetBatHitBox() { return GetPunchHitBox(); }
	bool IsBatHitBoxActive() const { return IsPunchHitBoxActive(); }

	void SetParticleSystem(ParticleSystem* ps) { particleSystem_ = ps; }
	ParticleSystem* GetParticleSystem() const { return particleSystem_; }


private:
	Vector3 position_ = { 0.0f, 0.0f, 0.0f };
	Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };
	bool isGrounded_ = false;
	bool isFacingRight_ = true;

	//Player
	ParticleSystem* particleSystem_ = nullptr;

	// Slime visual (procedural squash & stretch, no skeletal animation)
	void UpdateSlimeVisual(float deltaTime, bool grounded); // idle / run / punch / damage pose
	void ApplySlimeTransform();                             // write pose to playerObj_.worldTransform
	float animTime_ = 0.0f;           // clock for idle breathing / run bobbing
	float damageAnimTimer_ = 99.0f;   // seconds since the last hit (drives the bounce)
	float poseSY_ = 1.0f;             // smoothed vertical scale multiplier
	float poseLean_ = 0.0f;           // smoothed forward lean (radians, + = toward facing direction)
	float visualYaw_ = 0.0f;          // smoothed turn when changing direction
	float finalSY_ = 1.0f;            // final vertical multiplier (pose + oscillation + damage bounce)
	float finalLean_ = 0.0f;          // final roll (lean + damage wobble)
	Vector3 visualOffset_ = { 0.0f, 0.0f, 0.0f }; // visual-only offset (punch jump)
	float footstepDustTimer_ = 0.0f;
	Object3d playerObj_;


	PlayerState state_ = PlayerState::idle;
	float swingTimer_ = 0.0f;
	float tossCooldown_ = 0.0f;

	//HP / damage
	int maxHP_ = 10;
	int hp_ = 10;
	float invincibleTimer_ = 0.0f; // also drives the white blink
	float knockbackTimer_ = 0.0f;  // while > 0, input does not override velocity.x

	AABBCollider hurtBox_{};

	class PunchHitBox : public ICollidable
	{
	public:
		Player* player = nullptr;
		Ball* ball = nullptr;
		AABBCollider collider{};
		bool isActive = false;

		ColliderShape GetColliderShape() const override { return ColliderShape::AABB; }
		const AABBCollider& GetAABBCollider() const override { return collider; }
		void OnCollision(ICollidable* other) override; // Collision with ball
	} punchHitbox_;

};
