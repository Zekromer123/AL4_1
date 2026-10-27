#include <RizeEngine.h>
#include <chrono>
#include <algorithm>
#include "GameManager.h"
#include "3d/PrimitiveDrawer.h"
#include "3d/CollisionManager.h"


/// Entry point for Windows application (main function)
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

	// ===== Initialize Engine =====
	RizeEngine::Initialize("LE2B_31_リザルデイ_BoingBoing", 1280, 720);

	// ===== Setup Camera =====
	Camera camera;
	camera.Initialize();
	camera.translation = { 0.0f, 2.5f, -12.0f };
	camera.rotation = { 0.15f, 0.0f, 0.0f };

	// ===== Lighting Setup (Papercraft Studio / Desk Lamp Illumination) =====
	LightGroup* lights = LightGroup::Create();
	lights->SetDefaultLighting();
	lights->SetAmbientColor({ 0.95f, 0.92f, 0.88f }); // Warm parchment ambient reflection
	lights->SetAmbientIntensity(0.65f);               // Soft translucent ambient
	lights->GetDirectionalLight().direction = { 0.30f, -0.85f, 0.45f }; // Overhead desk lamp angle
	lights->GetDirectionalLight().color = { 1.0f, 0.98f, 0.94f, 1.0f }; // Warm studio light
	lights->GetDirectionalLight().intensity = 0.95f;

	// Player model (slime). Put player.obj, player.mtl and slimeTex.png together in Resources/Player/
	Model* playerModel = Model::CreateFromFile("Resources/Player", "player.obj");
	Model* cubeModel = Model::CreateFromFile("Resources","cube.obj");
	//Ball Model
	Model* ballModel = Model::CreateFromFile("Resources", "ball.obj");
	//SkyDome Model
	Model* skyDomeModel = Model::CreateFromFile("Resources/SkyDome", "SkyDome.obj");

	//Game Manager
	GameManager gameManager;
	gameManager.Initialize(playerModel, cubeModel, ballModel, &camera, skyDomeModel);

	auto lastTime = std::chrono::high_resolution_clock::now();

	// ===== Game Loop =====
	while (RizeEngine::ProcessMessage() && gameManager.IsRunning()) {
		RizeEngine::BeginFrame();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
		lastTime = currentTime;
		deltaTime = std::clamp(deltaTime, 0.0001f, 0.1f);


		gameManager.Update(deltaTime);
		
		// Update Camera
		camera.Update();
		lights->Update();

		
		// --- Render ---
		auto* cmdList = RizeEngine::GetDirectXCommon()->GetCommandList();
		lights->Draw(cmdList, 3);
		gameManager.Draw(cmdList);
		gameManager.DrawUI(cmdList);
		
		RizeEngine::EndFrame();
	}

	// ===== Finalize =====
	RizeEngine::Finalize();
	return 0;
}
