#pragma once

#include "OpenGL/Buffer.hpp"
#include "OpenGL/Context.hpp"
#include "OpenGL/VertexArray.hpp"

#include "AutoFader.hpp"
#include "ColorChangeListener.hpp"

using namespace Fetcko;

class Pause : public AutoFader<false>, public ColorChangeListener {
public:
	Pause() {
		autoFadeSpeed = 5.0f;
		waitTime = 0s;

		alpha = 0.0f;
	}

	void OnInit(float radius) {
		vao = std::make_unique<VertexArray>();
		vbo = std::make_unique<ArrayBuffer>();
		eab = std::make_unique<ElementBuffer>();

		vao->Bind();
		vbo->Bind();
		vao->AddAttribute(VertexArray::Attribute(0, 2, 2 * sizeof(float)));
		OnResize(radius);

		eab->Bind();
		eab->BufferData<std::size(Buffers::SquareBuffer)>(Buffers::SquareBuffer);
		eab->Unbind();
	}

	void OnResize(float radius) {
		this->radius = radius;

		std::vector<float> squareBuffer = {
			static_cast<GLfloat>(-radius / 8), static_cast<GLfloat>(-radius / 2),
			static_cast<GLfloat>(-radius / 8), static_cast<GLfloat>(radius / 2) ,
			static_cast<GLfloat>(radius / 8) , static_cast<GLfloat>(radius / 2) ,
			static_cast<GLfloat>(radius / 8) , static_cast<GLfloat>(-radius / 2),
		};

		vao->Bind();
		vbo->Bind();
		vbo->BufferData(squareBuffer);
		vbo->Unbind();
		vao->Unbind();
	}

	void OnLoop(float x, float y, const Delta &time, Context &context) {
		context.Use("basic"_hash);

		AutoFader::OnLoop(time);

		context.Translate(x - radius / 3.0f, y, 0);
		context.Scale(1.5f, 1.1f, 1.0f);
		context.Apply();

		context.Color(0.0f, 0.0f, 0.0f, alpha);

		// Left outline
		vao->Bind();
		eab->Bind();
		eab->DrawElements(GL_TRIANGLES);
		eab->Unbind();
		vao->Unbind();

		context.Color(color.r, color.g, color.b, alpha);
		context.Scale(1.0f / 1.5f, 1.0f / 1.1f, 1.0f);
		context.Apply();

		// Left rectangle
		vao->Bind();
		eab->Bind();
		eab->DrawElements(GL_TRIANGLES);
		eab->Unbind();
		vao->Unbind();

		context.Translate(radius / 3.0f * 2.0f, 0, 0);
		context.Scale(1.5f, 1.1f, 1.0f);
		context.Apply();

		context.Color(0.0f, 0.0f, 0.0f, alpha);

		// Right outline
		vao->Bind();
		eab->Bind();
		eab->DrawElements(GL_TRIANGLES);
		eab->Unbind();
		vao->Unbind();

		context.Color(color.r, color.g, color.b, alpha);
		context.Scale(1.0f / 1.5f, 1.0f / 1.1f, 1.0f);
		context.Apply();

		// Right rectangle
		vao->Bind();
		eab->Bind();
		eab->DrawElements(GL_TRIANGLES);
		eab->Unbind();
		vao->Unbind();

		context.Use("texture"_hash);
		context.LoadIdentity();
	}

	void OnDestroy() {
		vao.reset();
		vbo.reset();
		eab.reset();
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
	std::unique_ptr<ElementBuffer> eab;
};