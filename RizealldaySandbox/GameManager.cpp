#include "GameManager.h"
#include "base/RizeEngine.h"
#include "base/TextureManager.h"
#include "input/Input.h"
#include "3d/CollisionManager.h"
#include "2d/SpriteCommon.h"
#include "math/MathUtil.h"
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <filesystem>
#include <fstream>

// ============================================================================
// Centralized Replaceable UI Sprite Configuration Table
// ============================================================================
// Users can easily swap filenames and toggle useCrop = false when using custom full-texture PNGs.
static const UISpriteSpec kSprStageSelectHeader = {
	"Resources/UI/stageselect.png",
	Vector2{ 310.0f, 37.0f },
	true,
	Vector2{ 184.0f, 164.0f },
	Vector2{ 270.0f, 32.0f }
};
static const UISpriteSpec kPlay = {
	"Resources/UI/play.png",
	Vector2{ 150.0f, 37.0f },
	false,
	Vector2{ 0.0f, 0.0f },
	Vector2{ 0.0f, 0.0f }
};

static const UISpriteSpec kSprTutorialBtn = {
	"Resources/UI/tutorial.png",
	Vector2{ 239.0f, 29.0f },
	true,
	Vector2{ 201.0f, 166.0f },
	Vector2{ 239.0f, 29.0f }
};

static const UISpriteSpec kSprStage1Btn = {
	"Resources/UI/stage1.png",
	Vector2{ 164.0f, 32.0f },
	true,
	Vector2{ 236.0f, 164.0f },
	Vector2{ 164.0f, 32.0f }
};

static const UISpriteSpec kSprStage2Btn = {
	"Resources/UI/stage2.png",
	Vector2{ 163.0f, 32.0f },
	true,
	Vector2{ 236.0f, 164.0f },
	Vector2{ 163.0f, 32.0f }
};

static const UISpriteSpec kSprStage3Btn = {
	"Resources/UI/stage3.png",
	Vector2{ 163.0f, 32.0f },
	true,
	Vector2{ 236.0f, 164.0f },
	Vector2{ 163.0f, 32.0f }
};

static const UISpriteSpec kSprBackBtn = {
	"Resources/UI/back.png",
	Vector2{ 65.0f, 33.0f },
	true,
	Vector2{ 286.0f, 163.0f },
	Vector2{ 65.0f, 33.0f }
};

static const UISpriteSpec kSprComboLabel = {
	"Resources/UI/combo.png",
	Vector2{ 173.0f, 44.0f },
	true,
	Vector2{ 232.0f, 160.0f },
	Vector2{ 173.0f, 44.0f }
};

static const UISpriteSpec kSprDigitSpec = {
	"Resources/UI/0.png",
	Vector2{ 25.0f, 31.0f },
	true,
	Vector2{ 18.0f, 10.0f },
	Vector2{ 25.0f, 31.0f }
};

// Replaceable specs with safe fallbacks:
static const UISpriteSpec kSprTitleLogo = {
	"Resources/UI/title_logo.png",
	Vector2{ 460.0f, 254.0f },
	false,
	Vector2{ 0.0f, 0.0f },
	Vector2{ 0.0f, 0.0f }
};

static const UISpriteSpec kSprPressStart = {
	"Resources/UI/press_start.png",
	Vector2{ 228.0f, 55.0f },
	false,
	Vector2{ 0.0f, 0.0f },
	Vector2{ 0.0f, 0.0f }
};

static const UISpriteSpec kSprStageClear = {
	"Resources/UI/stage_clear.png",
	Vector2{ 500.0f, 340.0f },
	false,
	Vector2{ 0.0f, 0.0f },
	Vector2{ 500.0f, 340.0f }
};

static const UISpriteSpec kSprGameOver = {
	"Resources/UI/game_over.png",
	Vector2{ 500.0f, 340.0f },
	false,
	Vector2{ 0.0f, 0.0f },
	Vector2{ 500.0f, 340.0f }
};

static const UISpriteSpec kSprTutorialGuide = {
	"Resources/UI/tutorial_guide.png",
	Vector2{ 240.0f, 322.0f },
	false,
	Vector2{ 0.0f, 0.0f },
	Vector2{ 764.0f, 1024.0f }
};

static const UISpriteSpec kSprIconPlayer = {
	"Resources/icon/icon_player.png",
	Vector2{ 32.0f, 32.0f },
	false,
	Vector2{ 0.0f, 0.0f },
	Vector2{ 0.0f, 0.0f }
};

static const UISpriteSpec kArrow = {
	"Resources/UI/arrow.png",
	Vector2{ 32.0f, 32.0f },
	false,
	Vector2{ 0.0f, 0.0f },
	Vector2{ 0.0f, 0.0f }
};

static const UISpriteSpec kSprIconBall = {
	"Resources/icon/icon_ball.png",
	Vector2{ 32.0f, 32.0f },
	false,
	Vector2{ 0.0f, 0.0f },
	Vector2{ 0.0f, 0.0f }
};

static const UISpriteSpec kSprIconEnemy = {
	"Resources/icon/icon_enemy.png",
	Vector2{ 32.0f, 32.0f },
	false,
	Vector2{ 0.0f, 0.0f },
	Vector2{ 0.0f, 0.0f }
};

static const UISpriteSpec kSprHpFill = {
	"Resources/UI/hp_fill.png",
	Vector2{ 280.0f, 28.0f },
	false,
	Vector2{ 0.0f, 0.0f },
	Vector2{ 0.0f, 0.0f }
};

// Safe file loader using std::ifstream that falls back gracefully if custom image does not exist yet
static uint32_t LoadTextureOrFallback(const std::string& path, const std::string& fallbackPath) {
	TextureManager* texMgr = TextureManager::GetInstance();
	std::ifstream f(path);
	if (f.is_open()) {
		f.close();
		return texMgr->LoadTexture(path);
	}
	return texMgr->LoadTexture(fallbackPath);
}

// ============================================================================
// GameManager Lifecycle & Sprite Pool
// ============================================================================
GameManager::GameManager() = default;

GameManager::~GameManager() {
	Audio* audio = Audio::GetInstance();
	if (audio) {
		audio->StopBGM();
		audio->SoundUnload(&bgmSound_);
		audio->SoundUnload(&bounceSound_);
	}

	for (auto& entry : spritePool_) {
		for (Sprite* sprite : entry.second) {
			delete sprite;
		}
	}
	spritePool_.clear();
}

Sprite* GameManager::AcquireSprite(uint32_t texture) {
	std::vector<Sprite*>& list = spritePool_[texture];
	size_t& used = spritePoolUsed_[texture];
	if (used >= list.size()) {
		list.push_back(Sprite::Create(texture, Vector2{ 0.0f, 0.0f }));
	}
	return list[used++];
}

void GameManager::ResetSpritePool() {
	for (auto& entry : spritePoolUsed_) {
		entry.second = 0;
	}
}

// ============================================================================
// Drawing Helpers
// ============================================================================
void GameManager::DrawRect(const Vector2& pos, const Vector2& size, const Vector4& color) {
	Sprite* rect = AcquireSprite(texWhite_);
	if (!rect) return;
	rect->SetAnchorPoint(Vector2{ 0.0f, 0.0f });
	rect->SetPosition(pos);
	rect->SetSize(size);
	rect->SetColor(color);
	rect->Update();
	rect->Draw();
}

void GameManager::DrawRectOutline(const Vector2& pos, const Vector2& size, const Vector4& color, float border) {
	DrawRect(pos, Vector2{ size.x, border }, color);
	DrawRect(Vector2{ pos.x, pos.y + size.y - border }, Vector2{ size.x, border }, color);
	DrawRect(pos, Vector2{ border, size.y }, color);
	DrawRect(Vector2{ pos.x + size.x - border, pos.y }, Vector2{ border, size.y }, color);
}

void GameManager::DrawUISprite(uint32_t texHandle, const UISpriteSpec& spec, const Vector2& pos,
	const Vector4& color, const Vector2& anchor, float scale) {
	if (texHandle == 0) return;
	Sprite* sprite = AcquireSprite(texHandle);
	if (!sprite) return;

	if (spec.useCrop) {
		sprite->SetTextureRect(spec.cropPos, spec.cropSize);
	}
	else {
		Vector2 nat = TextureManager::GetInstance()->GetTextureSize(texHandle);
		if (nat.x > 0.0f && nat.y > 0.0f) {
			sprite->SetTextureRect(Vector2{ 0.0f, 0.0f }, nat);
		}
	}

	Vector2 renderSize = Vector2{ spec.displaySize.x * scale, spec.displaySize.y * scale };
	if (renderSize.x <= 0.0f || renderSize.y <= 0.0f) {
		renderSize = Vector2{ spec.cropSize.x * scale, spec.cropSize.y * scale };
	}
	if (renderSize.x <= 0.0f || renderSize.y <= 0.0f) {
		renderSize = Vector2{ 32.0f * scale, 32.0f * scale };
	}

	sprite->SetAnchorPoint(anchor);
	sprite->SetPosition(pos);
	sprite->SetSize(renderSize);
	sprite->SetColor(color);
	sprite->Update();
	sprite->Draw();
}

void GameManager::DrawNumberSprites(int value, const Vector2& pos, float digitHeight,
	const Vector4& color, const Vector2& anchor) {
	std::string str = std::to_string((std::max)(0, value));
	const float aspect = kSprDigitSpec.cropSize.x / (std::max)(1.0f, kSprDigitSpec.cropSize.y);
	float digitWidth = digitHeight * aspect;
	float spacing = digitWidth * 0.15f;
	float totalW = str.length() * digitWidth + (str.length() > 0 ? (str.length() - 1) * spacing : 0.0f);

	float startX = pos.x - anchor.x * totalW;
	float startY = pos.y - anchor.y * digitHeight;

	float curX = startX;
	for (char c : str) {
		int d = c - '0';
		if (d >= 0 && d <= 9) {
			uint32_t tex = texDigits_[d];
			Sprite* sp = AcquireSprite(tex);
			if (sp) {
				if (kSprDigitSpec.useCrop) {
					sp->SetTextureRect(kSprDigitSpec.cropPos, kSprDigitSpec.cropSize);
				}
				else {
					Vector2 nat = TextureManager::GetInstance()->GetTextureSize(tex);
					if (nat.x > 0.0f && nat.y > 0.0f) {
						sp->SetTextureRect(Vector2{ 0.0f, 0.0f }, nat);
					}
				}
				sp->SetAnchorPoint(Vector2{ 0.0f, 0.0f });
				sp->SetPosition(Vector2{ curX, startY });
				sp->SetSize(Vector2{ digitWidth, digitHeight });
				sp->SetColor(color);
				sp->Update();
				sp->Draw();
			}
			curX += digitWidth + spacing;
		}
	}
}

void GameManager::UpdateUIParticles(float deltaTime) {
	for (auto& p : uiInkParticles_) {
		p.pos.y -= p.speedY * deltaTime;
		p.pos.x += std::sin(uiTime_ * p.waveFreq + p.wavePhase) * p.waveAmp * deltaTime;
		if (p.pos.y < -30.0f) {
			p.pos.y = 750.0f;
			p.pos.x = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 1280.0f;
		}
		if (p.pos.x < -50.0f) {
			p.pos.x = 1330.0f;
		}
		else if (p.pos.x > 1330.0f) {
			p.pos.x = -50.0f;
		}
	}
}

void GameManager::DrawUIParticles() {
	if (texCircle_ == 0) return;
	Vector2 nat = TextureManager::GetInstance()->GetTextureSize(texCircle_);

	for (const auto& p : uiInkParticles_) {
		Sprite* sp = AcquireSprite(texCircle_);
		if (!sp) continue;

		if (nat.x > 0.0f && nat.y > 0.0f) {
			sp->SetTextureRect(Vector2{ 0.0f, 0.0f }, nat);
		}

		float yNorm = std::clamp(p.pos.y / 720.0f, 0.0f, 1.0f);
		float edgeFade = std::sin(yNorm * 3.14159265f);
		float pulse = 0.8f + 0.2f * std::sin(uiTime_ * 3.0f + p.wavePhase);
		float alpha = std::clamp(p.maxAlpha * edgeFade * pulse, 0.0f, 1.0f);

		sp->SetAnchorPoint(Vector2{ 0.5f, 0.5f });
		sp->SetPosition(p.pos);
		sp->SetSize(Vector2{ p.size, p.size });
		sp->SetColor(Vector4{ 0.05f, 0.05f, 0.05f, alpha });
		sp->Update();
		sp->Draw();
	}
}

// ============================================================================
// Initialization
// ============================================================================
void GameManager::Initialize(Model* playerModel, Model* cubeModel, Model* ballModel, Camera* camera, Model* skyDomeModel) {
	playerModel_ = playerModel;
	cubeModel_ = cubeModel;
	ballModel_ = ballModel;
	camera_ = camera;
	skyDomeModel_ = skyDomeModel ? skyDomeModel : Model::CreateFromFile("Resources/SkyDome", "SkyDome.obj");
	darumaModel_ = Model::CreateFromFile("Resources/Enemy", "daruma.obj");
	lanternModel_ = Model::CreateFromFile("Resources/Enemy", "lantern.obj");

	if (skyDomeModel_) {
		skyDomeObj_.Initialize();
		skyDomeObj_.SetModel(skyDomeModel_);
		skyDomeObj_.SetCamera(camera_);
		skyDomeObj_.worldTransform.Initialize();
		skyDomeObj_.worldTransform.scale = { 1.0f, 1.0f, 1.0f };
		if (skyDomeObj_.GetMaterial()) {
			skyDomeObj_.GetMaterial()->color = { 1.0f, 1.0f, 1.0f, 1.0f };
			skyDomeObj_.GetMaterial()->enableLighting = 0; // Pure unlit paper texture
		}
	}

	currentScene_ = GameScene::Title;
	menuCursor_ = 0;
	stageSelectCursor_ = 0;
	endModalCursor_ = 0;
	selectedStage_ = 0;
	isRunning_ = true;
	uiTime_ = 0.0f;
	comboPopScale_ = 1.0f;
	lastComboCount_ = 0;

	// Define available stages
	stages_.clear();
	stages_.push_back({
		"Tutorial Arena",
		"Practice Ricochet & Slime Punch",
		"Resources/maps/map_tutorial.csv",
		"Tutorial Arena (32x16) - 1 Target Bot",
		true
		});
	stages_.push_back({
		"Stage 1: Terrace Citadel",
		"Citadel Terraces (50x20) - 5 Enemies",
		"Resources/maps/map_1_terrace.csv",
		"Terrace Citadel (50x20) - 5 Target Bots",
		false
		});
	stages_.push_back({
		"Stage 2: Power Grid",
		"Subterranean Facility (100x28) - 13 Enemies",
		"Resources/maps/map_2_courtyard.csv",
		"Power Subterranean Facility (100x28) - 13 Target Bots",
		false
		});
	stages_.push_back({
		"Stage 3: Wily Citadel",
		"Citadel Spire & Pinnacle (108x32) - 15 Enemies",
		"Resources/maps/map_3_fortress.csv",
		"Citadel Spire & Pinnacle (108x32) - 15 Target Bots",
		false
		});

	// Load UI textures via TextureManager
	TextureManager* texMgr = TextureManager::GetInstance();
	texWhite_ = texMgr->GetDefaultWhiteTexture();
	texStageSelect_ = texMgr->LoadTexture(kSprStageSelectHeader.filePath);
	texPlay_ = texMgr->LoadTexture(kPlay.filePath);
	texArrow_ = texMgr->LoadTexture(kArrow.filePath);
	texTutorial_ = texMgr->LoadTexture(kSprTutorialBtn.filePath);
	texStage1_ = texMgr->LoadTexture(kSprStage1Btn.filePath);
	texStage2_ = texMgr->LoadTexture(kSprStage2Btn.filePath);
	texStage3_ = texMgr->LoadTexture(kSprStage3Btn.filePath);
	texBack_ = texMgr->LoadTexture(kSprBackBtn.filePath);
	texCombo_ = texMgr->LoadTexture(kSprComboLabel.filePath);
	texHpFill_ = texMgr->LoadTexture(kSprHpFill.filePath);
	texIconBall_ = texMgr->LoadTexture(kSprIconBall.filePath);
	texIconEnemy_ = texMgr->LoadTexture(kSprIconEnemy.filePath);
	texIconPlayer_ = texMgr->LoadTexture(kSprIconPlayer.filePath);

	for (int i = 0; i <= 9; ++i) {
		std::string digitPath = "Resources/UI/" + std::to_string(i) + ".png";
		texDigits_[i] = texMgr->LoadTexture(digitPath);
	}

	// Replaceable UI textures (with safe fallbacks)
	texTitleLogo_ = LoadTextureOrFallback(kSprTitleLogo.filePath, "Resources/UI/stageselect.png");
	texPressStart_ = LoadTextureOrFallback(kSprPressStart.filePath, "Resources/UI/stageselect.png");
	texStageClear_ = LoadTextureOrFallback(kSprStageClear.filePath, "Resources/UI/stageselect.png");
	texGameOver_ = LoadTextureOrFallback(kSprGameOver.filePath, "Resources/UI/back.png");
	texTutorialGuide_ = LoadTextureOrFallback(kSprTutorialGuide.filePath, "Resources/UI/tutorial.png");

	std::ifstream frameFile("Resources/UI/minimap_frame.png");
	if (frameFile.is_open()) {
		frameFile.close();
		hasCustomMinimapFrame_ = true;
		texMinimapFrame_ = texMgr->LoadTexture("Resources/UI/minimap_frame.png");
	}
	else {
		hasCustomMinimapFrame_ = false;
		texMinimapFrame_ = 0;
	}

	// Load circle texture for 2D UI ink firefly particles and initialize 45 particles
	texCircle_ = texMgr->LoadTexture("Resources/circle.png");
	for (size_t i = 0; i < uiInkParticles_.size(); ++i) {
		auto& p = uiInkParticles_[i];
		float rx = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		float ry = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		float rs = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		float rz = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		float ra = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		float rf = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		float rp = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		float rm = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

		p.pos.x = rx * 1280.0f;
		p.pos.y = ry * 720.0f;
		p.speedY = 25.0f + rs * 40.0f;
		p.size = 6.0f + rz * 16.0f;
		p.waveAmp = 10.0f + ra * 25.0f;
		p.waveFreq = 1.2f + rf * 1.8f;
		p.wavePhase = rp * 6.283185f;
		p.maxAlpha = 0.25f + rm * 0.50f;
	}

	particleSystem_.Initialize(camera);
	player_.SetParticleSystem(&particleSystem_);
	ball_.SetParticleSystem(&particleSystem_);

	// Audio Initialization & BGM Looping across all scenes
	Audio* audio = Audio::GetInstance();
	if (audio) {
		std::string bgmPath = "Resources/bgm.wav";
		if (!std::ifstream(bgmPath).is_open()) bgmPath = "Resources/audio/bgm.wav";
		bgmSound_ = audio->SoundLoadWave(bgmPath);

		std::string bouncePath = "Resources/ball_bounce.wav";
		if (!std::ifstream(bouncePath).is_open()) bouncePath = "Resources/audio/ball_bounce.wav";
		bounceSound_ = audio->SoundLoadWave(bouncePath);
		// Buang lead-in senyap (~250ms) & ekor panjang agar suara pantulan terdengar tepat saat kontak
		audio->SoundTrim(&bounceSound_, 0.25f, 0.40f);

		ball_.SetBounceSound(bounceSound_);

		// Start playing BGM looping across all scenes
		audio->PlayBGM(bgmSound_, 0.5f);
	}

	ResetGame(0);
}

void GameManager::SelectStageAndPlay(int stageIndex) {
	ResetGame(stageIndex);
	currentScene_ = GameScene::GamePlay;
}

void GameManager::ResetGame(int stageIndex) {
	if (stageIndex >= 0 && stageIndex < static_cast<int>(stages_.size())) {
		selectedStage_ = stageIndex;
		stageSelectCursor_ = stageIndex;
	}

	std::string csvPath = "Resources/maps/map_tutorial.csv";
	if (selectedStage_ >= 0 && selectedStage_ < static_cast<int>(stages_.size())) {
		csvPath = stages_[selectedStage_].csvPath;
	}

	// 1. Load map CSV
	mapChip_.LoadCSV(csvPath, cubeModel_, camera_);

	// 2. Initialize player and place at spawn
	player_.Initialize(playerModel_, camera_);
	player_.SetPosition(mapChip_.GetPlayerSpawnPos());

	player_.SetParticleSystem(&particleSystem_);
	ball_.SetParticleSystem(&particleSystem_);
	for (auto& enemy : enemies_) {
		enemy->SetParticleSystem(&particleSystem_);
	}

	// 3. Initialize ball
	ball_.Initialize(ballModel_, camera_);
	ball_.SetBounceSound(bounceSound_);

	// 4. Spawn enemies from map
	enemies_.clear();
	for (const Vector3& spawnPos : mapChip_.GetEnemySpawnPositions()) {
		auto enemy = std::make_unique<Enemy>();
		enemy->Initialize(darumaModel_, lanternModel_, camera_, spawnPos);
		enemy->SetParticleSystem(&particleSystem_);
		enemies_.push_back(std::move(enemy));
	}
	hasSpawnedEnemies_ = !enemies_.empty();

	cameraSnap_ = true; // camera starts on the player
}

// ============================================================================
// Game Loop Update
// ============================================================================
void GameManager::Update(float deltaTime) {
	uiTime_ += deltaTime;
	UpdateUIParticles(deltaTime);
	Input* input = RizeEngine::GetInput();

	// Ensure BGM continues playing looping across all scenes
	Audio* audio = Audio::GetInstance();
	if (audio && !audio->IsBGMPlaying() && bgmSound_.pBuffer) {
		audio->PlayBGM(bgmSound_, 0.5f);
	}

	// Combo pop-up scale animation
	int curCombo = ball_.GetComboCount();
	if (curCombo > lastComboCount_) {
		comboPopScale_ = 1.35f;
	}
	lastComboCount_ = curCombo;

	if (comboPopScale_ > 1.0f) {
		comboPopScale_ = (std::max)(1.0f, comboPopScale_ - deltaTime * 1.8f);
	}

	switch (currentScene_) {
	case GameScene::Title:
		if (camera_) {
			camera_->translation = { 7.5f, 3.5f, -15.0f };
		}
		if (input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN)) {
			currentScene_ = GameScene::Menu;
		}
		break;

	case GameScene::Menu:
		if (input->TriggerKey(DIK_W) || input->TriggerKey(DIK_UP)) {
			menuCursor_ = (menuCursor_ - 1 + 3) % 3;
		}
		if (input->TriggerKey(DIK_S) || input->TriggerKey(DIK_DOWN)) {
			menuCursor_ = (menuCursor_ + 1) % 3;
		}
		if (input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN)) {
			if (menuCursor_ == 0) {
				currentScene_ = GameScene::StageSelect;
			}
			else if (menuCursor_ == 1) {
				SelectStageAndPlay(0); // Tutorial
			}
			else {
				isRunning_ = false; // exit
			}
		}
		break;

	case GameScene::StageSelect:
		if (input->TriggerKey(DIK_W) || input->TriggerKey(DIK_UP)) {
			stageSelectCursor_ = (stageSelectCursor_ - 1 + 5) % 5;
		}
		if (input->TriggerKey(DIK_S) || input->TriggerKey(DIK_DOWN)) {
			stageSelectCursor_ = (stageSelectCursor_ + 1) % 5;
		}
		if (input->TriggerKey(DIK_ESCAPE)) {
			currentScene_ = GameScene::Menu;
		}
		if (input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN)) {
			if (stageSelectCursor_ < static_cast<int>(stages_.size())) {
				SelectStageAndPlay(stageSelectCursor_);
			}
			else {
				currentScene_ = GameScene::Menu;
			}
		}
		break;

	case GameScene::GamePlay:
		if (input->TriggerKey(DIK_ESCAPE)) {
			currentScene_ = GameScene::StageSelect;
			break;
		}

		// Camera mode: Tutorial = fixed zoomed-out view; Normal = dynamic player follow
		if (camera_) {
			bool isTut = (selectedStage_ >= 0 && selectedStage_ < static_cast<int>(stages_.size()) && stages_[selectedStage_].isTutorial);
			if (isTut) {
				Vector3 center = mapChip_.GetCenter();
				camera_->translation = { center.x, center.y + 0.5f, -20.5f };
			}
			else {
				const float camDist = 12.0f;
				const float viewHalfH = camDist * std::tan(camera_->fovY * 0.5f);
				const float viewHalfW = viewHalfH * camera_->aspectRatio;

				float targetCamX = player_.GetPosition().x + (player_.IsFacingRight() ? 1.5f : -1.5f);
				float targetCamY = player_.GetPosition().y + 2.0f;

				const float tile = mapChip_.GetTileSize();
				const float mapLeft = -tile * 0.5f;
				const float mapRight = mapChip_.GetWidth() - tile * 0.5f;
				const float minCamX = mapLeft + viewHalfW;
				const float maxCamX = mapRight - viewHalfW;
				if (maxCamX > minCamX) {
					targetCamX = std::clamp(targetCamX, minCamX, maxCamX);
				}
				else {
					targetCamX = (mapLeft + mapRight) * 0.5f; // map narrower than view: center it
				}
				targetCamY = std::clamp(targetCamY, 4.0f, mapChip_.GetHeight() + 2.0f);

				if (cameraSnap_) {
					camera_->translation.x = targetCamX;
					camera_->translation.y = targetCamY;
					cameraSnap_ = false;
				}
				else {
					const float k = 1.0f - std::exp(-8.0f * deltaTime);
					camera_->translation.x += (targetCamX - camera_->translation.x) * k;
					camera_->translation.y += (targetCamY - camera_->translation.y) * k;
				}
				camera_->translation.z = -camDist;
			}
		}

		// Update gameplay entities
		mapChip_.Update(deltaTime);
		player_.Update(deltaTime, &ball_);
		ball_.Update(deltaTime);
		particleSystem_.Update(deltaTime);

		for (auto& enemy : enemies_) {
			enemy->Update(deltaTime);
		}

		// Collision detection and resolution
		{
			auto* colMgr = CollisionManager::GetInstance();
			colMgr->Clear();

			mapChip_.RegisterColliders(colMgr);

			colMgr->RegisterCollider(&player_);
			if (player_.IsPunchHitBoxActive()) {
				colMgr->RegisterCollider(player_.GetPunchHitBox());
			}

			if (ball_.isAlive()) {
				colMgr->RegisterCollider(&ball_);
			}

			for (auto& enemy : enemies_) {
				if (enemy && !enemy->IsDead()) {
					colMgr->RegisterCollider(enemy.get());
				}
			}

			colMgr->CheckAllCollisions();
		}

		// Remove defeated enemies
		enemies_.erase(
			std::remove_if(enemies_.begin(), enemies_.end(),
				[](const std::unique_ptr<Enemy>& e) { return e->IsDead(); }),
			enemies_.end());

		// Player fall / death check
		if (player_.IsDead() || player_.GetPosition().y < -10.0f) {
			currentScene_ = GameScene::GameOver;
			endModalCursor_ = 0;
		}

		// Win condition: all spawned enemies defeated
		if (currentScene_ == GameScene::GamePlay && hasSpawnedEnemies_ && enemies_.empty()) {
			currentScene_ = GameScene::GameClear;
			endModalCursor_ = 0;
		}
		break;

	case GameScene::GameClear:
		if (input->TriggerKey(DIK_W) || input->TriggerKey(DIK_UP) ||
			input->TriggerKey(DIK_S) || input->TriggerKey(DIK_DOWN)) {
			endModalCursor_ = (endModalCursor_ == 0 ? 1 : 0);
		}
		if (input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN)) {
			if (endModalCursor_ == 0) {
				if (selectedStage_ + 1 < static_cast<int>(stages_.size())) {
					SelectStageAndPlay(selectedStage_ + 1);
				}
				else {
					currentScene_ = GameScene::StageSelect;
				}
			}
			else {
				currentScene_ = GameScene::StageSelect;
			}
		}
		if (input->TriggerKey(DIK_ESCAPE)) {
			currentScene_ = GameScene::StageSelect;
		}
		break;

	case GameScene::GameOver:
		if (input->TriggerKey(DIK_W) || input->TriggerKey(DIK_UP) ||
			input->TriggerKey(DIK_S) || input->TriggerKey(DIK_DOWN)) {
			endModalCursor_ = (endModalCursor_ == 0 ? 1 : 0);
		}
		if (input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN)) {
			if (endModalCursor_ == 0) {
				ResetGame(selectedStage_);
				currentScene_ = GameScene::GamePlay;
			}
			else {
				currentScene_ = GameScene::StageSelect;
			}
		}
		if (input->TriggerKey(DIK_ESCAPE)) {
			currentScene_ = GameScene::StageSelect;
		}
		break;
	}
}

// ============================================================================
// 3D Scene Rendering
// ============================================================================
void GameManager::Draw(ID3D12GraphicsCommandList* cmdList) {
	if (currentScene_ == GameScene::GamePlay ||
		currentScene_ == GameScene::GameClear ||
		currentScene_ == GameScene::GameOver)
	{
		// Draw SkyDome background
		if (skyDomeModel_) {
			if (camera_) {
				skyDomeObj_.worldTransform.translation = camera_->translation;
			}
			skyDomeObj_.Update(0.0f);
			skyDomeObj_.Draw(cmdList);
		}

		// Tiles must be updated every frame AFTER camera.Update() runs in main.cpp:
		// Object3d::Update bakes the camera's view-projection into each tile's matrix.
		mapChip_.Update(0.0f);
		mapChip_.Draw();

		for (auto& enemy : enemies_) {
			enemy->Draw();
		}
		player_.Draw();
		ball_.Draw();
		particleSystem_.Draw(cmdList);
	}
}

// ============================================================================
// 2D UI Rendering Entry Point (100% Native DirectX 12)
// ============================================================================
void GameManager::DrawUI(ID3D12GraphicsCommandList* cmdList) {
	if (!cmdList) {
		cmdList = DirectXCommon::GetInstance()->GetCommandList();
	}

	// Set 2D Sprite pipeline with Alpha Blending
	SpriteCommon::GetInstance()->PreDraw(cmdList, BlendMode::AlphaBlend);
	ResetSpritePool();

	switch (currentScene_) {
	case GameScene::Title:
		DrawTitleUI();
		break;
	case GameScene::Menu:
		DrawMenuUI();
		break;
	case GameScene::StageSelect:
		DrawStageSelectUI();
		break;
	case GameScene::GamePlay:
		DrawGamePlayUI();
		break;
	case GameScene::GameClear:
		DrawGamePlayUI();
		DrawGameClearUI();
		break;
	case GameScene::GameOver:
		DrawGamePlayUI();
		DrawGameOverUI();
		break;
	}
}

// ============================================================================
// Scene UI Implementations
// ============================================================================
void GameManager::DrawTitleUI() {
	// 1. Full-screen warm paper backdrop (Sumi-e / Calligraphy aesthetic)
	DrawRect(Vector2{ 0.0f, 0.0f }, Vector2{ 1280.0f, 720.0f }, Vector4{ 0.97f, 0.96f, 0.94f, 1.0f });

	// Floating black ink firefly particles
	DrawUIParticles();

	// Decorative top & bottom charcoal accent bars
	DrawRect(Vector2{ 0.0f, 0.0f }, Vector2{ 1280.0f, 4.0f }, Vector4{ 0.10f, 0.10f, 0.10f, 0.90f });
	DrawRect(Vector2{ 0.0f, 716.0f }, Vector2{ 1280.0f, 4.0f }, Vector4{ 0.10f, 0.10f, 0.10f, 0.90f });

	// 2. Title Logo Banner
	DrawUISprite(texTitleLogo_, kSprTitleLogo, Vector2{ 640.0f, 215.0f }, Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }, Vector2{ 0.5f, 0.5f }, 1.25f);

	// 3. Pulsing "Press Start" button slot
	float pulse = 0.55f + 0.45f * std::sin(uiTime_ * 4.5f);
	Vector2 startSlotPos{ 470.0f, 435.0f };
	Vector2 startSlotSize{ 340.0f, 54.0f };
	DrawRect(startSlotPos, startSlotSize, Vector4{ 0.88f, 0.85f, 0.78f, 1.0f });
	DrawRectOutline(startSlotPos, startSlotSize, Vector4{ 0.08f, 0.08f, 0.08f, pulse }, 3.0f);
	DrawRect(Vector2{ startSlotPos.x, startSlotPos.y }, Vector2{ 6.0f, startSlotSize.y }, Vector4{ 0.08f, 0.08f, 0.08f, pulse });
	DrawRect(Vector2{ startSlotPos.x + startSlotSize.x - 6.0f, startSlotPos.y }, Vector2{ 6.0f, startSlotSize.y }, Vector4{ 0.08f, 0.08f, 0.08f, pulse });

	DrawUISprite(texPressStart_, kSprPressStart, Vector2{ 640.0f, 462.0f }, Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }, Vector2{ 0.5f, 0.5f }, 0.95f);
}

void GameManager::DrawMenuUI() {
	// 1. Full-screen warm paper backdrop
	DrawRect(Vector2{ 0.0f, 0.0f }, Vector2{ 1280.0f, 720.0f }, Vector4{ 0.97f, 0.96f, 0.94f, 1.0f });

	// Floating black ink firefly particles
	DrawUIParticles();

	// Decorative top & bottom charcoal accent bars
	DrawRect(Vector2{ 0.0f, 0.0f }, Vector2{ 1280.0f, 4.0f }, Vector4{ 0.10f, 0.10f, 0.10f, 0.90f });
	DrawRect(Vector2{ 0.0f, 716.0f }, Vector2{ 1280.0f, 4.0f }, Vector4{ 0.10f, 0.10f, 0.10f, 0.90f });

	// 2. Framed Center Menu Card
	Vector2 cardPos{ 430.0f, 140.0f };
	Vector2 cardSize{ 420.0f, 400.0f };
	DrawRect(cardPos, cardSize, Vector4{ 0.99f, 0.98f, 0.97f, 0.98f });
	DrawRectOutline(cardPos, cardSize, Vector4{ 0.12f, 0.12f, 0.12f, 0.95f }, 2.5f);
	DrawRectOutline(Vector2{ cardPos.x + 4.0f, cardPos.y + 4.0f }, Vector2{ cardSize.x - 8.0f, cardSize.y - 8.0f }, Vector4{ 0.35f, 0.35f, 0.35f, 0.35f }, 1.0f);

	// Header banner: StageSelect header as title
	DrawUISprite(texStageSelect_, kSprStageSelectHeader, Vector2{ 640.0f, 185.0f }, Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }, Vector2{ 0.5f, 0.5f }, 0.95f);
	DrawRect(Vector2{ 470.0f, 220.0f }, Vector2{ 340.0f, 1.5f }, Vector4{ 0.15f, 0.15f, 0.15f, 0.70f });

	// 3. Three Menu Options:
	// 0 = Stage Select (texStageSelect_)
	// 1 = Tutorial (texTutorial_)
	// 2 = Exit (texBack_)
	for (int i = 0; i < 3; ++i) {
		float btnY = 245.0f + i * 80.0f;
		bool isSel = (menuCursor_ == i);
		Vector2 bPos{ 470.0f, btnY };
		Vector2 bSize{ 340.0f, 56.0f };

		if (isSel) {
			DrawRect(bPos, bSize, Vector4{ 0.88f, 0.85f, 0.78f, 1.0f });
			DrawRectOutline(bPos, bSize, Vector4{ 0.08f, 0.08f, 0.08f, 1.0f }, 3.0f);
			DrawRect(Vector2{ bPos.x, bPos.y }, Vector2{ 6.0f, bSize.y }, Vector4{ 0.08f, 0.08f, 0.08f, 1.0f });
			DrawRect(Vector2{ bPos.x + bSize.x - 6.0f, bPos.y }, Vector2{ 6.0f, bSize.y }, Vector4{ 0.08f, 0.08f, 0.08f, 1.0f });
		}
		else {
			DrawRect(bPos, bSize, Vector4{ 0.94f, 0.93f, 0.90f, 0.95f });
			DrawRectOutline(bPos, bSize, Vector4{ 0.25f, 0.25f, 0.25f, 0.65f }, 1.5f);
		}

		Vector2 spriteCenter{ bPos.x + bSize.x * 0.5f, btnY + bSize.y * 0.5f };
		if (i == 0) {
			DrawUISprite(texPlay_, kPlay, spriteCenter, Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }, Vector2{ 0.5f, 0.5f }, 0.85f);
		}
		else if (i == 1) {
			DrawUISprite(texTutorial_, kSprTutorialBtn, spriteCenter, Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }, Vector2{ 0.5f, 0.5f }, 0.95f);
		}
		else {
			DrawUISprite(texBack_, kSprBackBtn, spriteCenter, Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }, Vector2{ 0.5f, 0.5f }, 1.0f);
		}

		if (isSel) {
			float bounce = std::sin(uiTime_ * 7.0f) * 6.0f;
			DrawUISprite(texArrow_, kArrow, Vector2{ bPos.x - 30.0f + bounce, btnY + bSize.y * 0.5f }, Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }, Vector2{ 0.5f, 0.5f }, 0.95f);
		}
	}
}

void GameManager::DrawStageSelectUI() {
	// 1. Full-screen warm paper backdrop
	DrawRect(Vector2{ 0.0f, 0.0f }, Vector2{ 1280.0f, 720.0f }, Vector4{ 0.97f, 0.96f, 0.94f, 1.0f });

	// Floating black ink firefly particles
	DrawUIParticles();

	// Decorative top & bottom charcoal accent bars
	DrawRect(Vector2{ 0.0f, 0.0f }, Vector2{ 1280.0f, 4.0f }, Vector4{ 0.10f, 0.10f, 0.10f, 0.90f });
	DrawRect(Vector2{ 0.0f, 716.0f }, Vector2{ 1280.0f, 4.0f }, Vector4{ 0.10f, 0.10f, 0.10f, 0.90f });

	// 2. Top Header Banner
	DrawUISprite(texStageSelect_, kSprStageSelectHeader, Vector2{ 640.0f, 65.0f }, Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }, Vector2{ 0.5f, 0.5f }, 1.15f);
	DrawRect(Vector2{ 440.0f, 105.0f }, Vector2{ 400.0f, 2.0f }, Vector4{ 0.12f, 0.12f, 0.12f, 0.85f });

	// 3. Centered 5 Stage Selection Buttons
	// 0: Tutorial, 1: Stage 1, 2: Stage 2, 3: Stage 3, 4: Back
	const float startY = 160.0f;
	const float stepY = 72.0f;
	const Vector2 bSize{ 360.0f, 54.0f };
	const float btnX = (1280.0f - bSize.x) * 0.5f; // 460.0f

	for (int i = 0; i < 5; ++i) {
		float btnY = startY + i * stepY;
		bool isSel = (stageSelectCursor_ == i);
		Vector2 bPos{ btnX, btnY };

		if (isSel) {
			DrawRect(bPos, bSize, Vector4{ 0.88f, 0.85f, 0.78f, 1.0f });
			DrawRectOutline(bPos, bSize, Vector4{ 0.08f, 0.08f, 0.08f, 1.0f }, 3.0f);
			DrawRect(Vector2{ bPos.x, bPos.y }, Vector2{ 6.0f, bSize.y }, Vector4{ 0.08f, 0.08f, 0.08f, 1.0f });
			DrawRect(Vector2{ bPos.x + bSize.x - 6.0f, bPos.y }, Vector2{ 6.0f, bSize.y }, Vector4{ 0.08f, 0.08f, 0.08f, 1.0f });
		}
		else {
			DrawRect(bPos, bSize, Vector4{ 0.94f, 0.93f, 0.90f, 0.95f });
			DrawRectOutline(bPos, bSize, Vector4{ 0.25f, 0.25f, 0.25f, 0.65f }, 1.5f);
		}

		Vector2 sprCenter{ 640.0f, btnY + bSize.y * 0.5f };
		if (i == 0) {
			DrawUISprite(texTutorial_, kSprTutorialBtn, sprCenter, Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }, Vector2{ 0.5f, 0.5f }, 0.95f);
		}
		else if (i == 1) {
			DrawUISprite(texStage1_, kSprStage1Btn, sprCenter, Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }, Vector2{ 0.5f, 0.5f }, 1.0f);
		}
		else if (i == 2) {
			DrawUISprite(texStage2_, kSprStage2Btn, sprCenter, Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }, Vector2{ 0.5f, 0.5f }, 1.0f);
		}
		else if (i == 3) {
			DrawUISprite(texStage3_, kSprStage3Btn, sprCenter, Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }, Vector2{ 0.5f, 0.5f }, 1.0f);
		}
		else {
			DrawUISprite(texBack_, kSprBackBtn, sprCenter, Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }, Vector2{ 0.5f, 0.5f }, 1.05f);
		}

		if (isSel) {
			float bounce = std::sin(uiTime_ * 7.0f) * 6.0f;
			DrawUISprite(texArrow_, kArrow, Vector2{ bPos.x - 30.0f + bounce, btnY + bSize.y * 0.5f }, Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }, Vector2{ 0.5f, 0.5f }, 1.0f);
		}
	}
}

void GameManager::DrawGamePlayUI() {
	DrawPlayerHPBar();
	DrawEnemyCounterHUD();
	DrawComboHUD();
	DrawMinimap();
	DrawBallIndicator();

	if (selectedStage_ >= 0 && selectedStage_ < static_cast<int>(stages_.size()) && stages_[selectedStage_].isTutorial) {
		DrawTutorialOverlay();
	}
}

void GameManager::DrawPlayerHPBar() {
	float posX = 30.0f;
	float posY = 25.0f;
	float barW = 260.0f;
	float barH = 28.0f;

	float curHP = static_cast<float>(player_.GetHP());
	float maxHP = static_cast<float>((std::max)(1, player_.GetMaxHP()));
	float hpRatio = std::clamp(curHP / maxHP, 0.0f, 1.0f);

	// 1. Player Badge
	Vector2 badgePos{ posX, posY };
	Vector2 badgeSize{ 42.0f, 42.0f };
	DrawRect(badgePos, badgeSize, Vector4{ 0.97f, 0.96f, 0.94f, 0.95f });
	DrawRectOutline(badgePos, badgeSize, Vector4{ 0.12f, 0.12f, 0.12f, 0.95f }, 2.0f);
	DrawUISprite(texIconPlayer_, kSprIconPlayer, Vector2{ posX + 21.0f, posY + 21.0f }, Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }, Vector2{ 0.5f, 0.5f }, 1.05f);

	// 2. Framed HP Bar
	Vector2 barPos{ posX + 50.0f, posY + 7.0f };
	DrawRect(Vector2{ barPos.x - 3.0f, barPos.y - 3.0f }, Vector2{ barW + 6.0f, barH + 6.0f }, Vector4{ 0.97f, 0.96f, 0.94f, 0.95f });
	DrawRectOutline(Vector2{ barPos.x - 3.0f, barPos.y - 3.0f }, Vector2{ barW + 6.0f, barH + 6.0f }, Vector4{ 0.12f, 0.12f, 0.12f, 0.95f }, 2.0f);
	DrawRect(barPos, Vector2{ barW, barH }, Vector4{ 0.85f, 0.83f, 0.80f, 0.95f });

	// Filled HP: flat vermilion ink wash (hp_fill.png is pure red, which a tint cannot mute)
	if (hpRatio > 0.001f) {
		DrawRect(barPos, Vector2{ barW * hpRatio, barH }, Vector4{ 0.72f, 0.26f, 0.18f, 0.95f });
	}

	// 10 Segment Notch Lines
	for (int s = 1; s < 10; ++s) {
		DrawRect(Vector2{ barPos.x + s * (barW / 10.0f), barPos.y }, Vector2{ 1.5f, barH }, Vector4{ 0.15f, 0.15f, 0.15f, 0.55f });
	}

	// 3. Numeric HP Counter on the Right
	Vector2 numBoxPos{ barPos.x + barW + 12.0f, barPos.y };
	Vector2 numBoxSize{ 74.0f, barH };
	DrawRect(numBoxPos, numBoxSize, Vector4{ 0.97f, 0.96f, 0.94f, 0.95f });
	DrawRectOutline(numBoxPos, numBoxSize, Vector4{ 0.12f, 0.12f, 0.12f, 0.95f }, 1.5f);

	DrawNumberSprites(player_.GetHP(), Vector2{ numBoxPos.x + 20.0f, numBoxPos.y + 4.0f }, 20.0f, Vector4{ 0.1f, 0.1f, 0.1f, 1.0f }, Vector2{ 0.5f, 0.0f });
	DrawRect(Vector2{ numBoxPos.x + 35.0f, numBoxPos.y + 13.0f }, Vector2{ 4.0f, 4.0f }, Vector4{ 0.2f, 0.2f, 0.2f, 0.9f });
	DrawNumberSprites(player_.GetMaxHP(), Vector2{ numBoxPos.x + 54.0f, numBoxPos.y + 7.0f }, 15.0f, Vector4{ 0.35f, 0.35f, 0.35f, 0.9f }, Vector2{ 0.5f, 0.0f });
}

void GameManager::DrawEnemyCounterHUD() {
	float posX = 30.0f;
	float posY = 75.0f;
	Vector2 boxPos{ posX, posY };
	Vector2 boxSize{ 120.0f, 36.0f };

	DrawRect(boxPos, boxSize, Vector4{ 0.97f, 0.96f, 0.94f, 0.95f });
	DrawRectOutline(boxPos, boxSize, Vector4{ 0.12f, 0.12f, 0.12f, 0.95f }, 1.5f);

	// Enemy icon with subtle alert pulse
	float ep = 0.85f + 0.15f * std::sin(uiTime_ * 6.0f);
	DrawUISprite(texIconEnemy_, kSprIconEnemy, Vector2{ posX + 22.0f, posY + 18.0f }, Vector4{ 1.0f, 0.4f, 0.4f, ep }, Vector2{ 0.5f, 0.5f }, 0.95f);

	// Remaining Enemies Count in clean black ink
	int remaining = 0;
	for (const auto& e : enemies_) {
		if (e && !e->IsDead()) ++remaining;
	}
	DrawNumberSprites(remaining, Vector2{ posX + 52.0f, posY + 7.0f }, 22.0f, Vector4{ 0.1f, 0.1f, 0.1f, 1.0f }, Vector2{ 0.0f, 0.0f });
}

void GameManager::DrawComboHUD() {
	int curCombo = ball_.GetComboCount();
	float posX = 25.0f;
	float posY = 720.0f * 0.42f;

	float comboH = 44.0f * comboPopScale_;
	float comboW = (173.0f / 44.0f) * comboH;

	// Combo label sprite (combo.png)
	// Paper & ink: label toned down to a stamped vermilion, digits in sumi ink
	DrawUISprite(texCombo_, kSprComboLabel, Vector2{ posX, posY }, Vector4{ 0.80f, 0.55f, 0.50f, curCombo > 0 ? 1.0f : 0.65f }, Vector2{ 0.0f, 0.0f }, comboPopScale_);

	// Combo numeric digits
	DrawNumberSprites(curCombo, Vector2{ posX + comboW + 10.0f * comboPopScale_, posY }, comboH, Vector4{ 0.12f, 0.10f, 0.09f, curCombo > 0 ? 1.0f : 0.65f }, Vector2{ 0.0f, 0.0f });
}

void GameManager::DrawMinimap() {
	float mapW = 250.0f;
	float mapH = 145.0f;
	float posX = 1280.0f - mapW - 20.0f;
	float posY = 20.0f;

	if (!hasCustomMinimapFrame_) {
		// Fallback paper outer frame
		DrawRect(Vector2{ posX, posY }, Vector2{ mapW, mapH }, Vector4{ 0.97f, 0.96f, 0.94f, 0.95f });
		DrawRectOutline(Vector2{ posX, posY }, Vector2{ mapW, mapH }, Vector4{ 0.12f, 0.12f, 0.12f, 0.95f }, 1.5f);

		// Accent header status stripe & radar beacon dot
		DrawRect(Vector2{ posX + 8.0f, posY + 8.0f }, Vector2{ 36.0f, 3.0f }, Vector4{ 0.15f, 0.15f, 0.15f, 0.9f });
		float radarPulse = 0.5f + 0.5f * std::sin(uiTime_ * 5.0f);
		DrawRect(Vector2{ posX + mapW - 16.0f, posY + 7.0f }, Vector2{ 6.0f, 6.0f }, Vector4{ 0.1f, 0.8f, 0.3f, radarPulse });
	}

	// Inner map canvas (clean parchment)
	float cvsX = posX + 6.0f;
	float cvsY = posY + 26.0f;
	float cvsW = 238.0f;
	float cvsH = 113.0f;
	DrawRect(Vector2{ cvsX, cvsY }, Vector2{ cvsW, cvsH }, Vector4{ 0.98f, 0.98f, 0.96f, 0.95f });

	float worldW = (std::max)(1.0f, mapChip_.GetWidth());
	float worldH = (std::max)(1.0f, mapChip_.GetHeight());

	auto WorldToMinimap = [&](float wx, float wy) -> Vector2 {
		float u = std::clamp(wx / worldW, 0.0f, 1.0f);
		float v = std::clamp(1.0f - (wy / worldH), 0.0f, 1.0f);
		return Vector2{ cvsX + u * cvsW, cvsY + v * cvsH };
		};

	// 1. Solid terrain blocks (charcoal ink blocks)
	for (const auto& tile : mapChip_.GetTiles()) {
		Vector2 b0 = WorldToMinimap(tile.aabb.min.x, tile.aabb.max.y);
		Vector2 b1 = WorldToMinimap(tile.aabb.max.x, tile.aabb.min.y);
		DrawRect(b0, Vector2{ (std::max)(1.5f, b1.x - b0.x), (std::max)(1.5f, b1.y - b0.y) }, Vector4{ 0.22f, 0.22f, 0.22f, 0.9f });
	}

	// 2. Camera frustum
	if (camera_) {
		float camDist = std::abs(camera_->translation.z);
		float halfH = camDist * std::tan(camera_->fovY * 0.5f);
		float halfW = halfH * camera_->aspectRatio;
		Vector2 c0 = WorldToMinimap(camera_->translation.x - halfW, camera_->translation.y + halfH);
		Vector2 c1 = WorldToMinimap(camera_->translation.x + halfW, camera_->translation.y - halfH);
		DrawRectOutline(c0, Vector2{ c1.x - c0.x, c1.y - c0.y }, Vector4{ 0.12f, 0.12f, 0.12f, 0.65f }, 1.0f);
	}

	// 3. Enemies
	float eAlpha = 0.8f + 0.2f * std::sin(uiTime_ * 6.0f);
	for (const auto& enemy : enemies_) {
		if (enemy && !enemy->IsDead()) {
			Vector2 mPos = WorldToMinimap(enemy->GetPosition().x, enemy->GetPosition().y + 0.5f);
			DrawUISprite(texIconEnemy_, kSprIconEnemy, mPos, Vector4{ 1.0f, 0.3f, 0.3f, eAlpha }, Vector2{ 0.5f, 0.5f }, 0.45f);
		}
	}

	// 4. Ball
	if (ball_.isAlive()) {
		Vector2 bPos = WorldToMinimap(ball_.GetPosition().x, ball_.GetPosition().y);
		DrawUISprite(texIconBall_, kSprIconBall, bPos, Vector4{ 0.86f, 0.30f, 0.20f, 1.0f }, Vector2{ 0.5f, 0.5f }, 0.45f);
	}

	// 5. Player
	Vector2 pPos = WorldToMinimap(player_.GetPosition().x, player_.GetPosition().y + 0.5f);
	DrawUISprite(texIconPlayer_, kSprIconPlayer, pPos, Vector4{ 0.15f, 0.14f, 0.13f, 1.0f }, Vector2{ 0.5f, 0.5f }, 0.55f);

	// 6. Custom Minimap Frame Overlay (drawn on top)
	if (hasCustomMinimapFrame_ && texMinimapFrame_ != 0) {
		if (Sprite* frame = AcquireSprite(texMinimapFrame_)) {
			Vector2 nat = TextureManager::GetInstance()->GetTextureSize(texMinimapFrame_);
			if (nat.x > 0.0f && nat.y > 0.0f) {
				frame->SetTextureRect(Vector2{ 0.0f, 0.0f }, nat);
			}
			frame->SetAnchorPoint(Vector2{ 0.0f, 0.0f });
			frame->SetPosition(Vector2{ posX, posY });
			frame->SetSize(Vector2{ mapW, mapH });
			frame->SetColor(Vector4{ 1.0f, 1.0f, 1.0f, 1.0f });
			frame->Update();
			frame->Draw();
		}
	}
}

void GameManager::DrawBallIndicator() {
	if (!ball_.isAlive() || !camera_) return;

	Matrix4x4 vp = camera_->GetViewProjectionMatrix();
	Vector3 ndc = Mathematics::Transform(ball_.GetPosition(), vp);
	float sx = (ndc.x * 0.5f + 0.5f) * 1280.0f;
	float sy = (1.0f - (ndc.y * 0.5f + 0.5f)) * 720.0f;

	const float margin = 32.0f;
	bool isOffScreen = (sx < margin || sx > 1280.0f - margin || sy < margin || sy > 720.0f - margin || ndc.z < 0.0f);
	if (!isOffScreen) return;

	Vector2 pos{
		std::clamp(sx, margin, 1280.0f - margin),
		std::clamp(sy, margin, 720.0f - margin)
	};

	float alpha = 0.75f + 0.25f * std::sin(uiTime_ * 6.0f);

	// Indicator Badge (Sumi-e paper & ink)
	DrawRect(Vector2{ pos.x - 20.0f, pos.y - 20.0f }, Vector2{ 40.0f, 40.0f }, Vector4{ 0.97f, 0.96f, 0.94f, 0.95f });
	DrawRectOutline(Vector2{ pos.x - 20.0f, pos.y - 20.0f }, Vector2{ 40.0f, 40.0f }, Vector4{ 0.12f, 0.12f, 0.12f, alpha }, 2.0f);
	DrawUISprite(texIconBall_, kSprIconBall, pos, Vector4{ 0.86f, 0.30f, 0.20f, alpha }, Vector2{ 0.5f, 0.5f }, 0.95f);

	float dist = (ball_.GetPosition() - player_.GetPosition()).Length();
	DrawNumberSprites(static_cast<int>(std::round(dist)), Vector2{ pos.x, pos.y + 24.0f }, 15.0f, Vector4{ 0.1f, 0.1f, 0.1f, alpha }, Vector2{ 0.5f, 0.0f });
}

void GameManager::DrawTutorialOverlay() {
	float posX = 1280.0f - 240.0f - 20.0f;
	float posY = 180.0f;

	// Render the tutorial guide card sprite (complete calligraphy card image)
	DrawUISprite(texTutorialGuide_, kSprTutorialGuide, Vector2{ posX, posY }, Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }, Vector2{ 0.0f, 0.0f });
}

void GameManager::DrawGameClearUI() {
	// 1. Soft semi-transparent white paper screen wash
	DrawRect(Vector2{ 0.0f, 0.0f }, Vector2{ 1280.0f, 720.0f }, Vector4{ 0.95f, 0.94f, 0.92f, 0.70f });

	// 2. Solid white paper backing rect
	Vector2 modalPos{ 390.0f, 180.0f };
	Vector2 modalSize{ 500.0f, 340.0f };
	DrawRect(modalPos, modalSize, Vector4{ 0.98f, 0.98f, 0.96f, 0.98f });

	// 3. Stage Clear Modal Frame Sprite (500x340)
	if (texStageClear_ != 0) {
		if (Sprite* sp = AcquireSprite(texStageClear_)) {
			Vector2 nat = TextureManager::GetInstance()->GetTextureSize(texStageClear_);
			if (nat.x > 0.0f && nat.y > 0.0f) {
				sp->SetTextureRect(Vector2{ 0.0f, 0.0f }, nat);
			}
			sp->SetAnchorPoint(Vector2{ 0.0f, 0.0f });
			sp->SetPosition(modalPos);
			sp->SetSize(modalSize);
			sp->SetColor(Vector4{ 1.0f, 1.0f, 1.0f, 1.0f });
			sp->Update();
			sp->Draw();
		}
	}

	// 4. Two Interactive Option Buttons in the empty lower half:
	// Option 0: Next Stage / Retry
	// Option 1: Stage Select (texBack_)
	for (int i = 0; i < 2; ++i) {
		float btnY = 325.0f + i * 78.0f;
		bool isSel = (endModalCursor_ == i);
		Vector2 bPos{ 460.0f, btnY };
		Vector2 bSize{ 360.0f, 54.0f };

		if (isSel) {
			DrawRect(bPos, bSize, Vector4{ 0.88f, 0.85f, 0.78f, 1.0f });
			DrawRectOutline(bPos, bSize, Vector4{ 0.08f, 0.08f, 0.08f, 1.0f }, 3.0f);
			DrawRect(Vector2{ bPos.x, bPos.y }, Vector2{ 6.0f, bSize.y }, Vector4{ 0.08f, 0.08f, 0.08f, 1.0f });
			DrawRect(Vector2{ bPos.x + bSize.x - 6.0f, bPos.y }, Vector2{ 6.0f, bSize.y }, Vector4{ 0.08f, 0.08f, 0.08f, 1.0f });
		}
		else {
			DrawRect(bPos, bSize, Vector4{ 0.94f, 0.93f, 0.90f, 0.95f });
			DrawRectOutline(bPos, bSize, Vector4{ 0.25f, 0.25f, 0.25f, 0.65f }, 1.5f);
		}

		Vector2 sprCenter{ bPos.x + bSize.x * 0.5f, btnY + bSize.y * 0.5f };
		if (i == 0) {
			DrawUISprite(texPressStart_, kSprPressStart, sprCenter, Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }, Vector2{ 0.5f, 0.5f }, 0.95f);
		}
		else {
			DrawUISprite(texBack_, kSprBackBtn, sprCenter, Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }, Vector2{ 0.5f, 0.5f }, 1.0f);
		}

		if (isSel) {
			float bounce = std::sin(uiTime_ * 7.0f) * 6.0f;
			DrawUISprite(texArrow_, kArrow, Vector2{ bPos.x - 26.0f + bounce, btnY + bSize.y * 0.5f }, Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }, Vector2{ 0.5f, 0.5f }, 0.95f);
		}
	}
}

void GameManager::DrawGameOverUI() {
	// 1. Soft semi-transparent white paper screen wash
	DrawRect(Vector2{ 0.0f, 0.0f }, Vector2{ 1280.0f, 720.0f }, Vector4{ 0.95f, 0.94f, 0.92f, 0.70f });

	// 2. Solid white paper backing rect
	Vector2 modalPos{ 390.0f, 180.0f };
	Vector2 modalSize{ 500.0f, 340.0f };
	DrawRect(modalPos, modalSize, Vector4{ 0.98f, 0.98f, 0.96f, 0.98f });

	// 3. Game Over Modal Frame Sprite (500x340)
	if (texGameOver_ != 0) {
		if (Sprite* sp = AcquireSprite(texGameOver_)) {
			Vector2 nat = TextureManager::GetInstance()->GetTextureSize(texGameOver_);
			if (nat.x > 0.0f && nat.y > 0.0f) {
				sp->SetTextureRect(Vector2{ 0.0f, 0.0f }, nat);
			}
			sp->SetAnchorPoint(Vector2{ 0.0f, 0.0f });
			sp->SetPosition(modalPos);
			sp->SetSize(modalSize);
			sp->SetColor(Vector4{ 1.0f, 1.0f, 1.0f, 1.0f });
			sp->Update();
			sp->Draw();
		}
	}

	// 4. Two Interactive Option Buttons in the empty lower half:
	// Option 0: Retry Stage (texPressStart_)
	// Option 1: Stage Select (texBack_)
	for (int i = 0; i < 2; ++i) {
		float btnY = 325.0f + i * 78.0f;
		bool isSel = (endModalCursor_ == i);
		Vector2 bPos{ 460.0f, btnY };
		Vector2 bSize{ 360.0f, 54.0f };

		if (isSel) {
			DrawRect(bPos, bSize, Vector4{ 0.88f, 0.85f, 0.78f, 1.0f });
			DrawRectOutline(bPos, bSize, Vector4{ 0.08f, 0.08f, 0.08f, 1.0f }, 3.0f);
			DrawRect(Vector2{ bPos.x, bPos.y }, Vector2{ 6.0f, bSize.y }, Vector4{ 0.08f, 0.08f, 0.08f, 1.0f });
			DrawRect(Vector2{ bPos.x + bSize.x - 6.0f, bPos.y }, Vector2{ 6.0f, bSize.y }, Vector4{ 0.08f, 0.08f, 0.08f, 1.0f });
		}
		else {
			DrawRect(bPos, bSize, Vector4{ 0.94f, 0.93f, 0.90f, 0.95f });
			DrawRectOutline(bPos, bSize, Vector4{ 0.25f, 0.25f, 0.25f, 0.65f }, 1.5f);
		}

		Vector2 sprCenter{ bPos.x + bSize.x * 0.5f, btnY + bSize.y * 0.5f };
		if (i == 0) {
			DrawUISprite(texPressStart_, kSprPressStart, sprCenter, Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }, Vector2{ 0.5f, 0.5f }, 0.95f);
		}
		else {
			DrawUISprite(texBack_, kSprBackBtn, sprCenter, Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }, Vector2{ 0.5f, 0.5f }, 1.0f);
		}

		if (isSel) {
			float bounce = std::sin(uiTime_ * 7.0f) * 6.0f;
			DrawUISprite(texArrow_, kArrow, Vector2{ bPos.x - 26.0f + bounce, btnY + bSize.y * 0.5f }, Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }, Vector2{ 0.5f, 0.5f }, 0.95f);
		}
	}
}
