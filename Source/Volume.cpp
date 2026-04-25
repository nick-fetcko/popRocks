#include "Volume.hpp"

#include "AlbumArt.hpp"
#include "Hash.hpp"
#include "HDR.hpp"

Volume::Volume() {
	radius = AlbumArt::BaseRadius;
}

void Volume::OnInit(const std::string &fontRoot, Context *context) {
	ring.SetWidth(radius / 10);
	outlineRing.SetWidth(radius / 10 + (radius / 25) * 2);
	font = new OpenGLFont();
	font->OnInit(fontRoot, static_cast<int>(radius / 2));
	outlineFont = new OpenGLFont();
	outlineFont->OnInit(fontRoot, static_cast<int>(radius / 2), static_cast<int>(radius / 25));

	text.OnInit(font, context);
	outlineText.OnInit(outlineFont, context);
	outlineText.SetColor(Colour<float>::Black);
	UpdateVolume(false);
}

void Volume::OnLoop(int x, int y, const Delta &time, const AlbumArt *const albumArt, bool miniPlayer, Context &context) {
	AutoFader::OnLoop(time);

	//glTranslatef(x, y, 0);

	const auto OutlineColor = miniPlayer ? albumArt->GetBlackColor() : 0.0f;

	context.Color(OutlineColor, OutlineColor, OutlineColor, alpha);
	outlineText.OnLoop(x - text.GetSize().x / 2, y - text.GetSize().y / 2);
	context.Color(color.r, color.g, color.b, alpha);
	text.OnLoop(x - text.GetSize().x / 2, y - text.GetSize().y / 2);
	/*
	glColor4f(1.0f, 1.0f, 1.0f, alpha);

	y -= (text.GetSize().y + height) / 2.0f;
	text.OnLoop(x + (fullSize.x - text.GetSize().x) / 2.0f, y);

	glTranslatef(x + fullSize.x / 2.0f, y + text.GetSize().y, 0.0f);

	glVertexPointer(2, GL_FLOAT, 0, rect);

	glEnableClientState(GL_VERTEX_ARRAY);

	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, Buffer::SquareBuffer.data());
	glDisableClientState(GL_VERTEX_ARRAY);

	glLoadIdentity();
	*/
	context.Use("basic"_hash);

	context.Color(OutlineColor, OutlineColor, OutlineColor, alpha);
	context.Translate(
		static_cast<GLfloat>(x),
		static_cast<GLfloat>(y),
		0
	);
	context.Apply();
	outlineRing.Draw<true>(context);

	context.Color(color.r, color.g, color.b, alpha);
	context.Translate(
		static_cast<GLfloat>(x),
		static_cast<GLfloat>(y),
		0
	);
	context.Apply();
	ring.Draw<true>(context);

	context.Use("texture"_hash);
}

void Volume::SetRadius(float radius) {
	this->radius = radius;
	font->SetFontSize(static_cast<int>(radius / 2));
	outlineFont->SetFontSize(static_cast<int>(radius / 2));
	outlineFont->SetOutlineRadius(static_cast<int>(std::max(1.0f, radius / 25.0f)));
	ring.SetWidth(radius / 10);
	outlineRing.SetWidth(radius / 10 + (radius / 25) * 2);
	UpdateVolume(false, true);
}

void Volume::ToggleVolumeControl() { volumeControl = !volumeControl; }
const bool Volume::GetVolumeControl() const { return volumeControl; }

void Volume::VolumeUp() {
	Settings::settings.SetVolume(std::clamp(GetVolume() + 0.02f + FLT_EPSILON, 0.0f, 1.0f));
	UpdateVolume(true);
}
void Volume::VolumeDown() {
	Settings::settings.SetVolume(std::clamp(GetVolume() - 0.02f - FLT_EPSILON, 0.0f, 1.0f));
	UpdateVolume(true);
}

void Volume::SetVolume(int volume) {
	Settings::settings.SetVolume(volume / 100.0f);
	UpdateVolume(true);
}
const float &Volume::GetVolume() const { return Settings::settings.GetVolume(); }

// https://stackoverflow.com/a/1165188
const float Volume::GetScaledVolume() const {
	return scaledVolume;
}

const float Volume::GetInverseVolume() const {
	return inverseVolume;
}

void Volume::OnDestroy() {
	ring.OnDestroy();
	outlineRing.OnDestroy();

	text.OnDestroy();

	delete font;
	font = nullptr;
	delete outlineFont;
	font = nullptr;
}

void Volume::OnColorChanged(const Colour<float> &color, bool silent) {
	auto hsv = color.ToHsv();
	hsv.v = 1.0f;
	this->color = Colour<float>::FromHsv(hsv.h, hsv.s, hsv.v);

	if (HDR::Enabled) {
		this->color.Tone(
			Settings::settings.GetAlbumArtGamma(),
			Settings::settings.GetAlbumArtContrast(),
			Settings::settings.GetAlbumArtBrightness(),
			HDR::WhiteLevel * HDR::Headroom
		);
	}
}

inline void Volume::UpdateVolume(bool fade, bool force) {
	const auto string = std::to_string(static_cast<int>(GetVolume() * 100));

	scaledVolume = ((std::exp(GetVolume()) - 1.0f) / (std::exp(1.0f) - 1.0f));
	if (scaledVolume <= FLT_EPSILON)
		inverseVolume = 0.0f;
	else
		inverseVolume = 1.0f / scaledVolume;

	outlineText.SetText(string, force);
	text.SetText(string, force);

	/*
	rect[0] = -width / 2.0f;
	rect[1] = height - volume * height;
	rect[2] = -width / 2.0f;
	rect[3] = height;
	rect[4] = width / 2.0f;
	rect[5] = height;
	rect[6] = width / 2.0f;
	rect[7] = height - volume * height;
	*/

	std::size_t size = static_cast<std::size_t>((GetVolume() * 100) + 1);

	auto *points = new Vector2f[size];
	auto *outlinePoints = new Vector2f[size];
	float degInRad = 0.0f;
	const auto outlineOffset = (outlineRing.GetWidth() - ring.GetWidth()) / 2.0f;
	for (auto i = 0; i < size; ++i) {
		degInRad = (((i / 100.0f) * 360.0f) + 180.0f /* start at 12 o'clock */) * Maths::DEG2RAD<float>;

		// Negative X for clockwise movement
		points[i].x = -(sin(degInRad) * ((radius - ring.GetWidth() / 2.0f) - outlineOffset));
		points[i].y = cos(degInRad) * (radius - ring.GetWidth() / 2.0f - outlineOffset);

		outlinePoints[i].x = -(sin(degInRad) * (radius - outlineRing.GetWidth() / 2.0f));
		outlinePoints[i].y = cos(degInRad) * (radius - outlineRing.GetWidth() / 2.0f);
	}
	ring.SetPoints<Polyline::Join::Miter>(points, size);
	outlineRing.SetPoints<Polyline::Join::Miter>(outlinePoints, size);

	if (size == 101) {
		ring.Loop<Polyline::Join::Miter>();
		outlineRing.Loop<Polyline::Join::Miter>();
	}

	delete[] points;
	delete[] outlinePoints;

	if (fade) Fade(true);
}