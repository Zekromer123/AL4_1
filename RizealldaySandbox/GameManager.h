#pragma once
#include <d3d12.h>
#include <memory>
#include <vector>
#include <array>
#include <unordered_map>
#include <string>
#include "3d/Camera.h"
#include "3d/Model.h"
#include "3d/Object3d.h"
#include "2d/Sprite.h"
#include "Player.h"
#include "Ball.h"
#include "Enemy.h"
#include "MapChip.h"
#include "ParticleSystem.h"
#include "audio/Audio.h"

enum class GameScene {
    Title,
    Menu,
    StageSelect,
    GamePlay,
    GameClear,
    GameOver
};

struct StageInfo {
    std::string name;
    std::string subtitle;
    std::string csvPath;
    std::string description;
    bool isTutorial = false;
};

/// <summary>
/// Centralized replaceable UI sprite specification.
/// Allows swapping textures and adjusting crop rectangle / display size easily.
/// </summary>
struct UISpriteSpec {
    const char* filePath;
    Vector2 displaySize;
    bool useCrop;
    Vector2 cropPos;
    Vector2 cropSize;
};

/// <summary>
/// Lightweight struct for 2D UI Sumi-e black ink firefly particles.
/// </summary>
struct UIInkParticle {
    Vector2 pos;
    float speedY;
    float wavePhase;
    float waveAmp;
    float waveFreq;
    float size;
    float maxAlpha;
};

class GameManager {
public:
    GameManager();
    ~GameManager();

    void Initialize(Model* playerModel, Model* cubeModel, Model* ballModel, Camera* camera, Model* skyDomeModel = nullptr);
    void Update(float deltaTime);
    void Draw(ID3D12GraphicsCommandList* cmdList);

    /// UI (100% Native DirectX 12 2D Rendering - Zero ImGui, Zero DebugText)
    void DrawUI(ID3D12GraphicsCommandList* cmdList = nullptr);

    bool IsRunning() const { return isRunning_; }
    void SelectStageAndPlay(int stageIndex);

private:
    void ResetGame(int stageIndex = -1);

    // Native 2D Drawing Helpers
    void DrawRect(const Vector2& pos, const Vector2& size, const Vector4& color);
    void DrawRectOutline(const Vector2& pos, const Vector2& size, const Vector4& color, float border = 1.0f);
    void DrawUISprite(uint32_t texHandle, const UISpriteSpec& spec, const Vector2& pos,
                      const Vector4& color = Vector4{ 1.0f, 1.0f, 1.0f, 1.0f },
                      const Vector2& anchor = Vector2{ 0.0f, 0.0f }, float scale = 1.0f);
    void DrawNumberSprites(int value, const Vector2& pos, float digitHeight,
                           const Vector4& color = Vector4{ 1.0f, 1.0f, 1.0f, 1.0f },
                           const Vector2& anchor = Vector2{ 0.0f, 0.0f });

    // Scene UI Renderers
    void DrawTitleUI();
    void DrawMenuUI();
    void DrawStageSelectUI();
    void DrawGamePlayUI();
    void DrawGameClearUI();
    void DrawGameOverUI();

    // Floating 2D UI Ink Particles (Title, Menu, StageSelect)
    void UpdateUIParticles(float deltaTime);
    void DrawUIParticles();

    // In-game HUD elements
    void DrawBallIndicator();
    void DrawTutorialOverlay();
    void DrawMinimap();
    void DrawComboHUD();
    void DrawPlayerHPBar();
    void DrawEnemyCounterHUD();

    Model* playerModel_ = nullptr;
    Model* cubeModel_ = nullptr;
    Model* ballModel_ = nullptr;
    Model* skyDomeModel_ = nullptr;
    Model* darumaModel_ = nullptr;  // ground enemy
    Model* lanternModel_ = nullptr; // flying enemy
    Camera* camera_ = nullptr;

    Object3d skyDomeObj_;
    Player player_;
    Ball ball_;
    ParticleSystem particleSystem_;
    std::vector<std::unique_ptr<Enemy>> enemies_;
    MapChip mapChip_;

    GameScene currentScene_ = GameScene::Title;
    bool hasSpawnedEnemies_ = false;
    int menuCursor_ = 0;             // 0 = Stage Select, 1 = Tutorial, 2 = Exit
    int stageSelectCursor_ = 0;      // 0..stages_.size()-1
    int endModalCursor_ = 0;         // 0 = Next / Retry, 1 = Stage Select
    bool isRunning_ = true;

    std::vector<StageInfo> stages_;
    int selectedStage_ = 0;

    // UI Texture handles
    uint32_t texWhite_ = 0;
    uint32_t texStageSelect_ = 0;
    uint32_t texPlay_ = 0;
    uint32_t texArrow_ = 0;
    uint32_t texTutorial_ = 0;
    uint32_t texStage1_ = 0;
    uint32_t texStage2_ = 0;
    uint32_t texStage3_ = 0;
    uint32_t texBack_ = 0;
    uint32_t texCombo_ = 0;
    uint32_t texHpFill_ = 0;
    uint32_t texIconBall_ = 0;
    uint32_t texIconEnemy_ = 0;
    uint32_t texIconPlayer_ = 0;
    uint32_t texDigits_[10] = {};

    // Replaceable UI textures (with safe fallbacks)
    uint32_t texTitleLogo_ = 0;
    uint32_t texPressStart_ = 0;
    uint32_t texStageClear_ = 0;
    uint32_t texGameOver_ = 0;
    uint32_t texTutorialGuide_ = 0;
    uint32_t texMinimapFrame_ = 0;
    bool hasCustomMinimapFrame_ = false;

    // Floating 2D UI Ink Firefly Particles
    uint32_t texCircle_ = 0;
    std::array<UIInkParticle, 45> uiInkParticles_{};

    // Sprite pool for per-draw CBV safety:
    // A Sprite owns ONE constant buffer. Drawing multiple sprites in a frame requires
    // each draw call to have its own Sprite instance.
    Sprite* AcquireSprite(uint32_t texture);
    void ResetSpritePool();
    std::unordered_map<uint32_t, std::vector<Sprite*>> spritePool_;
    std::unordered_map<uint32_t, size_t> spritePoolUsed_;

    bool cameraSnap_ = true;

    float uiTime_ = 0.0f;
    float comboPopScale_ = 1.0f;
    int lastComboCount_ = 0;

    // Audio Assets
    Audio::SoundData bgmSound_{};
    Audio::SoundData bounceSound_{};
};
