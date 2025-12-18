#pragma once

#include "Symbol.hpp"

using namespace Fetcko;

class Play : public Symbol {
public:
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

		// Triangle
		vao->Bind();
		vbo->DrawArrays(GL_TRIANGLES, 0, 3);
		vao->Unbind();

		context.Use("texture"_hash);
		context.LoadIdentity();
	}
};