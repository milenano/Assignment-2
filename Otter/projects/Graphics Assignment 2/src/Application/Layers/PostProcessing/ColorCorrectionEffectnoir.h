#pragma once
#include "Application/Layers/PostProcessingLayer.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/Textures/Texture3D.h"

class ColorCorrectionEffectnoir : public PostProcessingLayer::Effect {
public:
	MAKE_PTRS(ColorCorrectionEffectnoir);
	Texture3D::Sptr Lut;

	ColorCorrectionEffectnoir();
	ColorCorrectionEffectnoir(bool defaultLut);
	virtual ~ColorCorrectionEffectnoir();

	virtual void Apply(const Framebuffer::Sptr& gBuffer) override;
	virtual void RenderImGui() override;

	// Inherited from IResource

	ColorCorrectionEffectnoir::Sptr FromJson(const nlohmann::json& data);
	virtual nlohmann::json ToJson() const override;

protected:
	ShaderProgram::Sptr _shader;
	float _strength;
};

