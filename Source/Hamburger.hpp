#pragma once

#include "Symbol.hpp"

class Hamburger : public Symbol {
public:
	Hamburger(AlbumArt *const albumArt) : Symbol(albumArt) {
		alpha = 1.0f;
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
			static_cast<GLfloat>(-radius / 4), static_cast<GLfloat>(-radius / 24),
			static_cast<GLfloat>(-radius / 4), static_cast<GLfloat>(radius / 24) ,
			static_cast<GLfloat>(radius / 4) , static_cast<GLfloat>(radius / 24) ,
			static_cast<GLfloat>(radius / 4) , static_cast<GLfloat>(-radius / 24),
		};

		vbo->BufferData(squareBuffer);
		vbo->Unbind();
		vao->Unbind();
	}

	const float GetRadius() const override { return radius / 2; }

	void OnLoop(float x, float y, const Delta &time, Context &context, const float *alpha = nullptr) override {
		Symbol::OnLoop(x, y, time, context, alpha);

		const auto OutlineColor = alpha ? albumArt->GetBlackColor() : 0.0f;
		const auto HorizontalScale = alpha ? 1.5f : 1.5f;
		const auto VerticalScale = alpha ? 2.75f : 2.75f;

		context.Use("basic"_hash);

		AutoFader::OnLoop(time);

		context.Translate(x, y - radius / 3 + radius / 12, 0);
		context.Scale(HorizontalScale, VerticalScale, 1.0f);
		context.Apply();

		context.Color(OutlineColor, OutlineColor, OutlineColor, alpha ? *alpha : this->alpha);

		// Top outline
		vao->Bind();
		eab->Bind();
		eab->DrawElements(GL_TRIANGLES);
		eab->Unbind();
		vao->Unbind();

		context.Color(GetColor().r, GetColor().g, GetColor().b, alpha ? *alpha : this->alpha);
		context.Scale(1.0f / HorizontalScale, 1.0f / VerticalScale, 1.0f);
		context.Apply();

		// Top rectangle
		vao->Bind();
		eab->Bind();
		eab->DrawElements(GL_TRIANGLES);
		eab->Unbind();
		vao->Unbind();

		context.Translate(0, radius / 3 - radius / 12, 0);
		context.Scale(HorizontalScale, VerticalScale, 1.0f);
		context.Apply();

		context.Color(OutlineColor, OutlineColor, OutlineColor, alpha ? *alpha : this->alpha);

		// Middle outline
		vao->Bind();
		eab->Bind();
		eab->DrawElements(GL_TRIANGLES);
		eab->Unbind();
		vao->Unbind();

		context.Color(GetColor().r, GetColor().g, GetColor().b, alpha ? *alpha : this->alpha);
		context.Scale(1.0f / HorizontalScale, 1.0f / VerticalScale, 1.0f);
		context.Apply();

		// Middle rectangle
		vao->Bind();
		eab->Bind();
		eab->DrawElements(GL_TRIANGLES);
		eab->Unbind();
		vao->Unbind();

		context.Translate(0, radius / 3 - radius / 12, 0);
		context.Scale(HorizontalScale, VerticalScale, 1.0f);
		context.Apply();

		context.Color(OutlineColor, OutlineColor, OutlineColor, alpha ? *alpha : this->alpha);

		// Bottom outline
		vao->Bind();
		eab->Bind();
		eab->DrawElements(GL_TRIANGLES);
		eab->Unbind();
		vao->Unbind();

		context.Color(GetColor().r, GetColor().g, GetColor().b, alpha ? *alpha : this->alpha);
		context.Scale(1.0f / HorizontalScale, 1.0f / VerticalScale, 1.0f);
		context.Apply();

		// Bottom rectangle
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
