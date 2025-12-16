#pragma once

#include "OpenGL/Buffer.hpp"
#include "OpenGL/Context.hpp"
#include "OpenGL/VertexArray.hpp"

#include "AutoFader.hpp"
#include "ColorChangeListener.hpp"

using namespace Fetcko;

class Play : public AutoFader<false>, public ColorChangeListener {
public:
	Play() {
		autoFadeSpeed = 5.0f;
		waitTime = 0s;

		alpha = 0.0f;
	}

	void OnInit(float radius) {
		vao = std::make_unique<VertexArray>();
		vbo = std::make_unique<ArrayBuffer>();

		vao->Bind();
		vbo->Bind();
		vao->AddAttribute(VertexArray::Attribute(0, 2, 2 * sizeof(float)));
		OnResize(radius);
	}

	void OnResize(float radius) {
		this->radius = radius;

		std::vector<float> triangleBuffer = {
			static_cast<GLfloat>(-radius / 3 - radius / 8), static_cast<GLfloat>(-radius / 2.2f),
			static_cast<GLfloat>(-radius / 3 - radius / 8), static_cast<GLfloat>(radius / 2.2f) ,
			static_cast<GLfloat>(radius / 3 + radius / 8) , static_cast<GLfloat>(0)
		};

		vao->Bind();
		vbo->Bind();
		vbo->BufferData(triangleBuffer);
		vbo->Unbind();
		vao->Unbind();
	}

	void OnLoop(float x, float y, const Delta &time, Context &context) {
		context.Use("basic"_hash);

		AutoFader::OnLoop(time);

		context.Translate(x, y, 0);
		context.Scale(1.15f, 1.2f, 1.0f);
		context.Apply();

		context.Color(0.0f, 0.0f, 0.0f, alpha);

		// Outline
		vao->Bind();
		vbo->DrawArrays(GL_TRIANGLES, 0, 3);
		vao->Unbind();

		context.Color(color.r, color.g, color.b, alpha);
		context.Scale(1.0f / 1.15f, 1.0f / 1.2f, 1.0f);
		context.Apply();

		// Rectangle
		vao->Bind();
		vbo->DrawArrays(GL_TRIANGLES, 0, 3);
		vao->Unbind();

		context.Use("texture"_hash);
		context.LoadIdentity();
	}

	void OnDestroy() {
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

private:
	float radius = 1.0f;

	Colour<float> color = Colour<float>::White;

	std::unique_ptr<VertexArray> vao;
	std::unique_ptr<ArrayBuffer> vbo;
};