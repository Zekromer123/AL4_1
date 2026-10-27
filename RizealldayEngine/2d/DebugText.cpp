#include "2d/DebugText.h"
#include "base/TextureManager.h"
#include <algorithm>

DebugText* DebugText::GetInstance() {
	static DebugText instance;
	return &instance;
}

void DebugText::Initialize(const std::string& fontPath) {
	fontTextureHandle_ = TextureManager::GetInstance()->LoadTexture(fontPath);
	activeCount_ = 0;
	spritePool_.clear();
	spritePool_.reserve(kMaxChars);
	for (int i = 0; i < kMaxChars; ++i) {
		spritePool_.push_back(std::unique_ptr<Sprite>(Sprite::Create(fontTextureHandle_, { 0.0f, 0.0f })));
	}
}

void DebugText::Finalize() {
	spritePool_.clear();
	activeCount_ = 0;
}

void DebugText::Print(const std::string& text, float x, float y, float scale, const Vector4& color) {
	float curX = x;
	float curY = y;

	for (char c : text) {
		if (c == '\n') {
			curX = x;
			curY += static_cast<float>(kFontHeight) * scale;
			continue;
		}

		if (activeCount_ >= spritePool_.size()) {
			break;
		}

		int charIndex = static_cast<int>(c) - 32;
		if (charIndex >= 0 && charIndex < 98) {
			int col = charIndex % kFontLineCount;
			int row = charIndex / kFontLineCount;

			float srcX = static_cast<float>(col * kFontWidth);
			float srcY = static_cast<float>(row * kFontHeight);
			float srcW = static_cast<float>(kFontWidth);
			float srcH = static_cast<float>(kFontHeight);

			Sprite* sp = spritePool_[activeCount_].get();
			sp->SetTextureRect({ srcX, srcY }, { srcW, srcH });
			sp->SetSize({ srcW * scale, srcH * scale });
			sp->SetPosition({ curX, curY });
			sp->SetColor(color);
			sp->SetAnchorPoint({ 0.0f, 0.0f });
			sp->SetInvisible(false);
			sp->Update();

			activeCount_++;
		}

		curX += static_cast<float>(kFontWidth) * scale;
	}
}

void DebugText::DrawAll(ID3D12GraphicsCommandList* cmdList) {
	for (size_t i = 0; i < activeCount_; ++i) {
		spritePool_[i]->Draw(cmdList);
	}
	activeCount_ = 0;
}
