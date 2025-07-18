#pragma once

#include <string>

#include <SDL_ttf.h>
#include <glad/glad.h>

#include "MathCPP/Colour.hpp"
#include "MathCPP/Vector.hpp"

using namespace MathsCPP;

class Text {
public:
	void OnInit(TTF_Font *font);
	void OnLoop(int x, int y) const;
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
	float rect[8] = { 0 };

	std::string text;

	Vector2i size = { 0, 0 };
	Colour<float> color = { 1.0f, 1.0f, 1.0f, 1.0f };
};