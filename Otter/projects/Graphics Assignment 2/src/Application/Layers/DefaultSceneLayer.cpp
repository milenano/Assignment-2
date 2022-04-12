#include "DefaultSceneLayer.h"

// GLM math library
#include <GLM/glm.hpp>
#include <GLM/gtc/matrix_transform.hpp>
#include <GLM/gtc/type_ptr.hpp>
#include <GLM/gtc/random.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <GLM/gtx/common.hpp> // for fmod (floating modulus)

#include <filesystem>

// Graphics
#include "Graphics/Buffers/IndexBuffer.h"
#include "Graphics/Buffers/VertexBuffer.h"
#include "Graphics/VertexArrayObject.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/Textures/Texture2D.h"
#include "Graphics/Textures/TextureCube.h"
#include "Graphics/VertexTypes.h"
#include "Graphics/Font.h"
#include "Graphics/GuiBatcher.h"
#include "Graphics/Framebuffer.h"

// Utilities
#include "Utils/MeshBuilder.h"
#include "Utils/MeshFactory.h"
#include "Utils/ObjLoader.h"
#include "Utils/ImGuiHelper.h"
#include "Utils/ResourceManager/ResourceManager.h"
#include "Utils/FileHelpers.h"
#include "Utils/JsonGlmHelpers.h"
#include "Utils/StringUtils.h"
#include "Utils/GlmDefines.h"

// Gameplay
#include "Gameplay/Material.h"
#include "Gameplay/GameObject.h"
#include "Gameplay/Scene.h"
#include "Gameplay/Components/Light.h"

// Components
#include "Gameplay/Components/IComponent.h"
#include "Gameplay/Components/Camera.h"
#include "Gameplay/Components/RotatingBehaviour.h"
#include "Gameplay/Components/JumpBehaviour.h"
#include "Gameplay/Components/RenderComponent.h"
#include "Gameplay/Components/MaterialSwapBehaviour.h"
#include "Gameplay/Components/TriggerVolumeEnterBehaviour.h"
#include "Gameplay/Components/SimpleCameraControl.h"

// Physics
#include "Gameplay/Physics/RigidBody.h"
#include "Gameplay/Physics/Colliders/BoxCollider.h"
#include "Gameplay/Physics/Colliders/PlaneCollider.h"
#include "Gameplay/Physics/Colliders/SphereCollider.h"
#include "Gameplay/Physics/Colliders/ConvexMeshCollider.h"
#include "Gameplay/Physics/Colliders/CylinderCollider.h"
#include "Gameplay/Physics/TriggerVolume.h"
#include "Graphics/DebugDraw.h"

// GUI
#include "Gameplay/Components/GUI/RectTransform.h"
#include "Gameplay/Components/GUI/GuiPanel.h"
#include "Gameplay/Components/GUI/GuiText.h"
#include "Gameplay/InputEngine.h"

#include "Application/Application.h"
#include "Gameplay/Components/ParticleSystem.h"
#include "Graphics/Textures/Texture3D.h"
#include "Graphics/Textures/Texture1D.h"
#include "Application/Layers/ImGuiDebugLayer.h"
#include "Application/Windows/DebugWindow.h"
#include "Gameplay/Components/ShadowCamera.h"

std::string lightNum;

DefaultSceneLayer::DefaultSceneLayer() :
	ApplicationLayer()
{
	Name = "Default Scene";
	Overrides = AppLayerFunctions::OnAppLoad;
}

DefaultSceneLayer::~DefaultSceneLayer() = default;

void DefaultSceneLayer::OnAppLoad(const nlohmann::json& config) {
	_CreateScene();
}

void DefaultSceneLayer::_CreateScene()
{
	using namespace Gameplay;
	using namespace Gameplay::Physics;

	Application& app = Application::Get();

	bool loadScene = false;
	// For now we can use a toggle to generate our scene vs load from file
	if (loadScene && std::filesystem::exists("scene.json")) {
		app.LoadScene("scene.json");
	} else {
		 
		// Basic gbuffer generation with no vertex manipulation
		ShaderProgram::Sptr deferredForward = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/basic.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/deferred_forward.glsl" }
		});
		deferredForward->SetDebugName("Deferred - GBuffer Generation");  

		// Our foliage shader which manipulates the vertices of the mesh
		ShaderProgram::Sptr foliageShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/foliage.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/deferred_forward.glsl" }
		});  
		foliageShader->SetDebugName("Foliage");   

		// This shader handles our multitexturing example
		ShaderProgram::Sptr multiTextureShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/vert_multitextured.glsl" },  
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/frag_multitextured.glsl" }
		});
		multiTextureShader->SetDebugName("Multitexturing"); 

		// This shader handles our displacement mapping example
		ShaderProgram::Sptr displacementShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/displacement_mapping.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/deferred_forward.glsl" } 
		});
		displacementShader->SetDebugName("Displacement Mapping");

		// This shader handles our cel shading example
		ShaderProgram::Sptr celShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/displacement_mapping.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/cel_shader.glsl" }
		});
		celShader->SetDebugName("Cel Shader");

	

		// Load in the meshes
		MeshResource::Sptr monkeyMesh = ResourceManager::CreateAsset<MeshResource>("Monkey.obj");
		MeshResource::Sptr shipMesh   = ResourceManager::CreateAsset<MeshResource>("fenrir.obj");
		MeshResource::Sptr bmMesh = ResourceManager::CreateAsset<MeshResource>("bm.obj");
		MeshResource::Sptr tmMesh = ResourceManager::CreateAsset<MeshResource>("tm.obj");
		MeshResource::Sptr MushroomMesh = ResourceManager::CreateAsset<MeshResource>("Mushroom.obj");
		MeshResource::Sptr ExitMesh = ResourceManager::CreateAsset<MeshResource>("ExitTree.obj");


		//Our Meshes
		Texture2D::Sptr    backgroundTexture = ResourceManager::CreateAsset<Texture2D>("textures/BackgroundUV.png");
		Texture2D::Sptr    dbackgroundTexture = ResourceManager::CreateAsset<Texture2D>("textures/bg.png");
		Texture2D::Sptr    ladybugTexture = ResourceManager::CreateAsset<Texture2D>("textures/LadybugUV.png");
		Texture2D::Sptr	   BmTex = ResourceManager::CreateAsset<Texture2D>("textures/bmuv.png");
		Texture2D::Sptr    MushroomTex = ResourceManager::CreateAsset<Texture2D>("textures/MushroomUV.png");
		Texture2D::Sptr    TmTex = ResourceManager::CreateAsset<Texture2D>("textures/tmuv.png");
		Texture2D::Sptr    ExitTex = ResourceManager::CreateAsset<Texture2D>("textures/ExitTreeUV.png");

		// Load in some textures
		Texture2D::Sptr    boxTexture   = ResourceManager::CreateAsset<Texture2D>("textures/box-diffuse.png");
		Texture2D::Sptr    boxSpec      = ResourceManager::CreateAsset<Texture2D>("textures/box-specular.png");
		Texture2D::Sptr    monkeyTex    = ResourceManager::CreateAsset<Texture2D>("textures/monkey-uvMap.png");
		Texture2D::Sptr    leafTex      = ResourceManager::CreateAsset<Texture2D>("textures/leaves.png");
		leafTex->SetMinFilter(MinFilter::Nearest);
		leafTex->SetMagFilter(MagFilter::Nearest);

		//Our Textures
		MeshResource::Sptr backgroundMesh = ResourceManager::CreateAsset<MeshResource>("Background.obj");
		MeshResource::Sptr ladybugMesh = ResourceManager::CreateAsset<MeshResource>("lbo2.obj");


		// Load some images for drag n' drop
		ResourceManager::CreateAsset<Texture2D>("textures/flashlight.png");
		ResourceManager::CreateAsset<Texture2D>("textures/flashlight-2.png");
		ResourceManager::CreateAsset<Texture2D>("textures/light_projection.png");

		DebugWindow::Sptr debugWindow = app.GetLayer<ImGuiDebugLayer>()->GetWindow<DebugWindow>();

#pragma region Basic Texture Creation
		Texture2DDescription singlePixelDescriptor;
		singlePixelDescriptor.Width = singlePixelDescriptor.Height = 1;
		singlePixelDescriptor.Format = InternalFormat::RGB8;

		float normalMapDefaultData[3] = { 0.5f, 0.5f, 1.0f };
		Texture2D::Sptr normalMapDefault = ResourceManager::CreateAsset<Texture2D>(singlePixelDescriptor);
		normalMapDefault->LoadData(1, 1, PixelFormat::RGB, PixelType::Float, normalMapDefaultData);

		float solidGrey[3] = { 0.5f, 0.5f, 0.5f };
		float solidBlack[3] = { 0.0f, 0.0f, 0.0f };
		float solidWhite[3] = { 1.0f, 1.0f, 1.0f };

		Texture2D::Sptr solidBlackTex = ResourceManager::CreateAsset<Texture2D>(singlePixelDescriptor);
		solidBlackTex->LoadData(1, 1, PixelFormat::RGB, PixelType::Float, solidBlack);

		Texture2D::Sptr solidGreyTex = ResourceManager::CreateAsset<Texture2D>(singlePixelDescriptor);
		solidGreyTex->LoadData(1, 1, PixelFormat::RGB, PixelType::Float, solidGrey);

		Texture2D::Sptr solidWhiteTex = ResourceManager::CreateAsset<Texture2D>(singlePixelDescriptor);
		solidWhiteTex->LoadData(1, 1, PixelFormat::RGB, PixelType::Float, solidWhite);

#pragma endregion 

		// Loading in a 1D LUT
		Texture1D::Sptr toonLut = ResourceManager::CreateAsset<Texture1D>("luts/toon-1D.png"); 
		toonLut->SetWrap(WrapMode::ClampToEdge);

		// Here we'll load in the cubemap, as well as a special shader to handle drawing the skybox
		TextureCube::Sptr testCubemap = ResourceManager::CreateAsset<TextureCube>("cubemaps/ocean/ocean.jpg");
		ShaderProgram::Sptr      skyboxShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/skybox_vert.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/skybox_frag.glsl" } 
		});
		  
		// Create an empty scene
		Scene::Sptr scene = std::make_shared<Scene>();  

		// Setting up our enviroment map
		scene->SetSkyboxTexture(testCubemap); 
		scene->SetSkyboxShader(skyboxShader);
		// Since the skybox I used was for Y-up, we need to rotate it 90 deg around the X-axis to convert it to z-up 
		scene->SetSkyboxRotation(glm::rotate(MAT4_IDENTITY, glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f)));

		// Loading in a color lookup table
		Texture3D::Sptr lut = ResourceManager::CreateAsset<Texture3D>("luts/cool.CUBE");   

		// Configure the color correction LUT
		scene->SetColorLUT(lut);

		// Create our materials
		// This will be our box material, with no environment reflections
		Material::Sptr boxMaterial = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			boxMaterial->Name = "Box";
			boxMaterial->Set("u_Material.AlbedoMap", boxTexture);
			boxMaterial->Set("u_Material.Shininess", 0.1f);
			boxMaterial->Set("u_Material.NormalMap", normalMapDefault);
		}

		// This will be the reflective material, we'll make the whole thing 90% reflective
		Material::Sptr monkeyMaterial = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			monkeyMaterial->Name = "Monkey";
			monkeyMaterial->Set("u_Material.AlbedoMap", monkeyTex);
			monkeyMaterial->Set("u_Material.NormalMap", normalMapDefault);
			monkeyMaterial->Set("u_Material.Shininess", 0.5f);
		}

		Material::Sptr backgroundMaterial = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			backgroundMaterial->Name = "Box";
			backgroundMaterial->Set("u_Material.AlbedoMap", backgroundTexture);
			backgroundMaterial->Set("u_Material.Shininess", 0.1f);
			backgroundMaterial->Set("u_Material.NormalMap", normalMapDefault);
		}

		// This will be the reflective material, we'll make the whole thing 90% reflective
		Material::Sptr TMMaterial = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			TMMaterial->Name = "Tall Mushroom";
			TMMaterial->Set("u_Material.AlbedoMap", TmTex);
			TMMaterial->Set("u_Material.NormalMap", normalMapDefault);
			TMMaterial->Set("u_Material.Shininess", 0.5f);
		}

		// This will be the reflective material, we'll make the whole thing 90% reflective
		Material::Sptr BMMaterial = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			BMMaterial->Name = "Beeg Mushroom";
			BMMaterial->Set("u_Material.AlbedoMap", BmTex);
			BMMaterial->Set("u_Material.NormalMap", normalMapDefault);
			BMMaterial->Set("u_Material.Shininess", 0.5f);
		}

		// This will be the reflective material, we'll make the whole thing 90% reflective
		Material::Sptr MushroomMaterial = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			MushroomMaterial->Name = "Tall Mushroom";
			MushroomMaterial->Set("u_Material.AlbedoMap", MushroomTex);
			MushroomMaterial->Set("u_Material.NormalMap", normalMapDefault);
			MushroomMaterial->Set("u_Material.Shininess", 0.5f);
		}

		Material::Sptr bgMaterial = ResourceManager::CreateAsset<Material>(deferredForward); //2d background
		{
			bgMaterial->Name = "planebg";
			bgMaterial->Set("u_Material.AlbedoMap", dbackgroundTexture);
			bgMaterial->Set("u_Material.Shininess", 0.1f);
			bgMaterial->Set("u_Material.NormalMap", normalMapDefault);
		}

		Material::Sptr ExitMaterial = ResourceManager::CreateAsset<Material>(deferredForward); //2d background
		{
			ExitMaterial->Name = "ExitTree";
			ExitMaterial->Set("u_Material.AlbedoMap", ExitTex);
			ExitMaterial->Set("u_Material.Shininess", 0.1f);
			ExitMaterial->Set("u_Material.NormalMap", normalMapDefault);
		}

		Material::Sptr ladybugMaterial = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			ladybugMaterial->Name = "Box";
			ladybugMaterial->Set("u_Material.AlbedoMap", ladybugTexture);
			ladybugMaterial->Set("u_Material.Shininess", 0.1f);
			ladybugMaterial->Set("u_Material.NormalMap", normalMapDefault);
		}

		// This will be the reflective material, we'll make the whole thing 50% reflective
		Material::Sptr testMaterial = ResourceManager::CreateAsset<Material>(deferredForward); 
		{
			testMaterial->Name = "Box-Specular";
			testMaterial->Set("u_Material.AlbedoMap", boxTexture); 
			testMaterial->Set("u_Material.Specular", boxSpec);
			testMaterial->Set("u_Material.NormalMap", normalMapDefault);
		}

		// Our foliage vertex shader material 
		Material::Sptr foliageMaterial = ResourceManager::CreateAsset<Material>(foliageShader);
		{
			foliageMaterial->Name = "Foliage Shader";
			foliageMaterial->Set("u_Material.AlbedoMap", leafTex);
			foliageMaterial->Set("u_Material.Shininess", 0.1f);
			foliageMaterial->Set("u_Material.DiscardThreshold", 0.1f);
			foliageMaterial->Set("u_Material.NormalMap", normalMapDefault);

			foliageMaterial->Set("u_WindDirection", glm::vec3(1.0f, 1.0f, 0.0f));
			foliageMaterial->Set("u_WindStrength", 0.5f);
			foliageMaterial->Set("u_VerticalScale", 1.0f);
			foliageMaterial->Set("u_WindSpeed", 1.0f);
		}

		// Our toon shader material
		Material::Sptr toonMaterial = ResourceManager::CreateAsset<Material>(celShader);
		{
			toonMaterial->Name = "Toon"; 
			toonMaterial->Set("u_Material.AlbedoMap", ladybugTexture);
			toonMaterial->Set("u_Material.NormalMap", normalMapDefault);
			toonMaterial->Set("s_ToonTerm", toonLut);
			toonMaterial->Set("u_Material.Shininess", 0.1f); 
			toonMaterial->Set("u_Material.Steps", 8);
		}


		Material::Sptr displacementTest = ResourceManager::CreateAsset<Material>(displacementShader);
		{
			Texture2D::Sptr displacementMap = ResourceManager::CreateAsset<Texture2D>("textures/displacement_map.png");
			Texture2D::Sptr normalMap       = ResourceManager::CreateAsset<Texture2D>("textures/normal_map.png");
			Texture2D::Sptr diffuseMap      = ResourceManager::CreateAsset<Texture2D>("textures/bricks_diffuse.png");

			displacementTest->Name = "Displacement Map";
			displacementTest->Set("u_Material.AlbedoMap", diffuseMap);
			displacementTest->Set("u_Material.NormalMap", normalMap);
			displacementTest->Set("s_Heightmap", displacementMap);
			displacementTest->Set("u_Material.Shininess", 0.5f);
			displacementTest->Set("u_Scale", 0.1f);
		}

		Material::Sptr grey = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			grey->Name = "Grey";
			grey->Set("u_Material.AlbedoMap", solidGreyTex);
			grey->Set("u_Material.Specular", solidBlackTex);
			grey->Set("u_Material.NormalMap", normalMapDefault);
		}

		Material::Sptr polka = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			polka->Name = "Polka";
			polka->Set("u_Material.AlbedoMap", ResourceManager::CreateAsset<Texture2D>("textures/polka.png"));
			polka->Set("u_Material.Specular", solidBlackTex);
			polka->Set("u_Material.NormalMap", normalMapDefault);
			polka->Set("u_Material.EmissiveMap", ResourceManager::CreateAsset<Texture2D>("textures/polka.png"));
		}

		Material::Sptr whiteBrick = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			whiteBrick->Name = "White Bricks";
			whiteBrick->Set("u_Material.AlbedoMap", ResourceManager::CreateAsset<Texture2D>("textures/displacement_map.png"));
			whiteBrick->Set("u_Material.Specular", solidGrey);
			whiteBrick->Set("u_Material.NormalMap", ResourceManager::CreateAsset<Texture2D>("textures/normal_map.png"));
		}

		Material::Sptr normalmapMat = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			Texture2D::Sptr normalMap       = ResourceManager::CreateAsset<Texture2D>("textures/normal_map.png");
			Texture2D::Sptr diffuseMap      = ResourceManager::CreateAsset<Texture2D>("textures/bricks_diffuse.png");

			normalmapMat->Name = "Tangent Space Normal Map";
			normalmapMat->Set("u_Material.AlbedoMap", diffuseMap);
			normalmapMat->Set("u_Material.NormalMap", normalMap);
			normalmapMat->Set("u_Material.Shininess", 0.5f);
			normalmapMat->Set("u_Scale", 0.1f);
		}

		Material::Sptr multiTextureMat = ResourceManager::CreateAsset<Material>(multiTextureShader);
		{
			Texture2D::Sptr sand  = ResourceManager::CreateAsset<Texture2D>("textures/terrain/sand.png");
			Texture2D::Sptr grass = ResourceManager::CreateAsset<Texture2D>("textures/terrain/grass.png");

			multiTextureMat->Name = "Multitexturing";
			multiTextureMat->Set("u_Material.DiffuseA", sand);
			multiTextureMat->Set("u_Material.DiffuseB", grass);
			multiTextureMat->Set("u_Material.NormalMapA", normalMapDefault);
			multiTextureMat->Set("u_Material.NormalMapB", normalMapDefault);
			multiTextureMat->Set("u_Material.Shininess", 0.5f);
			multiTextureMat->Set("u_Scale", 0.1f); 
		}

		// Create some lights for our scene
		GameObject::Sptr lightParent = scene->CreateGameObject("Lights");

		for (int ix = 0; ix < 22; ix++) {
			if (ix <= 10)
			{
				GameObject::Sptr light = scene->CreateGameObject("Light");
				light->SetPostion(glm::vec3(-50.f * ix, 1.f, 40.0f));
				lightParent->AddChild(light);

				Light::Sptr lightComponent = light->Add<Light>();
				lightComponent->SetColor(glm::vec3(0.49f, 1.f, 0.32f));
				lightComponent->SetRadius(5.f);
				lightComponent->SetIntensity(250.f);
			}
			else if (ix <= 21)
			{
				GameObject::Sptr light = scene->CreateGameObject("Light");
				light->SetPostion(glm::vec3(-50.f * (ix - 11), -90, 100.0f));
				lightParent->AddChild(light);

				Light::Sptr lightComponent = light->Add<Light>();
				lightComponent->SetColor(glm::vec3(0.45f, 0.678f, 0.1872f));
				lightComponent->SetRadius(30.f);
				lightComponent->SetIntensity(500.f);
			}
			else if (ix <= 30)
			{
				GameObject::Sptr light = scene->CreateGameObject("Light");
				light->SetPostion(glm::vec3(-50.f * (ix - 21), 20.f, 5.0f));
				lightParent->AddChild(light);

				Light::Sptr lightComponent = light->Add<Light>();
				lightComponent->SetColor(glm::vec3(1.f, 1.f, 1.f));
				lightComponent->SetRadius(5.f);
				lightComponent->SetIntensity(1.f);
			}
			
		}

		// We'll create a mesh that is a simple plane that we can resize later
		MeshResource::Sptr planeMesh = ResourceManager::CreateAsset<MeshResource>();
		planeMesh->AddParam(MeshBuilderParam::CreatePlane(ZERO, UNIT_Z, UNIT_X, glm::vec2(1.0f)));
		planeMesh->GenerateMesh();

		MeshResource::Sptr sphere = ResourceManager::CreateAsset<MeshResource>();
		sphere->AddParam(MeshBuilderParam::CreateIcoSphere(ZERO, ONE, 5));
		sphere->GenerateMesh();

		// Set up the scene's camera
		GameObject::Sptr camera = scene->MainCamera->GetGameObject()->SelfRef();
		{
			camera->SetPostion({ 0, 6.8, 2 });
			camera->SetRotation({ 90, 0, -180 });
			camera->LookAt(glm::vec3(0.0f));
			camera->SetScale({ 0.8, 0.8, 0.8 });

			camera->Add<SimpleCameraControl>();

			// This is now handled by scene itself!
			//Camera::Sptr cam = camera->Add<Camera>();
			// Make sure that the camera is set as the scene's main camera!
			//scene->MainCamera = cam;
		}

		// Set up all our sample objects
		GameObject::Sptr Mushroom1 = scene->CreateGameObject("Mushroom1"); //2dbg
		{
			Mushroom1->SetPostion(glm::vec3(-50.f, 0.f, -0.66f));
			Mushroom1->SetRotation(glm::vec3(90.f, 0.0f, 0.0f));
			Mushroom1->SetScale(glm::vec3(0.5f));
			// Make a big tiled mesh
			MeshResource::Sptr tiledMesh = ResourceManager::CreateAsset<MeshResource>();
			tiledMesh->AddParam(MeshBuilderParam::CreatePlane(ZERO, UNIT_Z, UNIT_X, glm::vec2(100.0f), glm::vec2(20.0f)));
			tiledMesh->GenerateMesh();

			// Create and attach a RenderComponent to the object to draw our mesh
			RenderComponent::Sptr renderer = Mushroom1->Add<RenderComponent>();
			renderer->SetMesh(MushroomMesh);
			renderer->SetMaterial(MushroomMaterial);

			// Attach a plane collider that extends infinitely along the X/Y axis
			RigidBody::Sptr physics = Mushroom1->Add<RigidBody>(/*static by default*/);
			physics->AddCollider(BoxCollider::Create(glm::vec3(50.0f, 50.0f, 1.0f)))->SetPosition({ 0,0,-1 });
		}

		// Set up all our sample objects
		GameObject::Sptr Mushroom2 = scene->CreateGameObject("Mushroom2"); //2dbg
		{
			Mushroom2->SetPostion(glm::vec3(-100.f, 0.f, -0.66f));
			Mushroom2->SetRotation(glm::vec3(90.f, 0.0f, 0.0f));
			Mushroom2->SetScale(glm::vec3(0.5f));
			// Make a big tiled mesh
			MeshResource::Sptr tiledMesh = ResourceManager::CreateAsset<MeshResource>();
			tiledMesh->AddParam(MeshBuilderParam::CreatePlane(ZERO, UNIT_Z, UNIT_X, glm::vec2(100.0f), glm::vec2(20.0f)));
			tiledMesh->GenerateMesh();

			// Create and attach a RenderComponent to the object to draw our mesh
			RenderComponent::Sptr renderer = Mushroom2->Add<RenderComponent>();
			renderer->SetMesh(MushroomMesh);
			renderer->SetMaterial(MushroomMaterial);

			// Attach a plane collider that extends infinitely along the X/Y axis
			RigidBody::Sptr physics = Mushroom2->Add<RigidBody>(/*static by default*/);
			physics->AddCollider(BoxCollider::Create(glm::vec3(50.0f, 50.0f, 1.0f)))->SetPosition({ 0,0,-1 });
		}

		// Set up all our sample objects
		GameObject::Sptr Mushroom3 = scene->CreateGameObject("Mushroom3"); //2dbg
		{
			Mushroom3->SetPostion(glm::vec3(-150.f, 0.f, -0.66f));
			Mushroom3->SetRotation(glm::vec3(90.f, 0.0f, 0.0f));
			Mushroom3->SetScale(glm::vec3(1.f));
			// Make a big tiled mesh
			MeshResource::Sptr tiledMesh = ResourceManager::CreateAsset<MeshResource>();
			tiledMesh->AddParam(MeshBuilderParam::CreatePlane(ZERO, UNIT_Z, UNIT_X, glm::vec2(100.0f), glm::vec2(20.0f)));
			tiledMesh->GenerateMesh();

			// Create and attach a RenderComponent to the object to draw our mesh
			RenderComponent::Sptr renderer = Mushroom3->Add<RenderComponent>();
			renderer->SetMesh(tmMesh);
			renderer->SetMaterial(TMMaterial);

			// Attach a plane collider that extends infinitely along the X/Y axis
			RigidBody::Sptr physics = Mushroom3->Add<RigidBody>(/*static by default*/);
			physics->AddCollider(BoxCollider::Create(glm::vec3(50.0f, 50.0f, 1.0f)))->SetPosition({ 0,0,-1 });
		}

		// Set up all our sample objects
		GameObject::Sptr Mushroom4 = scene->CreateGameObject("Mushroom4"); //2dbg
		{
			Mushroom4->SetPostion(glm::vec3(-200.f, 0.f, -0.66f));
			Mushroom4->SetRotation(glm::vec3(90.f, 0.0f, 0.0f));
			Mushroom4->SetScale(glm::vec3(1.f));
			// Make a big tiled mesh
			MeshResource::Sptr tiledMesh = ResourceManager::CreateAsset<MeshResource>();
			tiledMesh->AddParam(MeshBuilderParam::CreatePlane(ZERO, UNIT_Z, UNIT_X, glm::vec2(100.0f), glm::vec2(20.0f)));
			tiledMesh->GenerateMesh();

			// Create and attach a RenderComponent to the object to draw our mesh
			RenderComponent::Sptr renderer = Mushroom4->Add<RenderComponent>();
			renderer->SetMesh(bmMesh);
			renderer->SetMaterial(BMMaterial);

			// Attach a plane collider that extends infinitely along the X/Y axis
			RigidBody::Sptr physics = Mushroom4->Add<RigidBody>(/*static by default*/);
			physics->AddCollider(BoxCollider::Create(glm::vec3(50.0f, 50.0f, 1.0f)))->SetPosition({ 0,0,-1 });
		}

		// Set up all our sample objects
		GameObject::Sptr Mushroom5 = scene->CreateGameObject("Mushroom5"); //2dbg
		{
			Mushroom5->SetPostion(glm::vec3(-250.f, 0.f, -0.66f));
			Mushroom5->SetRotation(glm::vec3(90.f, 0.0f, 0.0f));
			Mushroom5->SetScale(glm::vec3(0.5f));
			// Make a big tiled mesh
			MeshResource::Sptr tiledMesh = ResourceManager::CreateAsset<MeshResource>();
			tiledMesh->AddParam(MeshBuilderParam::CreatePlane(ZERO, UNIT_Z, UNIT_X, glm::vec2(100.0f), glm::vec2(20.0f)));
			tiledMesh->GenerateMesh();

			// Create and attach a RenderComponent to the object to draw our mesh
			RenderComponent::Sptr renderer = Mushroom5->Add<RenderComponent>();
			renderer->SetMesh(MushroomMesh);
			renderer->SetMaterial(MushroomMaterial);

			// Attach a plane collider that extends infinitely along the X/Y axis
			RigidBody::Sptr physics = Mushroom5->Add<RigidBody>(/*static by default*/);
			physics->AddCollider(BoxCollider::Create(glm::vec3(50.0f, 50.0f, 1.0f)))->SetPosition({ 0,0,-1 });
		}

		// Set up all our sample objects
		GameObject::Sptr Mushroom6 = scene->CreateGameObject("Mushroom6"); //2dbg
		{
			Mushroom6->SetPostion(glm::vec3(-280.f, 0.f, -0.66f));
			Mushroom6->SetRotation(glm::vec3(90.f, 0.0f, 0.0f));
			Mushroom6->SetScale(glm::vec3(0.5f));
			// Make a big tiled mesh
			MeshResource::Sptr tiledMesh = ResourceManager::CreateAsset<MeshResource>();
			tiledMesh->AddParam(MeshBuilderParam::CreatePlane(ZERO, UNIT_Z, UNIT_X, glm::vec2(100.0f), glm::vec2(20.0f)));
			tiledMesh->GenerateMesh();

			// Create and attach a RenderComponent to the object to draw our mesh
			RenderComponent::Sptr renderer = Mushroom6->Add<RenderComponent>();
			renderer->SetMesh(MushroomMesh);
			renderer->SetMaterial(MushroomMaterial);

			// Attach a plane collider that extends infinitely along the X/Y axis
			RigidBody::Sptr physics = Mushroom6->Add<RigidBody>(/*static by default*/);
			physics->AddCollider(BoxCollider::Create(glm::vec3(50.0f, 50.0f, 1.0f)))->SetPosition({ 0,0,-1 });
		}

		// Set up all our sample objects
		GameObject::Sptr Mushroom7 = scene->CreateGameObject("Mushroom7"); //2dbg
		{
			Mushroom7->SetPostion(glm::vec3(-310.f, 0.f, -0.66f));
			Mushroom7->SetRotation(glm::vec3(90.f, 0.0f, 0.0f));
			Mushroom7->SetScale(glm::vec3(0.5f));
			// Make a big tiled mesh
			MeshResource::Sptr tiledMesh = ResourceManager::CreateAsset<MeshResource>();
			tiledMesh->AddParam(MeshBuilderParam::CreatePlane(ZERO, UNIT_Z, UNIT_X, glm::vec2(100.0f), glm::vec2(20.0f)));
			tiledMesh->GenerateMesh();

			// Create and attach a RenderComponent to the object to draw our mesh
			RenderComponent::Sptr renderer = Mushroom7->Add<RenderComponent>();
			renderer->SetMesh(MushroomMesh);
			renderer->SetMaterial(MushroomMaterial);

			// Attach a plane collider that extends infinitely along the X/Y axis
			RigidBody::Sptr physics = Mushroom7->Add<RigidBody>(/*static by default*/);
			physics->AddCollider(BoxCollider::Create(glm::vec3(50.0f, 50.0f, 1.0f)))->SetPosition({ 0,0,-1 });
		}

		// Set up all our sample objects
		GameObject::Sptr Mushroom8 = scene->CreateGameObject("Mushroom8"); //2dbg
		{
			Mushroom8->SetPostion(glm::vec3(-350.f, 0.f, -0.66f));
			Mushroom8->SetRotation(glm::vec3(90.f, 0.0f, 0.0f));
			Mushroom8->SetScale(glm::vec3(1.f));
			// Make a big tiled mesh
			MeshResource::Sptr tiledMesh = ResourceManager::CreateAsset<MeshResource>();
			tiledMesh->AddParam(MeshBuilderParam::CreatePlane(ZERO, UNIT_Z, UNIT_X, glm::vec2(100.0f), glm::vec2(20.0f)));
			tiledMesh->GenerateMesh();

			// Create and attach a RenderComponent to the object to draw our mesh
			RenderComponent::Sptr renderer = Mushroom8->Add<RenderComponent>();
			renderer->SetMesh(bmMesh);
			renderer->SetMaterial(BMMaterial);

			// Attach a plane collider that extends infinitely along the X/Y axis
			RigidBody::Sptr physics = Mushroom8->Add<RigidBody>(/*static by default*/);
			physics->AddCollider(BoxCollider::Create(glm::vec3(50.0f, 50.0f, 1.0f)))->SetPosition({ 0,0,-1 });
		}


		// Set up all our sample objects
		GameObject::Sptr plane = scene->CreateGameObject("Plane"); //2dbg
		{
			plane->SetPostion(glm::vec3(350.f, -130.f, 62.f));
			plane->SetRotation(glm::vec3(90.f, 0.0f, -180.0f));
			plane->SetScale(glm::vec3(375.0f, 125.0f, 250.0f));
			// Make a big tiled mesh
			MeshResource::Sptr tiledMesh = ResourceManager::CreateAsset<MeshResource>();
			tiledMesh->AddParam(MeshBuilderParam::CreatePlane(ZERO, UNIT_Z, UNIT_X, glm::vec2(100.0f), glm::vec2(20.0f)));
			tiledMesh->GenerateMesh();

			// Create and attach a RenderComponent to the object to draw our mesh
			RenderComponent::Sptr renderer = plane->Add<RenderComponent>();
			renderer->SetMesh(planeMesh);
			renderer->SetMaterial(bgMaterial);

			// Attach a plane collider that extends infinitely along the X/Y axis
			RigidBody::Sptr physics = plane->Add<RigidBody>(/*static by default*/);
			physics->AddCollider(BoxCollider::Create(glm::vec3(50.0f, 50.0f, 1.0f)))->SetPosition({ 0,0,-1 });
		}

		// Set up all our sample objects
		GameObject::Sptr plane2 = scene->CreateGameObject("Plane2"); //2dbg
		{
			plane2->SetPostion(glm::vec3(0.f, -130.f, 62.f));
			plane2->SetRotation(glm::vec3(90.f, 0.0f, -180.0f));
			plane2->SetScale(glm::vec3(375.0f, 125.0f, 250.0f));
			// Make a big tiled mesh
			MeshResource::Sptr tiledMesh = ResourceManager::CreateAsset<MeshResource>();
			tiledMesh->AddParam(MeshBuilderParam::CreatePlane(ZERO, UNIT_Z, UNIT_X, glm::vec2(100.0f), glm::vec2(20.0f)));
			tiledMesh->GenerateMesh();

			// Create and attach a RenderComponent to the object to draw our mesh
			RenderComponent::Sptr renderer = plane2->Add<RenderComponent>();
			renderer->SetMesh(planeMesh);
			renderer->SetMaterial(bgMaterial);

			// Attach a plane collider that extends infinitely along the X/Y axis
			RigidBody::Sptr physics = plane2->Add<RigidBody>(/*static by default*/);
			physics->AddCollider(BoxCollider::Create(glm::vec3(50.0f, 50.0f, 1.0f)))->SetPosition({ 0,0,-1 });
		}

		// Set up all our sample objects
		GameObject::Sptr plane3 = scene->CreateGameObject("Plane3"); //2dbg
		{
			plane3->SetPostion(glm::vec3(-370.f, -130.f, 62.f));
			plane3->SetRotation(glm::vec3(90.f, 0.0f, -180.0f));
			plane3->SetScale(glm::vec3(375.0f, 125.0f, 250.0f));
			// Make a big tiled mesh
			MeshResource::Sptr tiledMesh = ResourceManager::CreateAsset<MeshResource>();
			tiledMesh->AddParam(MeshBuilderParam::CreatePlane(ZERO, UNIT_Z, UNIT_X, glm::vec2(100.0f), glm::vec2(20.0f)));
			tiledMesh->GenerateMesh();

			// Create and attach a RenderComponent to the object to draw our mesh
			RenderComponent::Sptr renderer = plane3->Add<RenderComponent>();
			renderer->SetMesh(planeMesh);
			renderer->SetMaterial(bgMaterial);

			// Attach a plane collider that extends infinitely along the X/Y axis
			RigidBody::Sptr physics = plane3->Add<RigidBody>(/*static by default*/);
			physics->AddCollider(BoxCollider::Create(glm::vec3(50.0f, 50.0f, 1.0f)))->SetPosition({ 0,0,-1 });
		}

		// Set up all our sample objects
		GameObject::Sptr plane4 = scene->CreateGameObject("Plane4"); //2dbg
		{
			plane4->SetPostion(glm::vec3(-700.f, -130.f, 62.f));
			plane4->SetRotation(glm::vec3(90.f, 0.0f, -180.0f));
			plane4->SetScale(glm::vec3(375.0f, 125.0f, 250.0f));
			// Make a big tiled mesh
			MeshResource::Sptr tiledMesh = ResourceManager::CreateAsset<MeshResource>();
			tiledMesh->AddParam(MeshBuilderParam::CreatePlane(ZERO, UNIT_Z, UNIT_X, glm::vec2(100.0f), glm::vec2(20.0f)));
			tiledMesh->GenerateMesh();

			// Create and attach a RenderComponent to the object to draw our mesh
			RenderComponent::Sptr renderer = plane4->Add<RenderComponent>();
			renderer->SetMesh(planeMesh);
			renderer->SetMaterial(bgMaterial);

			// Attach a plane collider that extends infinitely along the X/Y axis
			RigidBody::Sptr physics = plane4->Add<RigidBody>(/*static by default*/);
			physics->AddCollider(BoxCollider::Create(glm::vec3(50.0f, 50.0f, 1.0f)))->SetPosition({ 0,0,-1 });
		}

		GameObject::Sptr bg = scene->CreateGameObject("bg");
		{
			// Set position in the scene
			bg->SetPostion(glm::vec3(107.7f, -55.830f, -1.7f));
			bg->SetRotation(glm::vec3(90.f, 0.0f, -180.0f));
			bg->SetScale(glm::vec3(6.f));

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = bg->Add<RenderComponent>();
			renderer->SetMesh(backgroundMesh);
			renderer->SetMaterial(backgroundMaterial);

			// Example of a trigger that interacts with static and kinematic bodies as well as dynamic bodies
			TriggerVolume::Sptr trigger = bg->Add<TriggerVolume>();
			trigger->SetFlags(TriggerTypeFlags::Statics | TriggerTypeFlags::Kinematics);
			trigger->AddCollider(BoxCollider::Create(glm::vec3(1.0f)));

			bg->Add<TriggerVolumeEnterBehaviour>();
		}
		GameObject::Sptr bg2 = scene->CreateGameObject("bg2");
		{
			// Set position in the scene
			// Set position in the scene
			bg2->SetPostion(glm::vec3(0.f, -55.830f, -1.7f));
			bg2->SetRotation(glm::vec3(90.f, 0.0f, -180.0f));
			bg2->SetScale(glm::vec3(6.f));

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = bg2->Add<RenderComponent>();
			renderer->SetMesh(backgroundMesh);
			renderer->SetMaterial(backgroundMaterial);

			// Example of a trigger that interacts with static and kinematic bodies as well as dynamic bodies
			TriggerVolume::Sptr trigger = bg2->Add<TriggerVolume>();
			trigger->SetFlags(TriggerTypeFlags::Statics | TriggerTypeFlags::Kinematics);
			trigger->AddCollider(BoxCollider::Create(glm::vec3(1.0f)));

			bg2->Add<TriggerVolumeEnterBehaviour>();
		}
		GameObject::Sptr bg3 = scene->CreateGameObject("bg3");
		{
			// Set position in the scene
			// Set position in the scene
			bg3->SetPostion(glm::vec3(-107.7f, -55.830f, -1.7f));
			bg3->SetRotation(glm::vec3(90.f, 0.0f, -180.0f));
			bg3->SetScale(glm::vec3(6.f));

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = bg3->Add<RenderComponent>();
			renderer->SetMesh(backgroundMesh);
			renderer->SetMaterial(backgroundMaterial);

			// Example of a trigger that interacts with static and kinematic bodies as well as dynamic bodies
			TriggerVolume::Sptr trigger = bg3->Add<TriggerVolume>();
			trigger->SetFlags(TriggerTypeFlags::Statics | TriggerTypeFlags::Kinematics);
			trigger->AddCollider(BoxCollider::Create(glm::vec3(1.0f)));

			bg3->Add<TriggerVolumeEnterBehaviour>();
		}

		GameObject::Sptr bg4 = scene->CreateGameObject("bg4");
		{
			// Set position in the scene
			// Set position in the scene
			bg4->SetPostion(glm::vec3(-214.6f, -55.830f, -1.7f));
			bg4->SetRotation(glm::vec3(90.f, 0.0f, -180.0f));
			bg4->SetScale(glm::vec3(6.f));

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = bg4->Add<RenderComponent>();
			renderer->SetMesh(backgroundMesh);
			renderer->SetMaterial(backgroundMaterial);

			// Example of a trigger that interacts with static and kinematic bodies as well as dynamic bodies
			TriggerVolume::Sptr trigger = bg4->Add<TriggerVolume>();
			trigger->SetFlags(TriggerTypeFlags::Statics | TriggerTypeFlags::Kinematics);
			trigger->AddCollider(BoxCollider::Create(glm::vec3(1.0f)));

			bg4->Add<TriggerVolumeEnterBehaviour>();
		}

		GameObject::Sptr bg5 = scene->CreateGameObject("bg5");
		{
			// Set position in the scene
			// Set position in the scene
			bg5->SetPostion(glm::vec3(-321.9f, -55.830f, -1.7f));
			bg5->SetRotation(glm::vec3(90.f, 0.0f, -180.0f));
			bg5->SetScale(glm::vec3(6.f));

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = bg5->Add<RenderComponent>();
			renderer->SetMesh(backgroundMesh);
			renderer->SetMaterial(backgroundMaterial);

			// Example of a trigger that interacts with static and kinematic bodies as well as dynamic bodies
			TriggerVolume::Sptr trigger = bg5->Add<TriggerVolume>();
			trigger->SetFlags(TriggerTypeFlags::Statics | TriggerTypeFlags::Kinematics);
			trigger->AddCollider(BoxCollider::Create(glm::vec3(1.0f)));

			bg5->Add<TriggerVolumeEnterBehaviour>();
		}

		GameObject::Sptr bg6 = scene->CreateGameObject("bg6");
		{
			// Set position in the scene
			// Set position in the scene
			bg6->SetPostion(glm::vec3(-429.2f, -55.830f, -1.7f));
			bg6->SetRotation(glm::vec3(90.f, 0.0f, -180.0f));
			bg6->SetScale(glm::vec3(6.f));

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = bg6->Add<RenderComponent>();
			renderer->SetMesh(backgroundMesh);
			renderer->SetMaterial(backgroundMaterial);

			// Example of a trigger that interacts with static and kinematic bodies as well as dynamic bodies
			TriggerVolume::Sptr trigger = bg6->Add<TriggerVolume>();
			trigger->SetFlags(TriggerTypeFlags::Statics | TriggerTypeFlags::Kinematics);
			trigger->AddCollider(BoxCollider::Create(glm::vec3(1.0f)));

			bg6->Add<TriggerVolumeEnterBehaviour>();
		}

		GameObject::Sptr bg7 = scene->CreateGameObject("bg7");
		{
			// Set position in the scene
			// Set position in the scene
			bg7->SetPostion(glm::vec3(-536.5f, -55.830f, -1.7f));
			bg7->SetRotation(glm::vec3(90.f, 0.0f, -180.0f));
			bg7->SetScale(glm::vec3(6.f));

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = bg7->Add<RenderComponent>();
			renderer->SetMesh(backgroundMesh);
			renderer->SetMaterial(backgroundMaterial);

			// Example of a trigger that interacts with static and kinematic bodies as well as dynamic bodies
			TriggerVolume::Sptr trigger = bg7->Add<TriggerVolume>();
			trigger->SetFlags(TriggerTypeFlags::Statics | TriggerTypeFlags::Kinematics);
			trigger->AddCollider(BoxCollider::Create(glm::vec3(1.0f)));

			bg7->Add<TriggerVolumeEnterBehaviour>();
		}

		GameObject::Sptr Exit = scene->CreateGameObject("Exit");
		{
			// Set position in the scene
			// Set position in the scene
			Exit->SetPostion(glm::vec3(-409.5f, -3.38f, -0.34f));
			Exit->SetRotation(glm::vec3(90.f, 0.0f, 140.0f));
			Exit->SetScale(glm::vec3(3.f));

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = Exit->Add<RenderComponent>();
			renderer->SetMesh(ExitMesh);
			renderer->SetMaterial(ExitMaterial);

			// Example of a trigger that interacts with static and kinematic bodies as well as dynamic bodies
			TriggerVolume::Sptr trigger = Exit->Add<TriggerVolume>();
			trigger->SetFlags(TriggerTypeFlags::Statics | TriggerTypeFlags::Kinematics);
			trigger->AddCollider(BoxCollider::Create(glm::vec3(1.0f)));

			Exit->Add<TriggerVolumeEnterBehaviour>();
		}


		GameObject::Sptr ladybug = scene->CreateGameObject("ladybug");
		{
			// Set position in the scene
			ladybug->SetPostion(glm::vec3(6.f, 0.0f, 1.f));
			ladybug->SetRotation(glm::vec3(90.f, 0.0f, 90.000f));
			ladybug->SetScale({ 0.5f, 0.5f, 0.5f });

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = ladybug->Add<RenderComponent>();
			renderer->SetMesh(ladybugMesh);
			renderer->SetMaterial(ladybugMaterial);

			// Example of a trigger that interacts with static and kinematic bodies as well as dynamic bodies
			TriggerVolume::Sptr trigger = ladybug->Add<TriggerVolume>();
			trigger->SetFlags(TriggerTypeFlags::Statics | TriggerTypeFlags::Kinematics);
			trigger->AddCollider(BoxCollider::Create(glm::vec3(1.0f)));

			ladybug->Add<TriggerVolumeEnterBehaviour>();
		}

		GameObject::Sptr ship = scene->CreateGameObject("Fenrir");
		{
			// Set position in the scene
			ship->SetPostion(glm::vec3(1.5f, 0.0f, 4.0f));
			ship->SetScale(glm::vec3(0.1f));

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = ship->Add<RenderComponent>();
			renderer->SetMesh(shipMesh);
			renderer->SetMaterial(grey);
		}

		GameObject::Sptr demoBase = scene->CreateGameObject("Demo Parent");

		GameObject::Sptr shadowCaster = scene->CreateGameObject("Shadow Light");
		{
			// Set position in the scene
			shadowCaster->SetPostion(glm::vec3(3.0f, 3.0f, 5.0f));
			shadowCaster->LookAt(glm::vec3(0.0f));

			// Create and attach a renderer for the monkey
			ShadowCamera::Sptr shadowCam = shadowCaster->Add<ShadowCamera>();
			shadowCam->SetProjection(glm::perspective(glm::radians(120.0f), 1.0f, 0.1f, 100.0f));
		}

		/////////////////////////// UI //////////////////////////////
		/*
		GameObject::Sptr canvas = scene->CreateGameObject("UI Canvas");
		{
			RectTransform::Sptr transform = canvas->Add<RectTransform>();
			transform->SetMin({ 16, 16 });
			transform->SetMax({ 256, 256 });

			GuiPanel::Sptr canPanel = canvas->Add<GuiPanel>();


			GameObject::Sptr subPanel = scene->CreateGameObject("Sub Item");
			{
				RectTransform::Sptr transform = subPanel->Add<RectTransform>();
				transform->SetMin({ 10, 10 });
				transform->SetMax({ 128, 128 });

				GuiPanel::Sptr panel = subPanel->Add<GuiPanel>();
				panel->SetColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));

				panel->SetTexture(ResourceManager::CreateAsset<Texture2D>("textures/upArrow.png"));

				Font::Sptr font = ResourceManager::CreateAsset<Font>("fonts/Roboto-Medium.ttf", 16.0f);
				font->Bake();

				GuiText::Sptr text = subPanel->Add<GuiText>();
				text->SetText("Hello world!");
				text->SetFont(font);

				monkey1->Get<JumpBehaviour>()->Panel = text;
			}

			canvas->AddChild(subPanel);
		}
		*/

		GameObject::Sptr particles = scene->CreateGameObject("Particles");
		{
			ParticleSystem::Sptr particleManager = particles->Add<ParticleSystem>();  
			particleManager->AddEmitter(glm::vec3(0.0f), glm::vec3(0.0f, -1.0f, 10.0f), 10.0f, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)); 
		}

		GuiBatcher::SetDefaultTexture(ResourceManager::CreateAsset<Texture2D>("textures/ui-sprite.png"));
		GuiBatcher::SetDefaultBorderRadius(8);

		// Save the asset manifest for all the resources we just loaded
		ResourceManager::SaveManifest("scene-manifest.json");
		// Save the scene to a JSON file
		scene->Save("scene.json");

		// Send the scene to the application
		app.LoadScene(scene);
	}
}
