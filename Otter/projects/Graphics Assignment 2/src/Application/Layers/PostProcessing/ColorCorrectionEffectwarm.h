#pragma once
#include "Application/Layers/PostProcessingLayer.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/Textures/Texture3D.h"

class ColorCorrectionEffectwarm : public PostProcessingLayer::Effect {
public:
	MAKE_PTRS(ColorCorrectionEffectwarm);
	Texture3D::Sptr Lut;

	ColorCorrectionEffectwarm();
	ColorCorrectionEffectwarm(bool defaultLut);
	virtual ~ColorCorrectionEffectwarm();

	virtual void Apply(const Framebuffer::Sptr& gBuffer) override;
	virtual void RenderImGui() override;

	// Inherited from IResource

	ColorCorrectionEffectwarm::Sptr FromJson(const nlohmann::json& data);
	virtual nlohmann::json ToJson() const override;

protected:
	ShaderProgram::Sptr _shader;
	float _strength;
};

