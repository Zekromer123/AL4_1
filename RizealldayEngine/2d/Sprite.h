#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <string>
#include "math/Vector2.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include "math/Matrix4x4.h"

struct SpriteVertexPosUv {
	Vector4 position;
	Vector2 texcoord;
};

struct SpriteTransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
};

struct SpriteMaterial {
	Vector4 color;
};

class Sprite {
public:
	/// Factory method: Sprite::Create(textureHandle, position)
	static Sprite* Create(uint32_t textureHandle, Vector2 position = { 0.0f, 0.0f },
	                      Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f },
	                      Vector2 anchorPoint = { 0.0f, 0.0f });

	Sprite() = default;
	~Sprite();

	void Initialize(uint32_t textureHandle, Vector2 position, Vector4 color, Vector2 anchorPoint);
	void Update();
	void Draw(ID3D12GraphicsCommandList* cmdList = nullptr);

	// Transforms and properties
	void SetPosition(const Vector2& pos) { position_ = pos; }
	const Vector2& GetPosition() const { return position_; }

	void SetSize(const Vector2& size) { size_ = size; isVertexDirty_ = true; }
	const Vector2& GetSize() const { return size_; }

	void SetRotation(float radian) { rotation_ = radian; }
	float GetRotation() const { return rotation_; }

	void SetAnchorPoint(const Vector2& anchor) { anchorPoint_ = anchor; isVertexDirty_ = true; }
	const Vector2& GetAnchorPoint() const { return anchorPoint_; }

	void SetColor(const Vector4& color) { color_ = color; }
	const Vector4& GetColor() const { return color_; }

	void SetIsFlipX(bool isFlipX) { isFlipX_ = isFlipX; isVertexDirty_ = true; }
	bool GetIsFlipX() const { return isFlipX_; }

	void SetIsFlipY(bool isFlipY) { isFlipY_ = isFlipY; isVertexDirty_ = true; }
	bool GetIsFlipY() const { return isFlipY_; }

	void SetInvisible(bool isInvisible) { isInvisible_ = isInvisible; }
	bool IsInvisible() const { return isInvisible_; }

	/// UV Rect Cropping (base coordinate and size in source texture pixels)
	void SetTextureRect(const Vector2& texBase, const Vector2& texSize);

	void SetTextureHandle(uint32_t textureHandle);
	uint32_t GetTextureHandle() const { return textureHandle_; }

private:
	void UpdateVertices();

	uint32_t textureHandle_ = 0;
	Vector2 position_{ 0.0f, 0.0f };
	Vector2 size_{ 100.0f, 100.0f };
	float rotation_ = 0.0f;
	Vector2 anchorPoint_{ 0.0f, 0.0f };
	Vector4 color_{ 1.0f, 1.0f, 1.0f, 1.0f };

	bool isFlipX_ = false;
	bool isFlipY_ = false;
	bool isInvisible_ = false;

	Vector2 texBase_{ 0.0f, 0.0f };
	Vector2 texSize_{ 0.0f, 0.0f };
	Vector2 textureNaturalSize_{ 100.0f, 100.0f };
	bool isCustomTexRect_ = false;
	bool isVertexDirty_ = true;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	SpriteVertexPosUv* vertexData_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer_;
	D3D12_INDEX_BUFFER_VIEW indexBufferView_{};

	Microsoft::WRL::ComPtr<ID3D12Resource> constBufferTransform_;
	SpriteTransformationMatrix* transformData_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> constBufferMaterial_;
	SpriteMaterial* materialData_ = nullptr;
};
