#include "Volume.hpp"

void Volume::OnInit(const std::filesystem::path &fontFile) {
	auto string = fontFile.u8string();

	ring.SetWidth(radius / 10);
	outlineRing.SetWidth(radius / 10 + (radius / 25) * 2);
	font = TTF_OpenFont(string.c_str(), static_cast<int>(radius / 2));
	outlineFont = TTF_OpenFont(string.c_str(), static_cast<int>(radius / 2));

	TTF_SetFontOutline(outlineFont, static_cast<int>(radius / 25));
	text.OnInit(font);
	outlineText.OnInit(outlineFont);
	outlineText.SetColor(Colour<float>::Black);
	UpdateVolume();
}

void Volume::OnLoop(int x, int y, const Delta &time) {
	AutoFader::OnLoop(time);

	glColor4f(color.r, color.g, color.b, alpha);
	//glTranslatef(x, y, 0);

	outlineText.OnLoop(x - outlineText.GetSize().x / 2, y - outlineText.GetSize().y / 2);
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

	glColor4f(0.0f, 0.0f, 0.0f, alpha);
	glTranslatef(
		static_cast<GLfloat>(x),
		static_cast<GLfloat>(y),
		0
	);
	outlineRing.Draw();

	glColor4f(color.r, color.g, color.b, alpha);
	glTranslatef(
		static_cast<GLfloat>(x),
		static_cast<GLfloat>(y),
		0
	);
	ring.Draw();
}

void Volume::SetRadius(float radius) {
	this->radius = radius;
	TTF_SetFontSize(font, static_cast<int>(radius / 2));
	TTF_SetFontSize(outlineFont, static_cast<int>(radius / 2));
	TTF_SetFontOutline(outlineFont, static_cast<int>(std::max(1.0f, radius / 25.0f)));
	ring.SetWidth(radius / 10);
	outlineRing.SetWidth(radius / 10 + (radius / 25) * 2);
	UpdateVolume(true);
}

void Volume::ToggleVolumeControl() { volumeControl = !volumeControl; }
const bool Volume::GetVolumeControl() const { return volumeControl; }

void Volume::VolumeUp() {
	Settings::settings.SetVolume(std::clamp(GetVolume() + 0.02f + FLT_EPSILON, 0.0f, 1.0f));
	UpdateVolume();
}
void Volume::VolumeDown() {
	Settings::settings.SetVolume(std::clamp(GetVolume() - 0.02f - FLT_EPSILON, 0.0f, 1.0f));
	UpdateVolume();
}
const float &Volume::GetVolume() const { return Settings::settings.GetVolume(); }

// https://stackoverflow.com/a/1165188
const float Volume::GetScaledVolume() const {
	return ((std::exp(GetVolume()) - 1.0f) / (std::exp(1.0f) - 1.0f));
}

const float Volume::GetInverseVolume() const {
	const auto scaled = GetScaledVolume();

	if (scaled <= FLT_EPSILON)
		return 0.0f;

	return 1.0f / scaled;
}

void Volume::OnDestroy() {
	text.OnDestroy();
	TTF_CloseFont(font);
}

void Volume::OnColorChanged(const Colour<float> &color, bool silent) {
	auto hsv = color.ToHsv();
	hsv.v = 1.0f;
	this->color = Colour<float>::FromHsv(hsv.h, hsv.s, hsv.v);
}

inline void Volume::UpdateVolume(bool force) {
	const auto string = std::to_string(static_cast<int>(GetVolume() * 100));

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

	// Update setting here

	Fade(true);
}