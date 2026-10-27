#pragma once
#include <RizeEngine.h>
#include "3d/CollisionManager.h"
#include "3d/Collider.h"
#include "3d/Object3d.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include "audio/Audio.h"

class ParticleSystem;

class Ball : public ICollidable
{
public:
	~Ball() override = default;
	void Initialize(Model* cubeModel, Camera* camera);
	void Toss(const Vector3& originPos);
	void GotHit(const Vector3& hitDirection);//Hit by bat
	void Despawn();
	void Update(float deltaTime);
	void Draw();

	//Audio
	void SetBounceSound(const Audio::SoundData& sound) { bounceSound_ = sound; hasBounceSound_ = true; }
	void PlayBounceSound();

	//Collision
	ColliderShape GetColliderShape() const override { return ColliderShape::Sphere; }
	const SphereCollider& GetSphereCollider() const override { return sphereCollider_; }
	void OnCollision(ICollidable* other) override;

	//Getter and Setter
	const Vector3& GetPosition() const { return position_; }
	const Vector3& GetVelocity() const { return velocity_; }
	bool isAlive() const { return isAlive_; }
	int GetComboCount() const { return comboCount_; }
	bool IsSmashHit() const { return isSmashHit_; }
	bool IsRicocheting() const { return isRicocheting_; }
	int GetRicochetCount() const { return ricochetCount_; }

	void SetParticleSystem(ParticleSystem* ps) { particleSystem_ = ps; }
	ParticleSystem* GetParticleSystem() const { return particleSystem_; }

private:
	Vector3 position_ = { 0.0f, 0.0f, 0.0f };
	Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };
	float radius_ = 0.25f;

	float baseSpeed_ = 10.0f;
	float speedPerCombo_ = 2.0f;
	float maxSpeed_ = 35.0f;
	float gravity_ = -9.0f;
	
	bool isAlive_ = false;
	bool isSmashHit_ = false;
	bool isRicocheting_ = false;
	int  ricochetCount_ = 0;
	int  maxRicochetCount_ = 6;
	float bounceCooldown_ = 0.0f; // Debounce to prevent multi-tile boundary overcounting

	int  comboCount_ = 0;

	SphereCollider sphereCollider_{};//Collision sphere collider for the ball

	ParticleSystem* particleSystem_ = nullptr;
	Object3d ballDebug;

	Audio::SoundData bounceSound_{};
	bool hasBounceSound_ = false;
};
