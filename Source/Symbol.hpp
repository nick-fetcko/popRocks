#pragma once

#include "OpenGL/Buffer.hpp"
#include "OpenGL/Context.hpp"
#include "OpenGL/VertexArray.hpp"

#include "AutoFader.hpp"
#include "ColorChangeListener.hpp"
#include "HDR.hpp"

class Symbol : public AutoFader<false>, public ColorChangeListener {
public:
	Symbol() {
		autoFadeSpeed = 5.0f;
		waitTime = 0s;

		alpha = 0.0f;
	}

	virtual void OnInit(float radius) {
		vao = std::make_unique<VertexArray>();
		vbo = std::make_unique<ArrayBuffer>();

		vao->Bind();
		vbo->Bind();
	}

	virtual void OnResize(float radius) {
		this->radius = radius;

		vao->Bind();
		vbo->Bind();
	}

	virtual void OnDestroy() {
		vao.reset();
		vbo.reset();
	}

	void OnColorChanged(const Colour<float> &color, bool silent) {
		auto hsv = color.ToHsv();
		hsv.v = 1.0f;
		this->color = Colour<float>::FromHsv(hsv.h, hsv.s, hsv.v);

		if (HDR::Enabled) {
			this->color.Tone(
				Settings::settings.GetAlbumArtGamma(),
				Settings::settings.GetAlbumArtContrast(),
				Settings::settings.GetAlbumArtBrightness(),
				HDR::WhiteLevel * HDR::Headroom
			);
		}
	}

protected:
	float radius = 1.0f;

	Colour<float> color = Colour<float>::White;

	std::unique_ptr<VertexArray> vao;
	std::unique_ptr<ArrayBuffer> vbo;
};