#include "ParticleSystem.h"
#include "base/RizeEngine.h"
#include "base/TextureManager.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <random>
#include <algorithm>

namespace {

struct GPUTransformMatrix {
    Matrix4x4 wvp;
    Matrix4x4 world;
};

Matrix4x4 MultiplyMatrix(const Matrix4x4& m1, const Matrix4x4& m2) {
    Matrix4x4 res{};
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            res.m[r][c] = m1.m[r][0] * m2.m[0][c] +
                          m1.m[r][1] * m2.m[1][c] +
                          m1.m[r][2] * m2.m[2][c] +
                          m1.m[r][3] * m2.m[3][c];
        }
    }
    return res;
}

Matrix4x4 MakeQuadWorldMatrix(const Vector3& pos, float scale, float rotZ) {
    float c = std::cos(rotZ);
    float s = std::sin(rotZ);
    Matrix4x4 m{};
    m.m[0][0] = c * scale;
    m.m[0][1] = s * scale;
    m.m[0][2] = 0.0f;
    m.m[0][3] = 0.0f;

    m.m[1][0] = -s * scale;
    m.m[1][1] = c * scale;
    m.m[1][2] = 0.0f;
    m.m[1][3] = 0.0f;

    m.m[2][0] = 0.0f;
    m.m[2][1] = 0.0f;
    m.m[2][2] = 1.0f;
    m.m[2][3] = 0.0f;

    m.m[3][0] = pos.x;
    m.m[3][1] = pos.y;
    m.m[3][2] = pos.z;
    m.m[3][3] = 1.0f;
    return m;
}

} // anonymous namespace

ParticleSystem::~ParticleSystem() {
    if (materialBuffer_ && mappedMaterial_) {
        materialBuffer_->Unmap(0, nullptr);
        mappedMaterial_ = nullptr;
    }
    if (transformBuffer_ && mappedTransform_) {
        transformBuffer_->Unmap(0, nullptr);
        mappedTransform_ = nullptr;
    }
}

void ParticleSystem::Initialize(Camera* camera) {
    camera_ = camera;

    // 1. Load circle texture and hit effect texture
    circleTextureHandle_ = TextureManager::GetInstance()->LoadTexture("Resources/circle.png");
    hitEffectTextureHandle_ = TextureManager::GetInstance()->LoadTexture("Resources/UI/hitEffect.png");

    // 2. Initialize pipeline binder (Object3d binds engine GraphicPipeline PSO & RootSignature)
    pipelineBinder_.Initialize();

    // 3. Create GPU quad vertex and index buffers
    CreateMeshBuffers();

    // 4. Create 256-byte aligned per-particle constant buffers (Material & TransformationMatrix)
    CreateConstantBuffers();

    Clear();
}

void ParticleSystem::CreateMeshBuffers() {
    DirectXCommon* dxCommon = RizeEngine::GetDirectXCommon();
    ID3D12Device* device = dxCommon->GetDevice();

    // Quad in XY plane facing -Z (towards camera at Z = -12.0f looking +Z)
    VertexData vertices[4] = {};
    // Top-Left
    vertices[0].position = { -0.5f, 0.5f, 0.0f, 1.0f };
    vertices[0].texcoord = { 0.0f, 0.0f };
    vertices[0].normal   = { 0.0f, 0.0f, -1.0f };
    // Top-Right
    vertices[1].position = { 0.5f, 0.5f, 0.0f, 1.0f };
    vertices[1].texcoord = { 1.0f, 0.0f };
    vertices[1].normal   = { 0.0f, 0.0f, -1.0f };
    // Bottom-Left
    vertices[2].position = { -0.5f, -0.5f, 0.0f, 1.0f };
    vertices[2].texcoord = { 0.0f, 1.0f };
    vertices[2].normal   = { 0.0f, 0.0f, -1.0f };
    // Bottom-Right
    vertices[3].position = { 0.5f, -0.5f, 0.0f, 1.0f };
    vertices[3].texcoord = { 1.0f, 1.0f };
    vertices[3].normal   = { 0.0f, 0.0f, -1.0f };

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC vbDesc{};
    vbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    vbDesc.Width = sizeof(vertices);
    vbDesc.Height = 1;
    vbDesc.DepthOrArraySize = 1;
    vbDesc.MipLevels = 1;
    vbDesc.SampleDesc.Count = 1;
    vbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &vbDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&vertexBuffer_));
    assert(SUCCEEDED(hr));

    void* mappedVB = nullptr;
    vertexBuffer_->Map(0, nullptr, &mappedVB);
    std::memcpy(mappedVB, vertices, sizeof(vertices));
    vertexBuffer_->Unmap(0, nullptr);

    vbView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vbView_.SizeInBytes = sizeof(vertices);
    vbView_.StrideInBytes = sizeof(VertexData);

    // 6 indices for 2 triangles (Clockwise winding facing camera)
    uint32_t indices[6] = { 0, 1, 2, 2, 1, 3 };

    D3D12_RESOURCE_DESC ibDesc{};
    ibDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    ibDesc.Width = sizeof(indices);
    ibDesc.Height = 1;
    ibDesc.DepthOrArraySize = 1;
    ibDesc.MipLevels = 1;
    ibDesc.SampleDesc.Count = 1;
    ibDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &ibDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&indexBuffer_));
    assert(SUCCEEDED(hr));

    void* mappedIB = nullptr;
    indexBuffer_->Map(0, nullptr, &mappedIB);
    std::memcpy(mappedIB, indices, sizeof(indices));
    indexBuffer_->Unmap(0, nullptr);

    ibView_.BufferLocation = indexBuffer_->GetGPUVirtualAddress();
    ibView_.SizeInBytes = sizeof(indices);
    ibView_.Format = DXGI_FORMAT_R32_UINT;
}

void ParticleSystem::CreateConstantBuffers() {
    DirectXCommon* dxCommon = RizeEngine::GetDirectXCommon();
    ID3D12Device* device = dxCommon->GetDevice();

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    // 1. Material Buffer (kMaxParticles * 256 bytes)
    D3D12_RESOURCE_DESC matDesc{};
    matDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    matDesc.Width = kMaxParticles * kConstantBufferAlignment;
    matDesc.Height = 1;
    matDesc.DepthOrArraySize = 1;
    matDesc.MipLevels = 1;
    matDesc.SampleDesc.Count = 1;
    matDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &matDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&materialBuffer_));
    assert(SUCCEEDED(hr));

    materialBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedMaterial_));

    // 2. Transform Buffer (kMaxParticles * 256 bytes)
    D3D12_RESOURCE_DESC transDesc{};
    transDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    transDesc.Width = kMaxParticles * kConstantBufferAlignment;
    transDesc.Height = 1;
    transDesc.DepthOrArraySize = 1;
    transDesc.MipLevels = 1;
    transDesc.SampleDesc.Count = 1;
    transDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &transDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&transformBuffer_));
    assert(SUCCEEDED(hr));

    transformBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedTransform_));
}

size_t ParticleSystem::AllocateParticle() {
    // O(1) rolling index search, oldest-particle eviction on saturation
    size_t targetIndex = nextFreeIndex_;
    for (size_t i = 0; i < kMaxParticles; ++i) {
        size_t idx = (nextFreeIndex_ + i) % kMaxParticles;
        if (!particles_[idx].isActive) {
            targetIndex = idx;
            break;
        }
    }
    // If saturated, targetIndex defaults to nextFreeIndex_ (evicting oldest particle)
    nextFreeIndex_ = (targetIndex + 1) % kMaxParticles;
    return targetIndex;
}

float ParticleSystem::RandomFloat(float min, float max) {
    static thread_local std::mt19937 gen(1337);
    std::uniform_real_distribution<float> dist(min, max);
    return dist(gen);
}

void ParticleSystem::Update(float deltaTime) {
    for (size_t i = 0; i < kMaxParticles; ++i) {
        Particle& p = particles_[i];
        if (!p.isActive) continue;

        p.currentAge += deltaTime;
        if (p.currentAge >= p.lifetime) {
            p.isActive = false;
            continue;
        }

        // Physics update
        p.velocity.x += p.acceleration.x * deltaTime;
        p.velocity.y += p.acceleration.y * deltaTime;
        p.velocity.z += p.acceleration.z * deltaTime;

        p.position.x += p.velocity.x * deltaTime;
        p.position.y += p.velocity.y * deltaTime;
        p.position.z = -0.1f; // maintain strictly on 2.5D visual layer

        p.rotation += p.rotationSpeed * deltaTime;

        // Ground bounce simulation
        if (p.bounceElasticity > 0.0f && p.position.y <= p.groundY && p.velocity.y < 0.0f) {
            p.position.y = p.groundY;
            p.velocity.y = -p.velocity.y * p.bounceElasticity;
            p.velocity.x *= 0.8f; // ground friction
        }

        // Interpolation
        float t = std::clamp(p.currentAge / p.lifetime, 0.0f, 1.0f);
        p.currentScale = p.startScale + t * (p.endScale - p.startScale);

        p.currentColor.x = p.startColor.x + t * (p.endColor.x - p.startColor.x);
        p.currentColor.y = p.startColor.y + t * (p.endColor.y - p.startColor.y);
        p.currentColor.z = p.startColor.z + t * (p.endColor.z - p.startColor.z);
        p.currentColor.w = p.startColor.w + t * (p.endColor.w - p.startColor.w);
    }
}

void ParticleSystem::Draw(ID3D12GraphicsCommandList* cmdList) {
    if (!cmdList) return;

    alphaCircleCount_ = 0;
    alphaHitCount_ = 0;
    addCircleCount_ = 0;
    addHitCount_ = 0;

    Matrix4x4 vp = camera_ ? camera_->GetViewProjectionMatrix() : Mathematics::MakeIdentity4x4();

    for (size_t i = 0; i < kMaxParticles; ++i) {
        const Particle& p = particles_[i];
        if (!p.isActive) continue;

        // 1. Material CBV (unlit, modulated by particle color)
        Material* mat = reinterpret_cast<Material*>(mappedMaterial_ + i * kConstantBufferAlignment);
        mat->color = p.currentColor;
        mat->enableLighting = 0;
        mat->lightingType = 0;
        mat->inkEdgeMask = 0;
        mat->padding = 0.0f;

        // 2. Transformation CBV (WVP and World)
        Matrix4x4 world = MakeQuadWorldMatrix(p.position, p.currentScale, p.rotation);
        Matrix4x4 wvp = MultiplyMatrix(world, vp);

        GPUTransformMatrix* trans = reinterpret_cast<GPUTransformMatrix*>(mappedTransform_ + i * kConstantBufferAlignment);
        trans->wvp = wvp;
        trans->world = world;

        if (p.blendMode == BlendMode::AlphaBlend) {
            if (p.textureHandle == hitEffectTextureHandle_) {
                alphaHitIndices_[alphaHitCount_++] = static_cast<uint16_t>(i);
            } else {
                alphaCircleIndices_[alphaCircleCount_++] = static_cast<uint16_t>(i);
            }
        } else {
            if (p.textureHandle == hitEffectTextureHandle_) {
                addHitIndices_[addHitCount_++] = static_cast<uint16_t>(i);
            } else {
                addCircleIndices_[addCircleCount_++] = static_cast<uint16_t>(i);
            }
        }
    }

    // Render AlphaBlend particles (Footstep smoke, Ink splatter droplets, Ink impact splashes)
    if (alphaCircleCount_ > 0 || alphaHitCount_ > 0) {
        pipelineBinder_.SetBlendMode(BlendMode::AlphaBlend);
        pipelineBinder_.Draw(cmdList);

        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmdList->IASetVertexBuffers(0, 1, &vbView_);
        cmdList->IASetIndexBuffer(&ibView_);

        // 1. Alpha circle particles (Footstep smoke, ink droplets)
        if (alphaCircleCount_ > 0) {
            cmdList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetGPUHandle(circleTextureHandle_));
            for (size_t k = 0; k < alphaCircleCount_; ++k) {
                uint16_t idx = alphaCircleIndices_[k];
                D3D12_GPU_VIRTUAL_ADDRESS matAddr = materialBuffer_->GetGPUVirtualAddress() + idx * kConstantBufferAlignment;
                D3D12_GPU_VIRTUAL_ADDRESS transAddr = transformBuffer_->GetGPUVirtualAddress() + idx * kConstantBufferAlignment;

                cmdList->SetGraphicsRootConstantBufferView(0, matAddr);
                cmdList->SetGraphicsRootConstantBufferView(1, transAddr);
                cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);
            }
        }

        // 2. Alpha hit effect particles (Ink impact splashes)
        if (alphaHitCount_ > 0) {
            cmdList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetGPUHandle(hitEffectTextureHandle_));
            for (size_t k = 0; k < alphaHitCount_; ++k) {
                uint16_t idx = alphaHitIndices_[k];
                D3D12_GPU_VIRTUAL_ADDRESS matAddr = materialBuffer_->GetGPUVirtualAddress() + idx * kConstantBufferAlignment;
                D3D12_GPU_VIRTUAL_ADDRESS transAddr = transformBuffer_->GetGPUVirtualAddress() + idx * kConstantBufferAlignment;

                cmdList->SetGraphicsRootConstantBufferView(0, matAddr);
                cmdList->SetGraphicsRootConstantBufferView(1, transAddr);
                cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);
            }
        }
    }

    // Render Additive particles (if any)
    if (addCircleCount_ > 0 || addHitCount_ > 0) {
        pipelineBinder_.SetBlendMode(BlendMode::Add);
        pipelineBinder_.Draw(cmdList);

        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmdList->IASetVertexBuffers(0, 1, &vbView_);
        cmdList->IASetIndexBuffer(&ibView_);

        // 1. Circle spark particles
        if (addCircleCount_ > 0) {
            cmdList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetGPUHandle(circleTextureHandle_));
            for (size_t k = 0; k < addCircleCount_; ++k) {
                uint16_t idx = addCircleIndices_[k];
                D3D12_GPU_VIRTUAL_ADDRESS matAddr = materialBuffer_->GetGPUVirtualAddress() + idx * kConstantBufferAlignment;
                D3D12_GPU_VIRTUAL_ADDRESS transAddr = transformBuffer_->GetGPUVirtualAddress() + idx * kConstantBufferAlignment;

                cmdList->SetGraphicsRootConstantBufferView(0, matAddr);
                cmdList->SetGraphicsRootConstantBufferView(1, transAddr);
                cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);
            }
        }

        // 2. Hit effect spiky impact flash particles
        if (addHitCount_ > 0) {
            cmdList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetGPUHandle(hitEffectTextureHandle_));
            for (size_t k = 0; k < addHitCount_; ++k) {
                uint16_t idx = addHitIndices_[k];
                D3D12_GPU_VIRTUAL_ADDRESS matAddr = materialBuffer_->GetGPUVirtualAddress() + idx * kConstantBufferAlignment;
                D3D12_GPU_VIRTUAL_ADDRESS transAddr = transformBuffer_->GetGPUVirtualAddress() + idx * kConstantBufferAlignment;

                cmdList->SetGraphicsRootConstantBufferView(0, matAddr);
                cmdList->SetGraphicsRootConstantBufferView(1, transAddr);
                cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);
            }
        }
    }
}

void ParticleSystem::EmitFootstepDust(const Vector3& feetPos, bool isFacingRight) {
    for (int i = 0; i < 2; ++i) {
        size_t idx = AllocateParticle();
        Particle& p = particles_[idx];
        p.isActive = true;
        p.blendMode = BlendMode::AlphaBlend;
        p.textureHandle = circleTextureHandle_;
        p.currentAge = 0.0f;
        p.lifetime = RandomFloat(0.42f, 0.52f); // ~0.5s

        float offsetX = RandomFloat(-0.06f, 0.06f);
        float offsetY = RandomFloat(0.0f, 0.04f);
        p.position = Vector3(feetPos.x + offsetX, feetPos.y + offsetY, -0.1f);

        // Drift backward relative to player facing direction + upward drift
        float driftX = isFacingRight ? RandomFloat(-0.55f, -0.15f) : RandomFloat(0.15f, 0.55f);
        float driftY = RandomFloat(0.35f, 0.65f); // upward drift
        p.velocity = Vector3(driftX, driftY, 0.0f);
        p.acceleration = Vector3(-driftX * 0.4f, -0.2f, 0.0f);

        // Scale 0.15f -> 0.40f
        p.startScale = RandomFloat(0.14f, 0.17f);
        p.endScale = RandomFloat(0.38f, 0.42f);
        p.currentScale = p.startScale;

        // Translucent soft grey/white smoke
        float grey = RandomFloat(0.85f, 0.95f);
        p.startColor = Vector4(grey, grey, grey, RandomFloat(0.55f, 0.70f));
        p.endColor = Vector4(grey, grey, grey, 0.0f); // alpha fade
        p.currentColor = p.startColor;

        p.rotation = RandomFloat(0.0f, 6.283185f);
        p.rotationSpeed = RandomFloat(-1.2f, 1.2f);
        p.bounceElasticity = 0.0f;
    }
}

void ParticleSystem::EmitImpactFlash(const Vector3& pos) {
    if (hitEffectTextureHandle_ == 0) return;

    // Layer 1: Primary spiky black ink impact splash
    {
        size_t idx = AllocateParticle();
        Particle& p = particles_[idx];
        p.isActive = true;
        p.blendMode = BlendMode::AlphaBlend;
        p.textureHandle = hitEffectTextureHandle_;
        p.currentAge = 0.0f;
        p.lifetime = 0.16f; // rapid impact flash

        p.position = Vector3(pos.x, pos.y, -0.1f);
        p.velocity = Vector3(0.0f, 0.0f, 0.0f);
        p.acceleration = Vector3(0.0f, 0.0f, 0.0f);

        p.startScale = 0.85f;
        p.endScale = 1.15f;
        p.currentScale = p.startScale;

        // Solid charcoal black ink fading out
        p.startColor = Vector4(0.04f, 0.04f, 0.04f, 0.95f);
        p.endColor = Vector4(0.04f, 0.04f, 0.04f, 0.0f);
        p.currentColor = p.startColor;

        p.rotation = RandomFloat(-0.2f, 0.2f);
        p.rotationSpeed = RandomFloat(-1.5f, 1.5f);
        p.bounceElasticity = 0.0f;
    }

    // Layer 2: Secondary offset rotated flash (~45 deg) for multi-spoke cross ink flare
    {
        size_t idx = AllocateParticle();
        Particle& p = particles_[idx];
        p.isActive = true;
        p.blendMode = BlendMode::AlphaBlend;
        p.textureHandle = hitEffectTextureHandle_;
        p.currentAge = 0.0f;
        p.lifetime = 0.13f;

        p.position = Vector3(pos.x, pos.y, -0.1f);
        p.velocity = Vector3(0.0f, 0.0f, 0.0f);
        p.acceleration = Vector3(0.0f, 0.0f, 0.0f);

        p.startScale = 0.65f;
        p.endScale = 0.90f;
        p.currentScale = p.startScale;

        // Soft charcoal black ink
        p.startColor = Vector4(0.08f, 0.08f, 0.08f, 0.85f);
        p.endColor = Vector4(0.08f, 0.08f, 0.08f, 0.0f);
        p.currentColor = p.startColor;

        p.rotation = 0.785398f + RandomFloat(-0.1f, 0.1f);
        p.rotationSpeed = RandomFloat(-2.0f, 2.0f);
        p.bounceElasticity = 0.0f;
    }
}

void ParticleSystem::EmitPunchHitBurst(const Vector3& contactPoint) {
    // 1. Dual-layer spiky black ink impact splash
    EmitImpactFlash(contactPoint);

    // 2. Radial black ink splatter droplets
    constexpr int kCount = 20;
    for (int i = 0; i < kCount; ++i) {
        size_t idx = AllocateParticle();
        Particle& p = particles_[idx];
        p.isActive = true;
        p.blendMode = BlendMode::AlphaBlend;
        p.textureHandle = circleTextureHandle_;
        p.currentAge = 0.0f;
        p.lifetime = RandomFloat(0.20f, 0.32f); // ~0.25s

        p.position = Vector3(contactPoint.x, contactPoint.y, -0.1f);

        // Radial distribution across 360 degrees
        float baseAngle = (i * 2.0f * 3.14159265f) / static_cast<float>(kCount);
        float angle = baseAngle + RandomFloat(-0.12f, 0.12f);
        float speed = RandomFloat(7.0f, 11.5f); // high initial speed

        p.velocity = Vector3(std::cos(angle) * speed, std::sin(angle) * speed, 0.0f);

        // Fast decay opposing velocity
        p.acceleration = Vector3(-p.velocity.x * 6.5f, -p.velocity.y * 6.5f, 0.0f);

        // Radial burst scale
        p.startScale = RandomFloat(0.24f, 0.32f);
        p.endScale = RandomFloat(0.06f, 0.10f);
        p.currentScale = p.startScale;

        // Black ink droplets
        float inkGrey = RandomFloat(0.02f, 0.08f);
        p.startColor = Vector4(inkGrey, inkGrey, inkGrey, RandomFloat(0.85f, 1.0f));
        p.endColor = Vector4(inkGrey, inkGrey, inkGrey, 0.0f); // rapid fade
        p.currentColor = p.startColor;

        p.rotation = RandomFloat(0.0f, 6.283185f);
        p.rotationSpeed = RandomFloat(-6.0f, 6.0f);
        p.bounceElasticity = 0.0f;
    }
}

void ParticleSystem::EmitBallBounce(const Vector3& contactPoint, const Vector3& normal) {
    if (hitEffectTextureHandle_ != 0) {
        // Layer 1: Compact spiky black ink impact flash at bounce point
        {
            size_t idx = AllocateParticle();
            Particle& p = particles_[idx];
            p.isActive = true;
            p.blendMode = BlendMode::AlphaBlend;
            p.textureHandle = hitEffectTextureHandle_;
            p.currentAge = 0.0f;
            p.lifetime = 0.14f;

            p.position = Vector3(contactPoint.x, contactPoint.y, -0.1f);
            p.velocity = Vector3(0.0f, 0.0f, 0.0f);
            p.acceleration = Vector3(0.0f, 0.0f, 0.0f);

            p.startScale = 0.65f;
            p.endScale = 0.90f;
            p.currentScale = p.startScale;

            p.startColor = Vector4(0.04f, 0.04f, 0.04f, 0.95f);
            p.endColor = Vector4(0.04f, 0.04f, 0.04f, 0.0f);
            p.currentColor = p.startColor;

            p.rotation = RandomFloat(-0.2f, 0.2f);
            p.rotationSpeed = RandomFloat(-1.5f, 1.5f);
            p.bounceElasticity = 0.0f;
        }

        // Layer 2: Offset rotated flare (~45 deg)
        {
            size_t idx = AllocateParticle();
            Particle& p = particles_[idx];
            p.isActive = true;
            p.blendMode = BlendMode::AlphaBlend;
            p.textureHandle = hitEffectTextureHandle_;
            p.currentAge = 0.0f;
            p.lifetime = 0.11f;

            p.position = Vector3(contactPoint.x, contactPoint.y, -0.1f);
            p.velocity = Vector3(0.0f, 0.0f, 0.0f);
            p.acceleration = Vector3(0.0f, 0.0f, 0.0f);

            p.startScale = 0.45f;
            p.endScale = 0.65f;
            p.currentScale = p.startScale;

            p.startColor = Vector4(0.08f, 0.08f, 0.08f, 0.80f);
            p.endColor = Vector4(0.08f, 0.08f, 0.08f, 0.0f);
            p.currentColor = p.startColor;

            p.rotation = 0.785398f + RandomFloat(-0.1f, 0.1f);
            p.rotationSpeed = RandomFloat(-2.0f, 2.0f);
            p.bounceElasticity = 0.0f;
        }
    }

    // Directional ink splatter droplets (spray outward from the surface)
    constexpr int kCount = 14;
    bool hasNormal = (normal.x != 0.0f || normal.y != 0.0f);
    float baseAngle = hasNormal ? std::atan2(normal.y, normal.x) : 0.0f;

    for (int i = 0; i < kCount; ++i) {
        size_t idx = AllocateParticle();
        Particle& p = particles_[idx];
        p.isActive = true;
        p.blendMode = BlendMode::AlphaBlend;
        p.textureHandle = circleTextureHandle_;
        p.currentAge = 0.0f;
        p.lifetime = RandomFloat(0.18f, 0.28f);

        p.position = Vector3(contactPoint.x, contactPoint.y, -0.1f);

        float angle = 0.0f;
        if (hasNormal) {
            // Spray outward within ±65 degrees of surface normal
            float offset = RandomFloat(-1.13f, 1.13f);
            angle = baseAngle + offset;
        } else {
            angle = (i * 2.0f * 3.14159265f) / static_cast<float>(kCount) + RandomFloat(-0.15f, 0.15f);
        }

        float speed = RandomFloat(5.0f, 9.5f);
        p.velocity = Vector3(std::cos(angle) * speed, std::sin(angle) * speed, 0.0f);
        p.acceleration = Vector3(-p.velocity.x * 6.0f, -p.velocity.y * 6.0f, 0.0f);

        p.startScale = RandomFloat(0.18f, 0.25f);
        p.endScale = RandomFloat(0.04f, 0.07f);
        p.currentScale = p.startScale;

        float inkGrey = RandomFloat(0.02f, 0.07f);
        p.startColor = Vector4(inkGrey, inkGrey, inkGrey, RandomFloat(0.85f, 1.0f));
        p.endColor = Vector4(inkGrey, inkGrey, inkGrey, 0.0f);
        p.currentColor = p.startColor;

        p.rotation = RandomFloat(0.0f, 6.283185f);
        p.rotationSpeed = RandomFloat(-5.0f, 5.0f);
        p.bounceElasticity = 0.0f;
    }
}

void ParticleSystem::EmitBallRecallBurst(const Vector3& ballPos) {
    if (hitEffectTextureHandle_ != 0) {
        // Layer 1: Large primary spiky ink explosion flash at ball position
        {
            size_t idx = AllocateParticle();
            Particle& p = particles_[idx];
            p.isActive = true;
            p.blendMode = BlendMode::AlphaBlend;
            p.textureHandle = hitEffectTextureHandle_;
            p.currentAge = 0.0f;
            p.lifetime = 0.20f;

            p.position = Vector3(ballPos.x, ballPos.y, -0.1f);
            p.velocity = Vector3(0.0f, 0.0f, 0.0f);
            p.acceleration = Vector3(0.0f, 0.0f, 0.0f);

            p.startScale = 0.90f;
            p.endScale = 1.35f;
            p.currentScale = p.startScale;

            p.startColor = Vector4(0.03f, 0.03f, 0.03f, 0.95f);
            p.endColor = Vector4(0.03f, 0.03f, 0.03f, 0.0f);
            p.currentColor = p.startColor;

            p.rotation = RandomFloat(-0.3f, 0.3f);
            p.rotationSpeed = RandomFloat(-2.0f, 2.0f);
            p.bounceElasticity = 0.0f;
        }

        // Layer 2: Secondary offset rotated flare (~45 deg)
        {
            size_t idx = AllocateParticle();
            Particle& p = particles_[idx];
            p.isActive = true;
            p.blendMode = BlendMode::AlphaBlend;
            p.textureHandle = hitEffectTextureHandle_;
            p.currentAge = 0.0f;
            p.lifetime = 0.16f;

            p.position = Vector3(ballPos.x, ballPos.y, -0.1f);
            p.velocity = Vector3(0.0f, 0.0f, 0.0f);
            p.acceleration = Vector3(0.0f, 0.0f, 0.0f);

            p.startScale = 0.70f;
            p.endScale = 1.10f;
            p.currentScale = p.startScale;

            p.startColor = Vector4(0.06f, 0.06f, 0.06f, 0.85f);
            p.endColor = Vector4(0.06f, 0.06f, 0.06f, 0.0f);
            p.currentColor = p.startColor;

            p.rotation = 0.785398f + RandomFloat(-0.1f, 0.1f);
            p.rotationSpeed = RandomFloat(-2.5f, 2.5f);
            p.bounceElasticity = 0.0f;
        }
    }

    // 24 radial ink splatter droplets bursting in 360 degrees
    constexpr int kCount = 24;
    for (int i = 0; i < kCount; ++i) {
        size_t idx = AllocateParticle();
        Particle& p = particles_[idx];
        p.isActive = true;
        p.blendMode = BlendMode::AlphaBlend;
        p.textureHandle = circleTextureHandle_;
        p.currentAge = 0.0f;
        p.lifetime = RandomFloat(0.22f, 0.35f);

        p.position = Vector3(ballPos.x, ballPos.y, -0.1f);

        float baseAngle = (i * 2.0f * 3.14159265f) / static_cast<float>(kCount);
        float angle = baseAngle + RandomFloat(-0.12f, 0.12f);
        float speed = RandomFloat(6.5f, 11.5f);

        p.velocity = Vector3(std::cos(angle) * speed, std::sin(angle) * speed, 0.0f);
        p.acceleration = Vector3(-p.velocity.x * 6.0f, -p.velocity.y * 6.0f, 0.0f);

        p.startScale = RandomFloat(0.24f, 0.32f);
        p.endScale = RandomFloat(0.04f, 0.07f);
        p.currentScale = p.startScale;

        float inkGrey = RandomFloat(0.02f, 0.07f);
        p.startColor = Vector4(inkGrey, inkGrey, inkGrey, RandomFloat(0.90f, 1.0f));
        p.endColor = Vector4(inkGrey, inkGrey, inkGrey, 0.0f);
        p.currentColor = p.startColor;

        p.rotation = RandomFloat(0.0f, 6.283185f);
        p.rotationSpeed = RandomFloat(-6.0f, 6.0f);
        p.bounceElasticity = 0.0f;
    }
}

void ParticleSystem::EmitPlayerRecallAbsorb(const Vector3& playerPos) {
    EmitPlayerRecallAbsorb(playerPos, playerPos);
}

void ParticleSystem::EmitPlayerRecallAbsorb(const Vector3& playerPos, const Vector3& ballPos) {
    Vector3 playerCenter(playerPos.x, playerPos.y + 0.35f, -0.1f);

    // 1. Central absorption pulse at player center (ink condensing into body)
    if (hitEffectTextureHandle_ != 0) {
        size_t idx = AllocateParticle();
        Particle& p = particles_[idx];
        p.isActive = true;
        p.blendMode = BlendMode::AlphaBlend;
        p.textureHandle = hitEffectTextureHandle_;
        p.currentAge = 0.0f;
        p.lifetime = 0.22f;

        p.position = playerCenter;
        p.velocity = Vector3(0.0f, 0.0f, 0.0f);
        p.acceleration = Vector3(0.0f, 0.0f, 0.0f);

        p.startScale = 0.50f;
        p.endScale = 0.90f;
        p.currentScale = p.startScale;

        p.startColor = Vector4(0.04f, 0.04f, 0.04f, 0.85f);
        p.endColor = Vector4(0.04f, 0.04f, 0.04f, 0.0f);
        p.currentColor = p.startColor;

        p.rotation = RandomFloat(-0.2f, 0.2f);
        p.rotationSpeed = RandomFloat(-3.0f, 3.0f);
        p.bounceElasticity = 0.0f;
    }

    // 2. Inward-converging ink droplets rushing into the player from surrounding aura
    constexpr int kInwardCount = 18;
    for (int i = 0; i < kInwardCount; ++i) {
        size_t idx = AllocateParticle();
        Particle& p = particles_[idx];
        p.isActive = true;
        p.blendMode = BlendMode::AlphaBlend;
        p.textureHandle = circleTextureHandle_;
        p.currentAge = 0.0f;

        float lifetime = RandomFloat(0.20f, 0.28f);
        p.lifetime = lifetime;

        float theta = (i * 2.0f * 3.14159265f) / static_cast<float>(kInwardCount) + RandomFloat(-0.1f, 0.1f);
        float radius = RandomFloat(0.85f, 1.45f);

        Vector3 spawnPos(playerCenter.x + std::cos(theta) * radius, playerCenter.y + std::sin(theta) * radius, -0.1f);
        p.position = spawnPos;

        // Velocity vector directly aimed at playerCenter
        p.velocity = Vector3(
            (playerCenter.x - spawnPos.x) / lifetime,
            (playerCenter.y - spawnPos.y) / lifetime,
            0.0f
        );
        p.acceleration = Vector3(0.0f, 0.0f, 0.0f);

        p.startScale = RandomFloat(0.22f, 0.28f);
        p.endScale = 0.02f; // shrinks as it enters the player
        p.currentScale = p.startScale;

        float inkGrey = RandomFloat(0.02f, 0.07f);
        p.startColor = Vector4(inkGrey, inkGrey, inkGrey, RandomFloat(0.85f, 1.0f));
        p.endColor = Vector4(inkGrey, inkGrey, inkGrey, 0.15f);
        p.currentColor = p.startColor;

        p.rotation = RandomFloat(0.0f, 6.283185f);
        p.rotationSpeed = RandomFloat(-4.0f, 4.0f);
        p.bounceElasticity = 0.0f;
    }

    // 3. Fast ink tracer wisps flying from ball position towards player
    float dx = playerCenter.x - ballPos.x;
    float dy = playerCenter.y - ballPos.y;
    float dist = std::sqrt(dx * dx + dy * dy);

    if (dist > 0.6f) {
        float dirX = dx / dist;
        float dirY = dy / dist;
        float perpX = -dirY;
        float perpY = dirX;

        constexpr int kTracerCount = 8;
        for (int i = 0; i < kTracerCount; ++i) {
            size_t idx = AllocateParticle();
            Particle& p = particles_[idx];
            p.isActive = true;
            p.blendMode = BlendMode::AlphaBlend;
            p.textureHandle = circleTextureHandle_;
            p.currentAge = 0.0f;
            p.lifetime = RandomFloat(0.16f, 0.24f);

            // Stagger along the distance between ball and player
            float tDist = RandomFloat(0.15f, 0.85f);
            float lateralOffset = RandomFloat(-0.15f, 0.15f);

            p.position = Vector3(
                ballPos.x + dirX * (dist * tDist) + perpX * lateralOffset,
                ballPos.y + dirY * (dist * tDist) + perpY * lateralOffset,
                -0.1f
            );

            // High velocity flying towards player
            float tracerSpeed = RandomFloat(14.0f, 22.0f);
            p.velocity = Vector3(dirX * tracerSpeed, dirY * tracerSpeed, 0.0f);
            p.acceleration = Vector3(-dirX * 12.0f, -dirY * 12.0f, 0.0f);

            p.startScale = RandomFloat(0.18f, 0.24f);
            p.endScale = 0.03f;
            p.currentScale = p.startScale;

            float inkGrey = RandomFloat(0.02f, 0.06f);
            p.startColor = Vector4(inkGrey, inkGrey, inkGrey, RandomFloat(0.80f, 0.95f));
            p.endColor = Vector4(inkGrey, inkGrey, inkGrey, 0.0f);
            p.currentColor = p.startColor;

            p.rotation = RandomFloat(0.0f, 6.283185f);
            p.rotationSpeed = RandomFloat(-5.0f, 5.0f);
            p.bounceElasticity = 0.0f;
        }
    }
}

void ParticleSystem::EmitEnemySparks(const Vector3& impactPoint, const Vector3& impactVelocity) {
    // Trigger black ink impact splash on ball striking enemy
    EmitImpactFlash(impactPoint);

    constexpr int kCount = 28;
    float groundLevel = impactPoint.y - 0.4f;

    Vector3 baseDir = impactVelocity;
    if (baseDir.Length() > 0.1f) {
        baseDir = baseDir.Normalize();
    } else {
        baseDir = Vector3(0.0f, 1.0f, 0.0f);
    }

    for (int i = 0; i < kCount; ++i) {
        size_t idx = AllocateParticle();
        Particle& p = particles_[idx];
        p.isActive = true;
        p.blendMode = BlendMode::AlphaBlend;
        p.textureHandle = circleTextureHandle_;
        p.currentAge = 0.0f;
        p.lifetime = RandomFloat(0.45f, 0.58f); // ~0.5s

        p.position = Vector3(impactPoint.x, impactPoint.y, -0.1f);

        // Outward spray around impact vector + fountain upward
        float angleOffset = RandomFloat(-1.25f, 1.25f);
        float baseAngle = std::atan2(baseDir.y, baseDir.x);
        float finalAngle = baseAngle + angleOffset;
        float speed = RandomFloat(5.0f, 9.5f);

        p.velocity = Vector3(std::cos(finalAngle) * speed, std::sin(finalAngle) * speed + RandomFloat(1.5f, 4.0f), 0.0f);

        // Downward gravity acceleration -20.0f m/s^2
        p.acceleration = Vector3(0.0f, -20.0f, 0.0f);

        // Ground bounce elasticity 0.5
        p.bounceElasticity = 0.5f;
        p.groundY = groundLevel;

        // Scale
        p.startScale = RandomFloat(0.14f, 0.20f);
        p.endScale = RandomFloat(0.03f, 0.06f);
        p.currentScale = p.startScale;

        // Black ink fountain sparks
        float inkGrey = RandomFloat(0.02f, 0.08f);
        p.startColor = Vector4(inkGrey, inkGrey, inkGrey, RandomFloat(0.85f, 1.0f));
        p.endColor = Vector4(inkGrey, inkGrey, inkGrey, 0.0f);
        p.currentColor = p.startColor;

        p.rotation = RandomFloat(0.0f, 6.283185f);
        p.rotationSpeed = RandomFloat(-8.0f, 8.0f);
    }
}

size_t ParticleSystem::GetActiveParticleCount() const {
    size_t count = 0;
    for (size_t i = 0; i < kMaxParticles; ++i) {
        if (particles_[i].isActive) {
            count++;
        }
    }
    return count;
}

void ParticleSystem::Clear() {
    for (size_t i = 0; i < kMaxParticles; ++i) {
        particles_[i].isActive = false;
    }
    nextFreeIndex_ = 0;
}
