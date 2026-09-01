#pragma once

#include "Symbol.hpp"

using namespace Fetcko;

class Play : public Symbol {
public:
	Play(AlbumArt *const albumArt) : Symbol(albumArt) {

	}

	void OnInit(float radius) override {
		Symbol::OnInit(radius);

		vao->AddAttribute(VertexArray::Attribute(0, 2, 2 * sizeof(float)));
		OnResize(radius);
	}

	void OnResize(float radius) override {
		Symbol::OnResize(radius);

		std::vector<float> triangleBuffer = {
			static_cast<GLfloat>(-radius / 3 - radius / 8), static_cast<GLfloat>(-radius / 2.2f),
			static_cast<GLfloat>(-radius / 3 - radius / 8), static_cast<GLfloat>(radius / 2.2f) ,
			static_cast<GLfloat>(radius / 3 + radius / 8) , static_cast<GLfloat>(0)
		};

		
		vbo->BufferData(triangleBuffer);
		vbo->Unbind();
		vao->Unbind();
	}

	void OnLoop(float x, float y, const Delta &time, Context &context, const float *alpha = nullptr) override {
		Symbol::OnLoop(x, y, time, context, alpha);

		const auto OutlineColor = alpha ? albumArt->GetBlackColor() : 0.0f;
		const auto HorizontalScale = alpha ? 1.25f : 1.15f;
		const auto VerticalScale = alpha ? 1.45f : 1.2f;

		context.Use("basic"_hash);

		AutoFader::OnLoop(time);

		context.Translate(x, y, 0);
		context.Scale(HorizontalScale, VerticalScale, 1.0f);
		context.Apply();

		context.Color(OutlineColor, OutlineColor, OutlineColor, alpha ? *alpha : this->alpha);

		// Outline
		vao->Bind();
		vbo->DrawArrays(GL_TRIANGLES, 0, 3);
		vao->Unbind();

		context.Color(GetColor().r, GetColor().g, GetColor().b, alpha ? *alpha : this->alpha);
		context.Scale(1.0f / HorizontalScale, 1.0f / VerticalScale, 1.0f);
		context.Apply();

		// Triangle
		vao->Bind();
		vbo->DrawArrays(GL_TRIANGLES, 0, 3);
		vao->Unbind();

		context.Use("texture"_hash);
		context.LoadIdentity();
	}
};