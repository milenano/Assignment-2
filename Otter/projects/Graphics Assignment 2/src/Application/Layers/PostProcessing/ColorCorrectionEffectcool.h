#pragma once
#include "Application/Layers/PostProcessingLayer.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/Textures/Texture3D.h"

class ColorCorrectionEffectcool : public PostProcessingLayer::Effect {
public:
	MAKE_PTRS(ColorCorrectionEffectcool);
	Texture3D::Sptr Lut;

	ColorCorrectionEffectcool();
	ColorCorrectionEffectcool(bool defaultLut);
	virtual ~ColorCorrectionEffectcool();

	virtual void Apply(const Framebuffer::Sptr& gBuffer) override;
	virtual void RenderImGui() override;

	// Inherited from IResource

	ColorCorrectionEffectcool::Sptr FromJson(const nlohmann::json& data);
	virtual nlohmann::json ToJson() const override;

protected:
	ShaderProgram::Sptr _shader;
	float _strength;
};

