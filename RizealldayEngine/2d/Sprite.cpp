#include "2d/Sprite.h"
#include "2d/SpriteCommon.h"
#include "base/DirectXCommon.h"
#include "base/TextureManager.h"
#include "math/MathUtil.h"
#include <algorithm>
#include <utility>

Sprite* Sprite::Create(uint32_t textureHandle, Vector2 position, Vector4 color, Vector2 anchorPoint) {
	Sprite* sprite = new Sprite();
	sprite->Initialize(textureHandle, position, color, anchorPoint);
	return sprite;
}

Sprite::~Sprite() {
	if (vertexBuffer_) {
		vertexBuffer_->Unmap(0, nullptr);
		vertexData_ = nullptr;
	}
	if (constBufferTransform_) {
		constBufferTransform_->Unmap(0, nullptr);
		transformData_ = nullptr;
	}
	if (constBufferMaterial_) {
		constBufferMaterial_->Unmap(0, nullptr);
		materialData_ = nullptr;
	}
}

void Sprite::Initialize(uint32_t textureHandle, Vector2 position, Vector4 color, Vector2 anchorPoint) {
	textureHandle_ = textureHandle;
	position_ = position;
	color_ = color;
	anchorPoint_ = anchorPoint;

	DirectXCommon* dxCommon = DirectXCommon::GetInstance();

	// 1. Texture natural size
	textureNaturalSize_ = TextureManager::GetInstance()->GetTextureSize(textureHandle_);
	if (textureNaturalSize_.x <= 0.0f || textureNaturalSize_.y <= 0.0f) {
		textureNaturalSize_ = { 100.0f, 100.0f };
	}
	size_ = textureNaturalSize_;

	// 2. Vertex Buffer (4 vertices)
	vertexBuffer_.Attach(dxCommon->CreateBufferResource(sizeof(SpriteVertexPosUv) * 4));
	vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeof(SpriteVertexPosUv) * 4;
	vertexBufferView_.StrideInBytes = sizeof(SpriteVertexPosUv);
	vertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

	// 3. Index Buffer (6 indices: 0, 1, 2,  1, 3, 2)
	indexBuffer_.Attach(dxCommon->CreateBufferResource(sizeof(uint32_t) * 6));
	indexBufferView_.BufferLocation = indexBuffer_->GetGPUVirtualAddress();
	indexBufferView_.SizeInBytes = sizeof(uint32_t) * 6;
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
	uint32_t* indexData = nullptr;
	indexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
	indexData[0] = 0;
	indexData[1] = 1;
	indexData[2] = 2;
	indexData[3] = 1;
	indexData[4] = 3;
	indexData[5] = 2;
	indexBuffer_->Unmap(0, nullptr);

	// 4. Transform CBV
	constBufferTransform_.Attach(dxCommon->CreateBufferResource((sizeof(SpriteTransformationMatrix) + 255) & ~255));
	constBufferTransform_->Map(0, nullptr, reinterpret_cast<void**>(&transformData_));
	transformData_->WVP = Mathematics::MakeIdentity4x4();
	transformData_->World = Mathematics::MakeIdentity4x4();

	// 5. Material CBV
	constBufferMaterial_.Attach(dxCommon->CreateBufferResource((sizeof(SpriteMaterial) + 255) & ~255));
	constBufferMaterial_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	materialData_->color = color_;

	isVertexDirty_ = true;
	Update();
}

void Sprite::SetTextureRect(const Vector2& texBase, const Vector2& texSize) {
	texBase_ = texBase;
	texSize_ = texSize;
	isCustomTexRect_ = true;
	isVertexDirty_ = true;
}

void Sprite::SetTextureHandle(uint32_t textureHandle) {
	textureHandle_ = textureHandle;
	textureNaturalSize_ = TextureManager::GetInstance()->GetTextureSize(textureHandle_);
	if (textureNaturalSize_.x <= 0.0f || textureNaturalSize_.y <= 0.0f) {
		textureNaturalSize_ = { 100.0f, 100.0f };
	}
	if (!isCustomTexRect_) {
		size_ = textureNaturalSize_;
	}
	isVertexDirty_ = true;
}

void Sprite::UpdateVertices() {
	if (!vertexData_) return;

	if (textureNaturalSize_.x <= 0.0f || textureNaturalSize_.y <= 0.0f) {
		textureNaturalSize_ = TextureManager::GetInstance()->GetTextureSize(textureHandle_);
		if (textureNaturalSize_.x <= 0.0f || textureNaturalSize_.y <= 0.0f) {
			textureNaturalSize_ = { 100.0f, 100.0f };
		}
	}

	Vector2 base = isCustomTexRect_ ? texBase_ : Vector2{ 0.0f, 0.0f };
	Vector2 cropSize = isCustomTexRect_ ? texSize_ : textureNaturalSize_;

	float uvLeft = base.x / textureNaturalSize_.x;
	float uvRight = (base.x + cropSize.x) / textureNaturalSize_.x;
	float uvTop = base.y / textureNaturalSize_.y;
	float uvBottom = (base.y + cropSize.y) / textureNaturalSize_.y;

	if (isFlipX_) std::swap(uvLeft, uvRight);
	if (isFlipY_) std::swap(uvTop, uvBottom);

	float left = -anchorPoint_.x * size_.x;
	float right = (1.0f - anchorPoint_.x) * size_.x;
	float top = -anchorPoint_.y * size_.y;
	float bottom = (1.0f - anchorPoint_.y) * size_.y;

	// 0: Top-Left
	vertexData_[0].position = { left, top, 0.0f, 1.0f };
	vertexData_[0].texcoord = { uvLeft, uvTop };

	// 1: Top-Right
	vertexData_[1].position = { right, top, 0.0f, 1.0f };
	vertexData_[1].texcoord = { uvRight, uvTop };

	// 2: Bottom-Left
	vertexData_[2].position = { left, bottom, 0.0f, 1.0f };
	vertexData_[2].texcoord = { uvLeft, uvBottom };

	// 3: Bottom-Right
	vertexData_[3].position = { right, bottom, 0.0f, 1.0f };
	vertexData_[3].texcoord = { uvRight, uvBottom };

	isVertexDirty_ = false;
}

void Sprite::Update() {
	if (isVertexDirty_) {
		UpdateVertices();
	}

	Matrix4x4 matWorld = Mathematics::MakeAffineMatrix(
		{ 1.0f, 1.0f, 1.0f },
		{ 0.0f, 0.0f, rotation_ },
		{ position_.x, position_.y, 0.0f }
	);

	Matrix4x4 matView = Mathematics::MakeIdentity4x4();
	Matrix4x4 matProj = Mathematics::MakeOrthographicMatrix(0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 100.0f);

	if (transformData_) {
		transformData_->World = matWorld;
		transformData_->WVP = Mathematics::Multiply(matWorld, Mathematics::Multiply(matView, matProj));
	}

	if (materialData_) {
		materialData_->color = color_;
	}
}

void Sprite::Draw(ID3D12GraphicsCommandList* cmdList) {
	if (isInvisible_) return;

	if (!cmdList) {
		cmdList = DirectXCommon::GetInstance()->GetCommandList();
	}

	cmdList->SetGraphicsRootConstantBufferView(0, constBufferTransform_->GetGPUVirtualAddress());
	cmdList->SetGraphicsRootConstantBufferView(1, constBufferMaterial_->GetGPUVirtualAddress());
	cmdList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetGPUHandle(textureHandle_));

	cmdList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	cmdList->IASetIndexBuffer(&indexBufferView_);
	cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}
