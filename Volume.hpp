#pragma once

#include <glad/glad.h>

#include "MathCPP/Colour.hpp"

#include "AutoFader.hpp"
#include "ColorChangeListener.hpp"
#include "Polyline.hpp"
#include "Settings.hpp"
#include "Text.hpp"

using namespace MathsCPP;

class Volume : public ColorChangeListener, public AutoFader {
public:
	Volume() = default;

	virtual ~Volume() = default;

	void OnInit(const std::filesystem::path &fontFile);

	void OnLoop(int x, int y, const Delta &time);

	void SetRadius(float radius);

	void ToggleVolumeControl();
	const bool GetVolumeControl() const;

	void VolumeUp();
	void VolumeDown();
	const float &GetVolume() const;

	const float GetScaledVolume() const;

	const float GetInverseVolume() const;

	void OnDestroy();

	void OnColorChanged(const Colour<float> &color, bool silent = false) override;

private:
	inline void UpdateVolume(bool force = false);

	float radius = 200.0f;

	TTF_Font *font = nullptr, *outlineFont = nullptr;

	bool volumeControl = true;

	Text text;
	Text outlineText;

	//float rect[8] = { 0 };
	Polyline outlineRing;
	Polyline ring;

	Colour<float> color = Colour<float>::White;
};