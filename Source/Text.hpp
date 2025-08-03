#pragma once

#include <string>

#include <glad/glad.h>

#include "MathCPP/Colour.hpp"
#include "MathCPP/Vector.hpp"

#include "OpenGL/Buffer.hpp"
#include "OpenGL/Context.hpp"
#include "OpenGL/OpenGLFont.hpp"
#include "OpenGL/VertexArray.hpp"

using namespace MathsCPP;
using namespace Fetcko;

class Text {
public:
	void OnInit(OpenGLFont *font, Context *context);
	void OnLoop(int x, int y) const;
	void OnDestroy();

	Vector2i MeasureText(const std::string &text) const;
	void SetText(const std::string &text, bool force = false);
	const std::string &GetText() const { return text; }

	const Vector2i &GetSize() const { return size; }

	bool Empty() const { return text.empty(); }

	void SetColor(const Colour<float> &color) { this->color = color; }

	std::unique_ptr<Framebuffer> &GetCached() { return cached; };

	const OpenGLFont::Bounds &GetBounds() const { return bounds; }

private:
	Context *context = nullptr;
	OpenGLFont *font = nullptr;

	GLuint texture = 0;
	OpenGLFont::Bounds bounds;
	std::unique_ptr<Framebuffer> cached;

	std::string text;

	Vector2i size = { 0, 0 };
	Colour<float> color = { 1.0f, 1.0f, 1.0f, 1.0f };

	float *buffer = nullptr;
};