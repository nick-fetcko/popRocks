#include "Text.hpp"

#include "Buffer.hpp"

void Text::OnInit(OpenGLFont *font, Context *context) {
	this->font = font;
	this->context = context;

	// Refresh our text, if there is any
	SetText(text, true);
}

Vector2i Text::MeasureText(const std::string &text) const {
	Vector2i ret;
	if (font && font->HasFaces()) {
		auto bounds = font->MeasureText(text, 1.0f);
		ret.x = bounds.width;
		ret.y = bounds.height;
	}
	return ret;
}

void Text::SetText(const std::string &text, bool force) {
	if ((this->text == text && !force) || !font || !font->HasFaces()) return;

	this->text = text;

	// Size needs to be re-measured
	size = { 0, 0 };

	// Don't try to load an empty string
	//
	// We'll likely get a null surface anyway
	if (Empty()) return;

	std::tie(cached, bounds) = font->CacheText(
		text,
		glm::vec3(color.r, color.g, color.b),
		*context
	);

	if (!cached) {
		this->text.clear();
		return;
	}

	size = { bounds.width, bounds.renderedHeight + font->GetDescender() / 2 };
}

void Text::OnDestroy() {
	glDeleteTextures(1, &texture);
	texture = 0;
}

void Text::OnLoop(int x, int y) const {
	if (!font || Empty()) return;

	glBindTexture(GL_TEXTURE_2D, texture);

	context->Translate(x, y, 0.0f);
	context->Apply();

	font->RenderCached(cached, context->GetProjection(), *context);

	context->LoadIdentity();
}