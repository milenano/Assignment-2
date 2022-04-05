#include "ColorCorrectionEffectnoir.h"
#include "Utils/ResourceManager/ResourceManager.h"
#include "Utils/JsonGlmHelpers.h"
#include "Utils/ImGuiHelper.h"

ColorCorrectionEffectnoir::ColorCorrectionEffectnoir() :
	ColorCorrectionEffectnoir(true) { }

ColorCorrectionEffectnoir::ColorCorrectionEffectnoir(bool defaultLut) :
	PostProcessingLayer::Effect(),
	_shader(nullptr),
	_strength(0.4f),
	Lut(nullptr)
{
	Name = "Color Correction noir";
	_format = RenderTargetType::ColorRgb8;

	_shader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
		{ ShaderPartType::Vertex, "shaders/vertex_shaders/fullscreen_quad.glsl" },
		{ ShaderPartType::Fragment, "shaders/fragment_shaders/post_effects/color_correction.glsl" }
	});

	if (defaultLut) {
		Lut = ResourceManager::CreateAsset<Texture3D>("luts/noir.cube");
	}
}

ColorCorrectionEffectnoir::~ColorCorrectionEffectnoir() = default;

void ColorCorrectionEffectnoir::Apply(const Framebuffer::Sptr& gBuffer)
{
	_shader->Bind();
	Lut->Bind(1);
	_shader->SetUniform("u_Strength", _strength);
}

void ColorCorrectionEffectnoir::RenderImGui()
{
	LABEL_LEFT(ImGui::LabelText, "LUT", Lut ? Lut->GetDebugName().c_str() : "none");
	LABEL_LEFT(ImGui::SliderFloat, "Strength", &_strength, 0, 1);
}

ColorCorrectionEffectnoir::Sptr ColorCorrectionEffectnoir::FromJson(const nlohmann::json& data)
{
	ColorCorrectionEffectnoir::Sptr result = std::make_shared<ColorCorrectionEffectnoir>(false);
	result->Enabled = JsonGet(data, "enabled", true);
	result->_strength = JsonGet(data, "strength", result->_strength);
	result->Lut = ResourceManager::Get<Texture3D>(Guid(data["lut"].get<std::string>()));
	return result;
}

nlohmann::json ColorCorrectionEffectnoir::ToJson() const
{
	return {
		{ "enabled", Enabled },
		{ "lut", Lut != nullptr ? Lut->GetGUID().str() : "null" }, 
		{ "strength", _strength }
	};
}
