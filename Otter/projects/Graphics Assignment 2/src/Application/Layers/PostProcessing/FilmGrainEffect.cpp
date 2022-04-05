#include "FilmGrainEffect.h"

#include "Utils/JsonGlmHelpers.h"
#include "Utils/ImGuiHelper.h"
#include "Application/Layers/RenderLayer.h"
#include "Application/Application.h"

FilmGrainEffect::FilmGrainEffect() :
	PostProcessingLayer::Effect(),
	_shader(nullptr)
{
	Name = "Film Grain";
	_format = RenderTargetType::ColorRgb8;

	_shader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
		{ ShaderPartType::Vertex, "shaders/vertex_shaders/fullscreen_quad.glsl" },
		{ ShaderPartType::Fragment, "shaders/fragment_shaders/post_effects/filmgrain.glsl" }
	});
}

FilmGrainEffect::~FilmGrainEffect() = default;

void FilmGrainEffect::Apply(const Framebuffer::Sptr & gBuffer)
{
	_shader->Bind();
	gBuffer->BindAttachment(RenderTargetAttachment::Depth, 1);
}

void FilmGrainEffect::RenderImGui()
{
	const auto& cam = Application::Get().CurrentScene()->MainCamera;

	if (cam != nullptr) {
	}
}

FilmGrainEffect::Sptr FilmGrainEffect::FromJson(const nlohmann::json & data)
{
	FilmGrainEffect::Sptr result = std::make_shared<FilmGrainEffect>();
	result->Enabled = JsonGet(data, "enabled", true);
	return result;
}

nlohmann::json FilmGrainEffect::ToJson() const
{
	return {
		{ "enabled", Enabled }
	};
}
