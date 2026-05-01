#include "ScrollingText.hpp"

#include "CApp.h"

void ScrollingText::OnResize(int windowWidth, int windowHeight) {
	this->windowHeight = windowHeight;
}

void ScrollingText::OnLoop(int x, int y, const Delta & time) {
	if (widthDelta > 0) {
		// Center horizontally
		x += (bounds.width - maxWidth) / 2 - font->GetOutlineRadius();

		// FIXME: This is just copy-pasted from Text::OnLoop
		//        Make this DRY!
		auto centeredY = y + (bounds.height - bounds.renderedHeight) / 2.0f;

		if (bounds.overhang)
			centeredY -= std::floor((bounds.renderedHeight - bounds.overhang) / 2.0f) - std::ceil(bounds.overhang / 2.0f);
		else
			centeredY -= bounds.renderedHeight / 2.0f;

		glEnable(GL_SCISSOR_TEST);

		// FIXME: OpenGL is cartesian, Vulkan is not
		glScissor(
			x - BleedEdge,
#if !VULKAN
			windowHeight -
#endif
			centeredY
#if !VULKAN
			- bounds.height / 2 - font->GetOutlineRadius() - bounds.renderedHeight / 2
#endif
			,
			maxWidth + BleedEdge * 2,
			bounds.height
		);
	}

	// Our shader _could_ be "texture" instead of "scrolling",
	// so we have a dummy uniform in fragment-texture to hold this
	context->GetShaderProgram().Uniform2f("origin"_hash, x, y);

	context->Blend(true, [this, x, y] {
		Text::OnLoop(x - offset, y);
	});

	if (widthDelta > 0) {
		glDisable(GL_SCISSOR_TEST);

		if (!pauseTimer) {
			offset += time.change.AsSeconds() * speed;

			if (offset > widthDelta)
				pauseTimer = std::chrono::system_clock::now();

		}
		else if (auto now = std::chrono::system_clock::now(); now - *pauseTimer > PauseTime) {
			pauseTimer = std::nullopt;

			if (offset > FLT_EPSILON) {
				offset = 0.0f;
				pauseTimer = std::chrono::system_clock::now();
			}
		}
	}
}

bool ScrollingText::SetText(const std::string &text, bool force, bool update) {
	if (Text::SetText(text, force) && update)
		UpdateWidthDelta();

	// Return value is currently ignored
	return true;
}

void ScrollingText::SetMaxWidth(float maxWidth) {
	this->maxWidth = maxWidth;

	UpdateWidthDelta();
}

// FIXME: This is a copy/paste from Text. 
//        Make it more DRY
void ScrollingText::SetFont(OpenGLFont *font) {
	if (this->font)
		this->font->RemoveSizeChangedListener(this);

	this->font = font;

	if (font)
		font->AddSizeChangedListener(this);

	SetText(text, true);
}

// In pixels-per-second
// Default: 20
void ScrollingText::SetSpeed(float speed) {
	this->speed = speed;
}