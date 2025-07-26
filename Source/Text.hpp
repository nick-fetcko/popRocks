#pragma once

#include <string>

#include <SDL_ttf.h>
#include <glad/glad.h>

#include "MathCPP/Colour.hpp"
#include "MathCPP/Vector.hpp"

#include "OpenGL/Buffer.hpp"
#include "OpenGL/Context.hpp"
#include "OpenGL/VertexArray.hpp"

using namespace MathsCPP;
using namespace Fetcko;

class Text {
public:
	void OnInit(TTF_Font *font);
	void OnLoop(int x, int y, Context &context) const;
	void OnDestroy();

	Vector2i MeasureText(const std::string &text) const;
	void SetText(const std::string &text, bool force = false);
	const std::string &GetText() const { return text; }

	const Vector2i &GetSize() const { return size; }

	bool Empty() const { return text.empty(); }

	void SetColor(const Colour<float> &color) { this->color = color; }

private:
	TTF_Font *font = nullptr;

	GLuint texture = 0;

	std::string text;

	Vector2i size = { 0, 0 };
	Colour<float> color = { 1.0f, 1.0f, 1.0f, 1.0f };

	std::unique_ptr<VertexArray> vao;
	std::unique_ptr<ArrayBuffer> vbo;
	std::unique_ptr<ElementBuffer> eab;

	float *buffer = nullptr;
};