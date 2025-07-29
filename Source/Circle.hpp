#pragma once

#include <memory>

#include "MathCPP/Maths.hpp"

#include "OpenGL/Buffer.hpp"
#include "OpenGL/Context.hpp"
#include "OpenGL/VertexArray.hpp"

using namespace MathsCPP;
using namespace Fetcko;

// TODO: Allow the center album art circle to use this, too
class Circle {
public:
	void OnInit(float radius) {
		this->radius = radius;

		std::vector<float> buffer(362 * 2);

		buffer[0] = 0;
		buffer[1] = 0;

		//buffer[2] = buffer[3] = 0.5;

		float degInRad;
		for (int i = 1; i <= 360; i++) {
			degInRad = i * Maths::DEG2RAD<float>;

			buffer[(i * 2)] = sin(degInRad) * radius;
			buffer[(i * 2) + 1] = cos(degInRad) * radius;

			//buffer[(i * 4) + 2] = 0.5f + 0.5f * sin(degInRad);
			//buffer[(i * 4) + 3] = 0.5f + 0.5f * cos(degInRad);
		}

		buffer[361 * 2] = buffer[2];
		buffer[361 * 2 + 1] = buffer[3];

		//buffer[361 * 4] = buffer[4];
		//buffer[361 * 4 + 1] = buffer[5];

		//buffer[361 * 4 + 2] = buffer[6];
		//buffer[361 * 4 + 3] = buffer[7];

		vao = std::make_unique<VertexArray>();
		vbo = std::make_unique<ArrayBuffer>();

		vao->Bind();
		vbo->Bind();
		vao->AddAttribute(VertexArray::Attribute(0, 2, 2 * sizeof(float)));
		//vao->AddAttribute(VertexArray::Attribute(1, 2, 4 * sizeof(float), 2 * sizeof(float)));
		vbo->BufferData(buffer);
		vbo->Unbind();
		vao->Unbind();
	}

	void OnDestroy() {
		vao.reset();
		vbo.reset();
	}

	void SetRadius(float radius) {
		this->radius = radius;
		UpdateVertexCoords();
	}

	void OnLoop(float x, float y, Context &context) {
		context.Translate(
			x,
			y,
			0
		);
		context.Apply();

		vao->Bind();
		vbo->DrawArrays(GL_TRIANGLE_FAN, 0, 362);
		vao->Unbind();

		context.LoadIdentity();
	}

	const float &GetRadius() const { return radius; }

protected:
	void UpdateVertexCoords() {
		float degInRad = 0.0f;
		float data[2];

		vbo->Bind();
		for (int i = 1; i <= 360; i++) {
			degInRad = i * Maths::DEG2RAD<float>;
			data[0] = sin(degInRad) * radius;
			data[1] = cos(degInRad) * radius;

			vbo->BufferSubData(i * 2, sizeof(data), data);
			if (i == 1) vbo->BufferSubData(361 * 2, sizeof(data), data);
		}
		vbo->Unbind();
	}

	float radius = 0.0f;

	std::unique_ptr<VertexArray> vao;
	std::unique_ptr<ArrayBuffer> vbo;
};