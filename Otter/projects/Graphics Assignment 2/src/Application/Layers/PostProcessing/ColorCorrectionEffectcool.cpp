#include "ColorCorrectionEffectcool.h"
#include "Utils/ResourceManager/ResourceManager.h"
#include "Utils/JsonGlmHelpers.h"
#include "Utils/ImGuiHelper.h"

ColorCorrectionEffectcool::ColorCorrectionEffectcool() :
	ColorCorrectionEffectcool(true) { }

ColorCorrectionEffectcool::ColorCorrectionEffectcool(bool defaultLut) :
	PostProcessingLayer::Effect(),
	_shader(nullptr),
	_strength(0.4f),
	Lut(nullptr)
{
	Name = "Color Correction cool";
	_format = RenderTargetType::ColorRgb8;

	_shader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
		{ ShaderPartType::Vertex, "shaders/vertex_shaders/fullscreen_quad.glsl" },
		{ ShaderPartType::Fragment, "shaders/fragment_shaders/post_effects/color_correction.glsl" }
	});

	if (defaultLut) {
		Lut = ResourceManager::CreateAsset<Texture3D>("luts/cool.cube");
	}
}

ColorCorrectionEffectcool::~ColorCorrectionEffectcool() = default;

void ColorCorrectionEffectcool::Apply(const Framebuffer::Sptr& gBuffer)
{
	_shader->Bind();
	Lut->Bind(1);
	_shader->SetUniform("u_Strength", _strength);
}

void ColorCorrectionEffectcool::RenderImGui()
{
	LABEL_LEFT(ImGui::LabelText, "LUT", Lut ? Lut->GetDebugName().c_str() : "none");
	LABEL_LEFT(ImGui::SliderFloat, "Strength", &_strength, 0, 1);
}

ColorCorrectionEffectcool::Sptr ColorCorrectionEffectcool::FromJson(const nlohmann::json& data)
{
	ColorCorrectionEffectcool::Sptr result = std::make_shared<ColorCorrectionEffectcool>(false);
	result->Enabled = JsonGet(data, "enabled", true);
	result->_strength = JsonGet(data, "strength", result->_strength);
	result->Lut = ResourceManager::Get<Texture3D>(Guid(data["lut"].get<std::string>()));
	return result;
}

nlohmann::json ColorCorrectionEffectcool::ToJson() const
{
	return {
		{ "enabled", Enabled },
		{ "lut", Lut != nullptr ? Lut->GetGUID().str() : "null" }, 
		{ "strength", _strength }
	};
}
