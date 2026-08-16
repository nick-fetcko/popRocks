#pragma once

#include <glad/glad.h>

#include "MathCPP/Colour.hpp"
#include "OpenGL/Context.hpp"
#include "OpenGL/Polyline.hpp"

#include "AutoFader.hpp"
#include "ColorChangeListener.hpp"
#include "HDR.hpp"
#include "Settings.hpp"
#include "Text.hpp"

using namespace MathsCPP;
using namespace Fetcko;

class AlbumArt;
class Volume : public ColorChangeListener, public AutoFader<false> {
public:
	Volume();

	virtual ~Volume() = default;

	void OnInit(const std::string &fontRoot, OpenGLFont *font, OpenGLFont *outlineFont, Context *context);

	void OnLoop(int x, int y, const Delta &time, const AlbumArt *const albumArt, bool miniPlayer, Context &context);

	void SetRadius(float radius);

	void ToggleVolumeControl();
	const bool GetVolumeControl() const;

	void VolumeUp();
	void VolumeDown();

	void SetVolume(int volume);
	const float &GetVolume() const;

	const float GetScaledVolume() const;

	const float GetInverseVolume() const;

	void OnDestroy();

	void OnColorChanged(const Colour<float> &color, bool silent = false) override;

private:
	inline void UpdateVolume(bool fade = true, bool force = false);

	float radius = 200.0f;

	OpenGLFont *font = nullptr, *outlineFont = nullptr;

	bool volumeControl = true;

	Text text;
	Text outlineText;

	Text labelOutline;
	Text label;

	//float rect[8] = { 0 };
	Fetcko::Polyline outlineRing;
	Fetcko::Polyline ring;

	Colour<float> color = HDR::WhiteColor;

	float scaledVolume = 0.0f;
	float inverseVolume = 0.0f;
};