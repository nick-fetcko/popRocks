#pragma once

#include "Symbol.hpp"

using namespace Fetcko;

class Pause : public Symbol {
public:
	Pause(AlbumArt *const albumArt) : Symbol(albumArt) {

	}

	void OnInit(float radius) override {
		Symbol::OnInit(radius);

		eab = std::make_unique<ElementBuffer>();

		vao->AddAttribute(VertexArray::Attribute(0, 2, 2 * sizeof(float)));
		OnResize(radius);

		eab->Bind();
		eab->BufferData<std::size(Buffers::SquareBuffer)>(Buffers::SquareBuffer);
		eab->Unbind();
	}

	void OnDestroy() override {
		Symbol::OnDestroy();

		eab.reset();
	}

	void OnResize(float radius) override {
		Symbol::OnResize(radius);

		std::vector<float> squareBuffer = {
			static_cast<GLfloat>(-radius / 8), static_cast<GLfloat>(-radius / 2),
			static_cast<GLfloat>(-radius / 8), static_cast<GLfloat>(radius / 2) ,
			static_cast<GLfloat>(radius / 8) , static_cast<GLfloat>(radius / 2) ,
			static_cast<GLfloat>(radius / 8) , static_cast<GLfloat>(-radius / 2),
		};

		vbo->BufferData(squareBuffer);
		vbo->Unbind();
		vao->Unbind();
	}

	void OnLoop(float x, float y, const Delta &time, Context &context, const float *alpha = nullptr) override {
		Symbol::OnLoop(x, y, time, context, alpha);

		const auto OutlineColor = alpha ? albumArt->GetBlackColor() : 0.0f;
		const auto HorizontalScale = alpha ? 2.25f : 1.5f;
		const auto VerticalScale = alpha ? 1.2f : 1.1f;

		context.Use("basic"_hash);

		AutoFader::OnLoop(time);

		context.Translate(x - radius / 3.0f, y, 0);
		context.Scale(HorizontalScale, VerticalScale, 1.0f);
		context.Apply();

		context.Color(OutlineColor, OutlineColor, OutlineColor, alpha ? *alpha : this->alpha);

		// Left outline
		vao->Bind();
		eab->Bind();
		eab->DrawElements(GL_TRIANGLES);
		eab->Unbind();
		vao->Unbind();

		context.Color(GetColor().r, GetColor().g, GetColor().b, alpha ? *alpha : this->alpha);
		context.Scale(1.0f / HorizontalScale, 1.0f / VerticalScale, 1.0f);
		context.Apply();

		// Left rectangle
		vao->Bind();
		eab->Bind();
		eab->DrawElements(GL_TRIANGLES);
		eab->Unbind();
		vao->Unbind();

		context.Translate(radius / 3.0f * 2.0f, 0, 0);
		context.Scale(HorizontalScale, VerticalScale, 1.0f);
		context.Apply();

		context.Color(OutlineColor, OutlineColor, OutlineColor, alpha ? *alpha : this->alpha);

		// Right outline
		vao->Bind();
		eab->Bind();
		eab->DrawElements(GL_TRIANGLES);
		eab->Unbind();
		vao->Unbind();

		context.Color(GetColor().r, GetColor().g, GetColor().b, alpha ? *alpha : this->alpha);
		context.Scale(1.0f / HorizontalScale, 1.0f / VerticalScale, 1.0f);
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

private:
	std::unique_ptr<ElementBuffer> eab;
};