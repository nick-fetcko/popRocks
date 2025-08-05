#pragma once

#include <memory>

#include "MathCPP/Maths.hpp"

#include "OpenGL/Buffer.hpp"
#include "OpenGL/Context.hpp"
#include "OpenGL/VertexArray.hpp"

using namespace MathsCPP;
using namespace Fetcko;

enum class Circles : uint8_t {
	Plain = 2,
	Textured = 4
};

template<Circles T>
class Circle {
public:
	void OnInit(float radius) {
		constexpr uint8_t Stride = static_cast<uint8_t>(T);

		this->radius = radius;

		std::vector<float> buffer(362 * Stride);

		buffer[0] = 0;
		buffer[1] = 0;

		if constexpr (T == Circles::Textured)
			buffer[2] = buffer[3] = 0.5;

		float degInRad;
		for (int i = 1; i <= 360; i++) {
			degInRad = i * Maths::DEG2RAD<float>;

			buffer[(i * Stride)] = sin(degInRad) * radius;
			buffer[(i * Stride) + 1] = cos(degInRad) * radius;

			if constexpr (T == Circles::Textured) {
				buffer[(i * Stride) + 2] = 0.5f + 0.5f * sin(degInRad);
				buffer[(i * Stride) + 3] = 0.5f + 0.5f * cos(degInRad);
			}
		}

		buffer[361 * Stride] = buffer[Stride];
		buffer[361 * Stride + 1] = buffer[Stride + 1];

		if (T == Circles::Textured) {
			buffer[361 * Stride + 2] = buffer[Stride + 2];
			buffer[361 * Stride + 3] = buffer[Stride + 3];
		}

		vao = std::make_unique<VertexArray>();
		vbo = std::make_unique<ArrayBuffer>();

		vao->Bind();
		vbo->Bind();
		vao->AddAttribute(VertexArray::Attribute(0, 2, Stride * sizeof(float)));

		if constexpr (T == Circles::Textured)
			vao->AddAttribute(VertexArray::Attribute(1, 2, Stride * sizeof(float), 2 * sizeof(float)));

		vbo->BufferData(buffer);
		vbo->Unbind();
		vao->Unbind();
	}

	virtual void OnDestroy() {
		vao.reset();
		vbo.reset();
	}

	virtual void SetRadius(float radius) {
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
	virtual void UpdateVertexCoords() {
		constexpr uint8_t Stride = static_cast<uint8_t>(T);

		float degInRad = 0.0f;
		float data[2];

		vbo->Bind();
		for (int i = 1; i <= 360; i++) {
			degInRad = i * Maths::DEG2RAD<float>;
			data[0] = sin(degInRad) * radius;
			data[1] = cos(degInRad) * radius;

			vbo->BufferSubData(i * Stride, sizeof(data), data);
			if (i == 1) vbo->BufferSubData(361 * Stride, sizeof(data), data);
		}
		vbo->Unbind();
	}

	virtual void UpdateTextureCoords(int width, int height, float aspectRatio) {
		constexpr uint8_t Stride = static_cast<uint8_t>(T);

		float degInRad = 0.0f;
		float data[2];

		vbo->Bind();
		for (int i = 1; i <= 360; i++) {
			degInRad = i * Maths::DEG2RAD<float>;

			// Switch dimensions based on which is larger
			//
			// This prefers "filling out" the circle over
			// letterboxing / pillarboxing inside of it
			if (width > height) {
				data[0] = 0.5f + 0.5f * sin(degInRad) / aspectRatio;
				data[1] = 0.5f + 0.5f * cos(degInRad);
			} else if (height > width) {
				data[0] = 0.5f + 0.5f * sin(degInRad);
				data[1] = 0.5f + 0.5f * cos(degInRad) * aspectRatio;
			} else { // If they're equal, we're 1:1
				data[0] = 0.5f + 0.5f * sin(degInRad);
				data[1] = 0.5f + 0.5f * cos(degInRad);
			}

			vbo->BufferSubData(i * Stride + 2, sizeof(data), data);
			if (i == 1) vbo->BufferSubData(361 * Stride + 2, sizeof(data), data);
		}
		vbo->Unbind();
	}

	float radius = 0.0f;

	std::unique_ptr<VertexArray> vao;
	std::unique_ptr<ArrayBuffer> vbo;
};