#include "ColorCorrectionEffectwarm.h"
#include "Utils/ResourceManager/ResourceManager.h"
#include "Utils/JsonGlmHelpers.h"
#include "Utils/ImGuiHelper.h"

ColorCorrectionEffectwarm::ColorCorrectionEffectwarm() :
	ColorCorrectionEffectwarm(true) { }

ColorCorrectionEffectwarm::ColorCorrectionEffectwarm(bool defaultLut) :
	PostProcessingLayer::Effect(),
	_shader(nullptr),
	_strength(0.4f),
	Lut(nullptr)
{
	Name = "Color Correction warm";
	_format = RenderTargetType::ColorRgb8;

	_shader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
		{ ShaderPartType::Vertex, "shaders/vertex_shaders/fullscreen_quad.glsl" },
		{ ShaderPartType::Fragment, "shaders/fragment_shaders/post_effects/color_correction.glsl" }
	});

	if (defaultLut) {
		Lut = ResourceManager::CreateAsset<Texture3D>("luts/warm.cube");
	}
}

ColorCorrectionEffectwarm::~ColorCorrectionEffectwarm() = default;

void ColorCorrectionEffectwarm::Apply(const Framebuffer::Sptr& gBuffer)
{
	_shader->Bind();
	Lut->Bind(1);
	_shader->SetUniform("u_Strength", _strength);
}

void ColorCorrectionEffectwarm::RenderImGui()
{
	LABEL_LEFT(ImGui::LabelText, "LUT", Lut ? Lut->GetDebugName().c_str() : "none");
	LABEL_LEFT(ImGui::SliderFloat, "Strength", &_strength, 0, 1);
}

ColorCorrectionEffectwarm::Sptr ColorCorrectionEffectwarm::FromJson(const nlohmann::json& data)
{
	ColorCorrectionEffectwarm::Sptr result = std::make_shared<ColorCorrectionEffectwarm>(false);
	result->Enabled = JsonGet(data, "enabled", true);
	result->_strength = JsonGet(data, "strength", result->_strength);
	result->Lut = ResourceManager::Get<Texture3D>(Guid(data["lut"].get<std::string>()));
	return result;
}

nlohmann::json ColorCorrectionEffectwarm::ToJson() const
{
	return {
		{ "enabled", Enabled },
		{ "lut", Lut != nullptr ? Lut->GetGUID().str() : "null" }, 
		{ "strength", _strength }
	};
}
