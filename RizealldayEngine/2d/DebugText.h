#pragma once
#include <string>
#include <vector>
#include <memory>
#include <d3d12.h>
#include "2d/Sprite.h"
#include "math/Vector4.h"

class DebugText {
public:
	static DebugText* GetInstance();

	void Initialize(const std::string& fontPath = "Resources/debugfont.png");
	void Finalize();

	/// Print debug text at screen coordinates (x, y)
	void Print(const std::string& text, float x, float y, float scale = 1.0f, const Vector4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

	/// Draw all queued text sprites for the current frame
	void DrawAll(ID3D12GraphicsCommandList* cmdList = nullptr);

private:
	DebugText() = default;
	~DebugText() = default;
	DebugText(const DebugText&) = delete;
	DebugText& operator=(const DebugText&) = delete;

	static const int kMaxChars = 1024;
	static const int kFontWidth = 9;
	static const int kFontHeight = 18;
	static const int kFontLineCount = 14;

	uint32_t fontTextureHandle_ = 0;
	std::vector<std::unique_ptr<Sprite>> spritePool_;
	size_t activeCount_ = 0;
};
