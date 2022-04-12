#include "Application/Application.h"

#include <Windows.h>
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include "Logging.h"
#include "Gameplay/InputEngine.h"
#include "Application/Timing.h"
#include <filesystem>
#include "Layers/GLAppLayer.h"
#include "Utils/FileHelpers.h"
#include "Utils/ResourceManager/ResourceManager.h"
#include "Utils/ImGuiHelper.h"

// Graphics
#include "Graphics/Buffers/IndexBuffer.h"
#include "Graphics/Buffers/VertexBuffer.h"
#include "Graphics/VertexArrayObject.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/Textures/Texture1D.h"
#include "Graphics/Textures/Texture2D.h"
#include "Graphics/Textures/Texture3D.h"
#include "Graphics/Textures/TextureCube.h"
#include "Graphics/VertexTypes.h"
#include "Graphics/Font.h"
#include "Graphics/GuiBatcher.h"
#include "Graphics/Framebuffer.h"

// Gameplay
#include "Gameplay/Material.h"
#include "Gameplay/GameObject.h"
#include "Gameplay/Scene.h"

// Components
#include "Gameplay/Components/IComponent.h"
#include "Gameplay/Components/Camera.h"
#include "Gameplay/Components/RotatingBehaviour.h"
#include "Gameplay/Components/JumpBehaviour.h"
#include "Gameplay/Components/RenderComponent.h"
#include "Gameplay/Components/MaterialSwapBehaviour.h"
#include "Gameplay/Components/TriggerVolumeEnterBehaviour.h"
#include "Gameplay/Components/SimpleCameraControl.h"
#include "Gameplay/Components/ParticleSystem.h"
#include "Gameplay/Components/Light.h"
#include "Gameplay/Components/ShadowCamera.h"

// GUI
#include "Gameplay/Components/GUI/RectTransform.h"
#include "Gameplay/Components/GUI/GuiPanel.h"
#include "Gameplay/Components/GUI/GuiText.h"
#include "Gameplay/Components/ComponentManager.h"

// Layers
#include "Layers/RenderLayer.h"
#include "Layers/InterfaceLayer.h"
#include "Layers/DefaultSceneLayer.h"
#include "Layers/LogicUpdateLayer.h"
#include "Layers/ImGuiDebugLayer.h"
#include "Layers/InstancedRenderingTestLayer.h"
#include "Layers/ParticleLayer.h"
#include "Layers/PostProcessingLayer.h"

Application* Application::_singleton = nullptr;
std::string Application::_applicationName = "INFR-2350U - DEMO";

#define DEFAULT_WINDOW_WIDTH 1280
#define DEFAULT_WINDOW_HEIGHT 720

Application::Application() :
	_window(nullptr),
	_windowSize({DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT}),
	_isRunning(false),
	_isEditor(true),
	_windowTitle("INFR - 2350U"),
	_currentScene(nullptr),
	_targetScene(nullptr)
{ }

Application::~Application() = default; 

Application& Application::Get() {
	LOG_ASSERT(_singleton != nullptr, "Failed to get application! Get was called before the application was started!");
	return *_singleton;
}

void Application::Start(int argCount, char** arguments) {
	LOG_ASSERT(_singleton == nullptr, "Application has already been started!");
	_singleton = new Application();
	_singleton->_Run();
}

GLFWwindow* Application::GetWindow() { return _window; }

const glm::ivec2& Application::GetWindowSize() const { return _windowSize; }


const glm::uvec4& Application::GetPrimaryViewport() const {
	return _primaryViewport;
}

void Application::SetPrimaryViewport(const glm::uvec4& value) {
	_primaryViewport = value;
}

void Application::ResizeWindow(const glm::ivec2& newSize)
{
	_HandleWindowSizeChanged(newSize);
}

void Application::Quit() {
	_isRunning = false;
}

bool Application::LoadScene(const std::string& path) {
	if (std::filesystem::exists(path)) { 

		std::string manifestPath = std::filesystem::path(path).stem().string() + "-manifest.json";
		if (std::filesystem::exists(manifestPath)) {
			LOG_INFO("Loading manifest from \"{}\"", manifestPath);
			ResourceManager::LoadManifest(manifestPath);
		}

		Gameplay::Scene::Sptr scene = Gameplay::Scene::Load(path);
		LoadScene(scene);
		return scene != nullptr;
	}
	return false;
}

void Application::LoadScene(const Gameplay::Scene::Sptr& scene) {
	_targetScene = scene;
}

void Application::SaveSettings()
{
	std::filesystem::path appdata = getenv("APPDATA");
	std::filesystem::path settingsPath = appdata / _applicationName / "app-settings.json";

	if (!std::filesystem::exists(appdata / _applicationName)) {
		std::filesystem::create_directory(appdata / _applicationName);
	}

	FileHelpers::WriteContentsToFile(settingsPath.string(), _appSettings.dump(1, '\t'));
}

void Application::_Run()
{
	// TODO: Register layers
	_layers.push_back(std::make_shared<GLAppLayer>());
	_layers.push_back(std::make_shared<LogicUpdateLayer>());
	_layers.push_back(std::make_shared<RenderLayer>());
	_layers.push_back(std::make_shared<ParticleLayer>());
	_layers.push_back(std::make_shared<PostProcessingLayer>());
	_layers.push_back(std::make_shared<InterfaceLayer>());

	// If we're in editor mode, we add all the editor layers
	if (_isEditor) {
		_layers.push_back(std::make_shared<ImGuiDebugLayer>());
	}

	_layers.push_back(std::make_shared<DefaultSceneLayer>());

	// Either load the settings, or use the defaults
	_ConfigureSettings();

	// We'll grab these since we'll need them!
	_windowSize.x = JsonGet(_appSettings, "window_width", DEFAULT_WINDOW_WIDTH);
	_windowSize.y = JsonGet(_appSettings, "window_height", DEFAULT_WINDOW_HEIGHT);

	// By default, we want our viewport to be the whole screen
	_primaryViewport = { 0, 0, _windowSize.x, _windowSize.y };

	// Register all component and resource types
	_RegisterClasses();


	// Load all layers
	_Load();

	// Grab current time as the previous frame
	double lastFrame =  glfwGetTime();

	// Done loading, app is now running!
	_isRunning = true;

	// Infinite loop as long as the application is running
	while (_isRunning) {
		// Handle scene switching
		if (_targetScene != nullptr) {
			_HandleSceneChange();
		}

		// Receive events like input and window position/size changes from GLFW
		glfwPollEvents();

		// Handle closing the app via the close button
		if (glfwWindowShouldClose(_window)) {
			_isRunning = false;
		}

		// Grab the timing singleton instance as a reference
		Timing& timing = Timing::_singleton;

		// Figure out the current time, and the time since the last frame
		double thisFrame = glfwGetTime();
		float dt = static_cast<float>(thisFrame - lastFrame);
		float scaledDt = dt * timing._timeScale;

		// Update all timing values
		timing._unscaledDeltaTime = dt;
		timing._deltaTime = scaledDt;
		timing._timeSinceAppLoad += scaledDt;
		timing._unscaledTimeSinceAppLoad += dt;
		timing._timeSinceSceneLoad += scaledDt;
		timing._unscaledTimeSinceSceneLoad += dt;

		ImGuiHelper::StartFrame();

		// Core update loop
		if (_currentScene != nullptr) {
			_Update();
			_LateUpdate();
			_PreRender();
			_RenderScene(); 
			_PostRender();
		}

		

		// Store timing for next loop
		lastFrame = thisFrame;

		InputEngine::EndFrame();
		ImGuiHelper::EndFrame();

		glfwSwapBuffers(_window);

	}

	// Unload all our layers
	_Unload();
}

void Application::_RegisterClasses()
{
	using namespace Gameplay;
	using namespace Gameplay::Physics;

	// Initialize our resource manager
	ResourceManager::Init();

	// Register all our resource types so we can load them from manifest files
	ResourceManager::RegisterType<Texture1D>();
	ResourceManager::RegisterType<Texture2D>();
	ResourceManager::RegisterType<Texture3D>();
	ResourceManager::RegisterType<TextureCube>();
	ResourceManager::RegisterType<ShaderProgram>();
	ResourceManager::RegisterType<Material>();
	ResourceManager::RegisterType<MeshResource>();
	ResourceManager::RegisterType<Font>();
	ResourceManager::RegisterType<Framebuffer>();

	// Register all of our component types so we can load them from files
	ComponentManager::RegisterType<Camera>();
	ComponentManager::RegisterType<RenderComponent>();
	ComponentManager::RegisterType<RigidBody>();
	ComponentManager::RegisterType<TriggerVolume>();
	ComponentManager::RegisterType<RotatingBehaviour>();
	ComponentManager::RegisterType<JumpBehaviour>();
	ComponentManager::RegisterType<MaterialSwapBehaviour>();
	ComponentManager::RegisterType<TriggerVolumeEnterBehaviour>();
	ComponentManager::RegisterType<SimpleCameraControl>();
	ComponentManager::RegisterType<RectTransform>();
	ComponentManager::RegisterType<GuiPanel>();
	ComponentManager::RegisterType<GuiText>();
	ComponentManager::RegisterType<ParticleSystem>();
	ComponentManager::RegisterType<Light>();
	ComponentManager::RegisterType<ShadowCamera>();
}

void Application::_Load() {
	for (const auto& layer : _layers) {
		if (layer->Enabled && *(layer->Overrides & AppLayerFunctions::OnAppLoad)) {
			layer->OnAppLoad(_appSettings);
		}
	}

	// Pass the window to the input engine and let it initialize itself
	InputEngine::Init(_window);
	
	// Initialize our ImGui helper
	ImGuiHelper::Init(_window);

	GuiBatcher::SetWindowSize(_windowSize);
}

float JTime = 0;
float JTemp = 0;
float FTime = 0;
float FTemp = 0;
float RTime = 0;
float RTemp = 0;
bool jumpo = false;
float jumpheight = 0;
float x = 0;
bool playerFlying = false;

void Application::_Update() {
	for (const auto& layer : _layers) {
		if (layer->Enabled && *(layer->Overrides & AppLayerFunctions::OnUpdate)) {
			layer->OnUpdate();
		}
	}

	//Game Loop
	Application& app = Application::Get();

	//Jump Code 
	{
		if ((InputEngine::GetKeyState(GLFW_KEY_SPACE) == ButtonState::Pressed)) {
			jumpo = true;
		}
		if (jumpo == true) {
			JTime = glfwGetTime() - JTemp;
			JTime = JTime / 2.5;

			app.CurrentScene()->FindObjectByName("ladybug")->SetPostion(glm::vec3(app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().x, app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().y, jumpheight));
		}
		else {
			JTemp = glfwGetTime();
		}

		x = JTime * 8; //Multiply to increase speed of jump
		std::cout << JTime << "\n";

		//parabola function so the jump will slow as it reaches the max height
		jumpheight = (-4 * pow(x - 1, 2) + 4) + 1; //0.2 is currently the ladybugs starting point on z

		if (jumpheight < 0) { //so the ladybug doesnt go through the ground
			jumpo = false;
		}
	}

	//Fly Code
	{

		if ((InputEngine::GetKeyState(GLFW_KEY_UP) == ButtonState::Pressed)) {
			playerFlying = true;
			jumpo = false;
		}

		if (playerFlying == true) {
			FTime = glfwGetTime() - FTemp;
			FTime = (FTime / 2.5) * 8;

			if (app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().z < 10.1) {
				//scene->FindObjectByName("player")->SetPostion(glm::vec3(scene->FindObjectByName("player")->GetPosition().x, scene->FindObjectByName("player")->GetPosition().y, (scene->FindObjectByName("player")->GetPosition().z) + 1.0));
				app.CurrentScene()->FindObjectByName("ladybug")->SetPostion(glm::vec3(app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().x, app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().y, app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().z + 1));
			}
			//isJumpPressed = true;
			//playerJumping = false;
		}
		else {
			FTemp = glfwGetTime();
		}

		if (FTime > 9) {
			playerFlying = false;
			if (app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().z > 1) {
				app.CurrentScene()->FindObjectByName("ladybug")->SetPostion(glm::vec3(app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().x, app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().y, app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().z - 1));
			}
		}
		//useLERP(FTime)
		std::cout << FTime << "\n";
	}

	//Slide Code
	if ((InputEngine::GetKeyState(GLFW_KEY_DOWN) == ButtonState::Down)) {
		app.CurrentScene()->FindObjectByName("ladybug")->SetScale(glm::vec3(0.5f, 0.15f, 0.5f));
	}
	else {
		app.CurrentScene()->FindObjectByName("ladybug")->SetScale(glm::vec3(0.3f, 0.3f, 0.3f));
	}

	//Light Toggle
	if ((InputEngine::GetKeyState(GLFW_KEY_1) == ButtonState::Down)) {
		for (int i = 0; i < app.CurrentScene()->FindObjectByName("Lights")->GetChildren().size(); i = i + 1) {
			app.CurrentScene()->FindObjectByName("Lights")->GetChildren()[i]->Get<Light>()->SetIntensity(0.f);
		}
	}
	else {
		for (int i = 0; i < app.CurrentScene()->FindObjectByName("Lights")->GetChildren().size(); i = i + 1) {
			if (i <= 10) {
				app.CurrentScene()->FindObjectByName("Lights")->GetChildren()[i]->Get<Light>()->SetIntensity(250.f);
			}
			else if (i <= 21) {
				app.CurrentScene()->FindObjectByName("Lights")->GetChildren()[i]->Get<Light>()->SetIntensity(500.f);
			}
			else if (i <= 30) {
				app.CurrentScene()->FindObjectByName("Lights")->GetChildren()[i]->Get<Light>()->SetIntensity(1.f);
			}
		}
	}

	if ((InputEngine::GetKeyState(GLFW_KEY_P) == ButtonState::Down)) {
		if (paused == true)
		{
			paused = false;
		}
		else if (paused == false)
		{
			paused = true;
		}
	}

	if (paused == true)
	{
		playermove = false;
	}

	if (paused == false)
	{
		playermove = true;
	}

	if (playermove == true)
	{
		app.CurrentScene()->FindObjectByName("ladybug")->SetPostion(glm::vec3(app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().x - 0.4f, app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().y, app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().z));
	}

	if ((InputEngine::GetKeyState(GLFW_KEY_Y) == ButtonState::Down))
	{
		if (followplayer == true)
		{
			followplayer = false;
		}
		else if (followplayer == false)
		{
			followplayer = true;
		}
	}

	if (app.CurrentScene()->FindObjectByName("ladybug") != nullptr && followplayer == true)
	{
		app.CurrentScene()->FindObjectByName("Main Camera")->SetPostion(glm::vec3(app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().x - 5, 11.480, 6.290));
		app.CurrentScene()->FindObjectByName("Main Camera")->SetRotation(glm::vec3(84, 0, -180));
	}

	if (app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().x < -406.f)
	{
		winner = true;
	}

	if (winner == true)
	{
		paused = true;
		app.CurrentScene()->FindObjectByName("ladybug")->SetPostion(glm::vec3(0.f, 0.f, 0.f));
		winner = false;
	}

	if (collision == true)
	{
		paused = true;
		app.CurrentScene()->FindObjectByName("deadbug")->SetPostion(glm::vec3(app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().x, app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().y + 2, app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().z));
		app.CurrentScene()->FindObjectByName("ladybug")->SetPostion(glm::vec3(0.f, 0.f, 0.f));
		collision = false;
	}

	app.CurrentScene()->FindObjectByName("Particles")->SetPostion(glm::vec3(app.CurrentScene()->FindObjectByName("deadbug")->GetPosition().x, app.CurrentScene()->FindObjectByName("deadbug")->GetPosition().y, app.CurrentScene()->FindObjectByName("deadbug")->GetPosition().z));

	app.CurrentScene()->FindObjectByName("Shadow Light")->SetPostion(glm::vec3(app.CurrentScene()->FindObjectByName("Main Camera")->GetPosition().x - 15, app.CurrentScene()->FindObjectByName("Main Camera")->GetPosition().y, app.CurrentScene()->FindObjectByName("Main Camera")->GetPosition().z + 15));

	if (playerlose == false)
	{
		if (app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().z < 1.56)
		{
			//mushroom 1
			if (app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().x < -49.66f && app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().x > -50.41)
			{
				collision = true;
			}

			//mushroom 2
			if (app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().x < -99.66f && app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().x > -100.41)
			{
				collision = true;
			}

			//mushroom 5
			if (app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().x < -249.66f && app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().x > -250.41)
			{
				collision = true;
			}

			//mushroom 6
			if (app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().x < -279.66f && app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().x > -280.41)
			{
				collision = true;
			}

			//mushroom 7
			if (app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().x < -309.66f && app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().x > -310.41)
			{
				collision = true;
			}
		}

		if (app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().z < 3.5)
		{
			//mushroom 3
			if (app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().x < -149.66f && app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().x > -150.41)
			{
				collision = true;
			}
		}

		if (app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().z < 4)
		{
			//mushroom 4
			if (app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().x < -199.66f && app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().x > -200.41)
			{
				collision = true;
			}

			//mushroom 8
			if (app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().x < -349.66f && app.CurrentScene()->FindObjectByName("ladybug")->GetPosition().x > -350.41)
			{
				collision = true;
			}
		}
	}

}

void Application::_LateUpdate() {
	for (const auto& layer : _layers) {
		if (layer->Enabled && *(layer->Overrides & AppLayerFunctions::OnLateUpdate)) {
			layer->OnLateUpdate();
		}
	}
}

void Application::_PreRender()
{
	glm::ivec2 size ={ 0, 0 };
	glfwGetWindowSize(_window, &size.x, &size.y);
	glViewport(0, 0, size.x, size.y);
	glScissor(0, 0, size.x, size.y);

	// Clear the screen
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	for (const auto& layer : _layers) {
		if (layer->Enabled && *(layer->Overrides & AppLayerFunctions::OnPreRender)) {
			layer->OnPreRender();
		}
	}
}

void Application::_RenderScene() {

	Framebuffer::Sptr result = nullptr;
	for (const auto& layer : _layers) {
		if (layer->Enabled && *(layer->Overrides & AppLayerFunctions::OnRender)) {
			layer->OnRender(result);
		}
	}
}

void Application::_PostRender() {
	// Note that we use a reverse iterator for post render
	for (auto it = _layers.begin(); it != _layers.end(); it++) {
		const auto& layer = *it;
		if (layer->Enabled && *(layer->Overrides & AppLayerFunctions::OnPostRender)) {
			layer->OnPostRender();
		}
	}
}

void Application::_Unload() {
	// Note that we use a reverse iterator for unloading
	for (auto it = _layers.crbegin(); it != _layers.crend(); it++) {
		const auto& layer = *it;
		if (layer->Enabled && *(layer->Overrides & AppLayerFunctions::OnAppUnload)) {
			layer->OnAppUnload();
		}
	}

	// Clean up ImGui
	ImGuiHelper::Cleanup();
}

void Application::_HandleSceneChange() {
	// If we currently have a current scene, let the layers know it's being unloaded
	if (_currentScene != nullptr) {
		// Note that we use a reverse iterator, so that layers are unloaded in the opposite order that they were loaded
		for (auto it = _layers.crbegin(); it != _layers.crend(); it++) {
			const auto& layer = *it;
			if (layer->Enabled && *(layer->Overrides & AppLayerFunctions::OnSceneUnload)) {
				layer->OnSceneUnload();
			}
		}
	}

	_currentScene = _targetScene;
	
	// Let the layers know that we've loaded in a new scene
	for (const auto& layer : _layers) {
		if (layer->Enabled && *(layer->Overrides & AppLayerFunctions::OnSceneLoad)) {
			layer->OnSceneLoad();
		}
	}

	// Wake up all game objects in the scene
	_currentScene->Awake();

	// If we are not in editor mode, scenes play by default
	if (!_isEditor) {
		_currentScene->IsPlaying = true;
	}

	_targetScene = nullptr;
}

void Application::_HandleWindowSizeChanged(const glm::ivec2& newSize) {
	for (const auto& layer : _layers) {
		if (layer->Enabled && *(layer->Overrides & AppLayerFunctions::OnWindowResize)) {
			layer->OnWindowResize(_windowSize, newSize);
		}
	}
	_windowSize = newSize;
	_primaryViewport = { 0, 0, newSize.x, newSize.y };
}

void Application::_ConfigureSettings() {
	// Start with the defaul application settings
	_appSettings = _GetDefaultAppSettings();

	// We'll store our settings in the %APPDATA% directory, under our application name
	std::filesystem::path appdata = getenv("APPDATA");
	std::filesystem::path settingsPath = appdata / _applicationName / "app-settings.json";

	// If the settings file exists, we can load it in!
	if (std::filesystem::exists(settingsPath)) {
		// Read contents and parse into a JSON blob
		std::string content = FileHelpers::ReadFile(settingsPath.string());
		nlohmann::json blob = nlohmann::json::parse(content);

		// We use merge_patch so that we can keep our defaults if they are missing from the file!
		_appSettings.merge_patch(blob);
	}
	// If the file does not exist, save the default application settings to the path
	else {
		SaveSettings();
	}
}

nlohmann::json Application::_GetDefaultAppSettings()
{
	nlohmann::json result ={};

	for (const auto& layer : _layers) {
		if (!layer->Name.empty()) {
			result[layer->Name] = layer->GetDefaultConfig();
		}
		else {
			LOG_WARN("Unnamed layer! Injecting settings into global namespace, may conflict with other layers!");
			result.merge_patch(layer->GetDefaultConfig());
		}
	}

	result["window_width"]  = DEFAULT_WINDOW_WIDTH;
	result["window_height"] = DEFAULT_WINDOW_HEIGHT;
	return result;
}

