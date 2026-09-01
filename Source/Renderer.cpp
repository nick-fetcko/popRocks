#include "Renderer.hpp"

#include "AlbumArt.hpp"
#include "Settings.hpp"

Renderer::Renderer(
	const DynamicGain<float> *dynamicGain,
	const AlbumArt *const albumArt
) : albumArt(albumArt), dynamicGain(dynamicGain), offset(Settings::settings.GetRendererOffset()) {
	pulse = Settings::settings.GetPulse();
	pulseTime = Settings::settings.GetPulseTime();
	scale = Settings::settings.GetScale();
}

Renderer::Renderer(Renderer &&other) noexcept : albumArt(other.albumArt) {
	initialized = other.initialized;
	windowWidth = other.windowWidth;
	windowHeight = other.windowHeight;
	maxDimension = other.maxDimension;
	fullBufferLength = other.fullBufferLength;
	bufferLength = other.bufferLength;
	dynamicGain = other.dynamicGain;
	buffer = other.buffer;
	numberOfChannels = other.numberOfChannels;
	pulse = other.pulse;
	// pulses intentionally left blank
	pulseTime = other.pulseTime;
	scale = other.scale;
	offset = other.offset;
}

Renderer::~Renderer() {

}

void Renderer::OnInit(
	int windowWidth,
	int windowHeight
) {
	this->windowWidth = windowWidth;
	this->windowHeight = windowHeight;

	initialized = true;
}

void Renderer::OnResize(
	int windowWidth,
	int windowHeight,
	int maxDimension
) {
	this->windowWidth = windowWidth;
	this->windowHeight = windowHeight;
	this->maxDimension = maxDimension;
}

void Renderer::SetBufferLength(std::size_t bufferLength, bool changed) {
	this->bufferLength = bufferLength;
}

bool Renderer::SetBuffer(const uint8_t *const buffer, std::size_t len, bool force) {
	this->buffer = buffer;
	if (len != fullBufferLength) {
		fullBufferLength = len;
		return true;
	}
	return force;
}

void Renderer::SetNumberOfChannels(uint8_t numberOfChannels) {
	this->numberOfChannels = numberOfChannels;
}

const bool Renderer::GetPulses() const { return pulses; }
const bool Renderer::GetPulse() const { return pulse; }

void Renderer::SetScale(float scale) {
	this->scale = scale;
}

void Renderer::SetOffset(int offset) {
	this->offset = offset;
}

void Renderer::TogglePulse() {
	pulse = !pulse;
	Settings::settings.SetPulse(pulse);
}
void Renderer::SetPulse(bool pulse) {
	this->pulse = pulse;
	Settings::settings.SetPulse(pulse);
}
void Renderer::SetPulseTime(Duration<Microseconds> time) {
	pulseTime = time;
	Settings::settings.SetPulseTime(time);
}

void Renderer::SetColor(const Colour<float> &color, float alpha, Context &context) {
	context.Color(color.r, color.g, color.b, alpha);
}

float Renderer::GetThickness(bool miniPlayer) const {
	return std::ceil(std::max((albumArt->GetRadius(miniPlayer) * Maths::PI<float>) / bufferLength, 1.0f));
}

inline float Renderer::GetEffectOffset() const {
	return static_cast<float>(
		std::max(
			Settings::settings.GetEffectRadiation() * 2.0f,
			0.0f
		) +
		std::max(
			Settings::settings.GetEffectIntensity() * 2.0f /* * (Settings::settings.GetBlurIntensity() / time.change.AsSeconds()) */,
			0.0f
		)
	);
}

float Renderer::GetHeight(float margin, bool miniPlayer) const {
	const auto radius = albumArt->GetRadius(miniPlayer);

	return std::max(miniPlayer ?
		(std::min(windowWidth, windowHeight) / 2.0f - radius) * scale - GetThickness(miniPlayer) * 2 - GetEffectOffset() - margin : // Min
		(std::max(windowWidth, windowHeight) / 2.0f - radius) * scale, 0.0f); // Max
}