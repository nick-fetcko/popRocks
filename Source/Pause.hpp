#pragma once

#include "Symbol.hpp"

using namespace Fetcko;

class Pause : public Symbol {
public:
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

private:
	std::unique_ptr<ElementBuffer> eab;
};