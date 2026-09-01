#pragma once

#include "Symbol.hpp"

class Next : public Symbol {
public:
	Next(AlbumArt *const albumArt) : Symbol(albumArt) {

	}

	void OnInit(float radius) override {
		rectVao = std::make_unique<VertexArray>();
		rectVbo = std::make_unique<ArrayBuffer>();
		rectEab = std::make_unique<ElementBuffer>();

		rectVao->Bind();
		rectVbo->Bind();
		rectVao->AddAttribute(VertexArray::Attribute(0, 2, 2 * sizeof(float)));
		rectVbo->Unbind();
		rectVao->Unbind();

		rectEab->Bind();
		rectEab->BufferData<std::size(Buffers::SquareBuffer)>(Buffers::SquareBuffer);
		rectEab->Unbind();

		Symbol::OnInit(radius);

		vao->AddAttribute(VertexArray::Attribute(0, 2, 2 * sizeof(float)));
		OnResize(radius);
	}

	void OnDestroy() override {
		rectVao.reset();
		rectVbo.reset();
		rectEab.reset();
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

		std::vector<float> squareBuffer = {
			static_cast<GLfloat>(-radius / 8), static_cast<GLfloat>(-radius / 2),
			static_cast<GLfloat>(-radius / 8), static_cast<GLfloat>(radius / 2) ,
			static_cast<GLfloat>(radius / 8) , static_cast<GLfloat>(radius / 2) ,
			static_cast<GLfloat>(radius / 8) , static_cast<GLfloat>(-radius / 2),
		};

		rectVao->Bind();
		rectVbo->Bind();
		rectVbo->BufferData(squareBuffer);
		rectVbo->Unbind();
		rectVao->Unbind();
	}

	void OnLoop(float x, float y, const Delta &time, Context &context, const float *alpha = nullptr) override {
		Symbol::OnLoop(x, y, time, context, alpha);

		const auto OutlineColor = alpha ? albumArt->GetBlackColor() : 0.0f;

		const auto TriangleHorizontalScale = alpha ? 1.45f : 1.15f;
		const auto TriangleVerticalScale = alpha ? 1.45f : 1.2f;

		const auto RectangleHorizontalScale = alpha ? 2.25f : 1.5f;
		const auto RectangleVerticalScale = alpha ? 1.15f : 1.1f;

		context.Use("basic"_hash);

		AutoFader::OnLoop(time);

		context.Translate(x, y, 0);
		context.Scale(TriangleHorizontalScale, TriangleVerticalScale, 1.0f);
		context.Apply();

		context.Color(OutlineColor, OutlineColor, OutlineColor, alpha ? *alpha : this->alpha);

		// Left Outline
		vao->Bind();
		vbo->DrawArrays(GL_TRIANGLES, 0, 3);
		vao->Unbind();

		context.Color(GetColor().r, GetColor().g, GetColor().b, alpha ? *alpha : this->alpha);
		context.Translate(-TriangleHorizontalScale, 0, 0);
		context.Scale(1.0f / TriangleHorizontalScale, 1.0f / TriangleVerticalScale, 1.0f);
		context.Apply();

		// Left Triangle
		vao->Bind();
		vbo->DrawArrays(GL_TRIANGLES, 0, 3);
		vao->Unbind();

		context.Translate(radius / 1.5f, 0, 0);
		context.Scale(RectangleHorizontalScale, RectangleVerticalScale, 1.0f);
		context.Apply();

		context.Color(OutlineColor, OutlineColor, OutlineColor, alpha ? *alpha : this->alpha);

		// Right outline
		rectVao->Bind();
		rectEab->Bind();
		rectEab->DrawElements(GL_TRIANGLES);
		rectEab->Unbind();
		rectVao->Unbind();

		context.Color(GetColor().r, GetColor().g, GetColor().b, alpha ? *alpha : this->alpha);
		context.Scale(1.0f / RectangleHorizontalScale, 1.0f / RectangleVerticalScale, 1.0f);
		context.Apply();

		// Right rectangle
		rectVao->Bind();
		rectEab->Bind();
		rectEab->DrawElements(GL_TRIANGLES);
		rectEab->Unbind();
		rectVao->Unbind();

		context.Use("texture"_hash);
		context.LoadIdentity();
	}

protected:
	std::unique_ptr<VertexArray> rectVao;
	std::unique_ptr<ArrayBuffer> rectVbo;
	std::unique_ptr<ElementBuffer> rectEab;
};