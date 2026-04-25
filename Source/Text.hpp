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

class Text : public OpenGLFont::SizeChangedListener, public LoggableClass {
public:
	Text() = default;
	Text(Text &&other) noexcept {
		context = other.context;
		font = other.font;
		bounds = std::move(other.bounds);
		cached = std::move(other.cached);
		text = std::move(other.text);
		altText = std::move(other.altText);
		size = std::move(other.size);
		color = std::move(other.color);
		buffer = other.buffer;

#ifdef _DEBUG
		other.destroyed = true;
		destroyed = false;
#endif

		font->RemoveSizeChangedListener(&other);
		font->AddSizeChangedListener(this);
	}
	Text(const Text &) = default;

#ifdef _DEBUG
	virtual ~Text();
#endif

	void OnInit(OpenGLFont *font, Context *context);
	void OnLoop(int x, int y) const;
	void OnDestroy();

	Vector2i MeasureText(const std::string &text) const;
	bool SetText(const std::string &text, bool force = false);
	const std::string &GetText() const { return text; }

	const Vector2i &GetSize() const { return size; }

	bool Empty() const { return text.empty(); }

	void SetColor(const Colour<float> &color) { this->color = color; }

	std::unique_ptr<FramebufferObject> &GetCached() { return cached; };

	const OpenGLFont::Bounds &GetBounds() const { return bounds; }

	void OnSizeChanged(FT_UInt size) override;

	const OpenGLFont *GetFont() const { return font; }

	void SetAltText(const std::string &altText);
	const std::string &GetAltText() const { return altText; }

	virtual void SetFont(OpenGLFont *font);

protected:
	Context *context = nullptr;
	OpenGLFont *font = nullptr;

	OpenGLFont::Bounds bounds;
	std::unique_ptr<FramebufferObject> cached;

	std::string text;
	std::string altText;

	Vector2i size = { 0, 0 };
	Colour<float> color = { 1.0f, 1.0f, 1.0f, 1.0f };

	float *buffer = nullptr;

#ifdef _DEBUG
	bool destroyed = false;
#endif
};