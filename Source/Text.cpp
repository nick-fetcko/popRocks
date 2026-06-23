#include "Text.hpp"

#include "Buffer.hpp"

Text::Text(bool delayedCacheUpdate) : delayedCacheUpdate(delayedCacheUpdate) {

}

#ifdef _DEBUG
Text::~Text() {
	if (!destroyed)
		LogWarning("Text deallocated without destruction!");
}
#endif

void Text::SetFont(OpenGLFont *font) {
	if (this->font)
		this->font->RemoveSizeChangedListener(this);

	this->font = font;

	if (font)
		font->AddSizeChangedListener(this);

	// Refresh our text, if there is any
	SetText(text, true);
}

void Text::OnInit(OpenGLFont *font, Context *context) {
	this->context = context;

	SetFont(font);
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

bool Text::SetText(const std::string &text, bool force) {
	if ((this->text == text && !force) || !font || !font->HasFaces()) return false;

	this->text = text;

	// Size needs to be re-measured
	size = { 0, 0 };

	// Don't try to load an empty string
	//
	// We'll likely get a null surface anyway
	if (Empty()) return true;

	std::tie(cached, bounds) = font->CacheText(
		text,
		glm::vec3(color.r, color.g, color.b),
		*context
	);

	if (!cached) {
		this->text.clear();
		return true;
	}

	size = { bounds.width, bounds.renderedHeight };

	return true;
}

void Text::OnDestroy() {
	cached.reset();

	font->RemoveSizeChangedListener(this);

#ifdef _DEBUG
	destroyed = true;
#endif
}

void Text::OnLoop(int x, int y) const {
	if (!font || Empty()) return;

	// Center rendered text on the line
	y += (bounds.height - bounds.renderedHeight) / 2.0f;

	if (bounds.overhang)
		y -= std::floor((bounds.renderedHeight - bounds.overhang) / 2.0f) - std::ceil(bounds.overhang / 2.0f);
	else
		y -= bounds.renderedHeight / 2.0f;

	context->Translate(x, y, 0.0f);
	context->Apply();

	font->RenderCached(cached, context->GetProjection(), *context);

	context->LoadIdentity();
}

void Text::OnSizeChanged(FT_UInt size) {
	// Force a cache refresh
	if (!delayedCacheUpdate)
		SetText(text, true);
}

void Text::SetAltText(const std::string &altText) {
	this->altText = altText;
}