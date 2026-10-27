#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <array>
#include <cstdint>
#include "3d/Camera.h"
#include "3d/Object3d.h"
#include "base/BlendMode.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include "math/Matrix4x4.h"
#include "math/VertexData.h"

struct Particle {
    Vector3 position = { 0.0f, 0.0f, -0.1f };
    Vector3 velocity = { 0.0f, 0.0f, 0.0f };
    Vector3 acceleration = { 0.0f, 0.0f, 0.0f };
    Vector4 startColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    Vector4 endColor = { 1.0f, 1.0f, 1.0f, 0.0f };
    Vector4 currentColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    float startScale = 0.2f;
    float endScale = 0.4f;
    float currentScale = 0.2f;
    float lifetime = 0.5f;
    float currentAge = 0.0f;
    float rotation = 0.0f;
    float rotationSpeed = 0.0f;
    float bounceElasticity = 0.0f;
    float groundY = -999.0f;
    BlendMode blendMode = BlendMode::AlphaBlend;
    uint32_t textureHandle = 0;
    bool isActive = false;
};

class ParticleSystem {
public:
    static constexpr size_t kMaxParticles = 512;

    ParticleSystem() = default;
    ~ParticleSystem();

    void Initialize(Camera* camera);
    void Update(float deltaTime);
    void Draw(ID3D12GraphicsCommandList* cmdList);

    // Emitter methods
    void EmitFootstepDust(const Vector3& feetPos, bool isFacingRight);
    void EmitPunchHitBurst(const Vector3& contactPoint);
    void EmitBatHitBurst(const Vector3& contactPoint) { EmitPunchHitBurst(contactPoint); }
    void EmitEnemySparks(const Vector3& impactPoint, const Vector3& impactVelocity);
    void EmitImpactFlash(const Vector3& pos);
    void EmitBallBounce(const Vector3& contactPoint, const Vector3& normal = Vector3(0.0f, 1.0f, 0.0f));
    void EmitBallRecallBurst(const Vector3& ballPos);
    void EmitPlayerRecallAbsorb(const Vector3& playerPos);
    void EmitPlayerRecallAbsorb(const Vector3& playerPos, const Vector3& ballPos);

    // Diagnostics & Utilities
    size_t GetActiveParticleCount() const;
    void Clear();

    uint32_t GetHitEffectTextureHandle() const { return hitEffectTextureHandle_; }

private:
    size_t AllocateParticle();
    void CreateMeshBuffers();
    void CreateConstantBuffers();
    static float RandomFloat(float min, float max);

    Camera* camera_ = nullptr;
    uint32_t circleTextureHandle_ = 0;
    uint32_t hitEffectTextureHandle_ = 0;

    // Fixed pool of 512 particles
    std::array<Particle, kMaxParticles> particles_{};
    size_t nextFreeIndex_ = 0;

    // D3D12 Quad Mesh Buffers
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vbView_{};
    D3D12_INDEX_BUFFER_VIEW ibView_{};

    // 256-byte aligned per-particle constant buffers
    static constexpr size_t kConstantBufferAlignment = 256;
    Microsoft::WRL::ComPtr<ID3D12Resource> materialBuffer_;
    uint8_t* mappedMaterial_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> transformBuffer_;
    uint8_t* mappedTransform_ = nullptr;

    // Pipeline binder (Object3d binds the engine's GraphicPipeline PSO & RootSignature per blend mode)
    Object3d pipelineBinder_;

    // Preallocated render bucket indices
    std::array<uint16_t, kMaxParticles> alphaCircleIndices_{};
    size_t alphaCircleCount_ = 0;
    std::array<uint16_t, kMaxParticles> alphaHitIndices_{};
    size_t alphaHitCount_ = 0;
    std::array<uint16_t, kMaxParticles> addCircleIndices_{};
    size_t addCircleCount_ = 0;
    std::array<uint16_t, kMaxParticles> addHitIndices_{};
    size_t addHitCount_ = 0;
};
