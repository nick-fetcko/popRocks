#include "OscilloscopeRenderer.hpp"

#include <bitset>

#include "AlbumArt.hpp"

OscilloscopeRenderer::OscilloscopeRenderer(
	const DynamicGain<float> *dynamicGain,
	const AlbumArt *albumArt
) : LineRenderer(dynamicGain, albumArt) {
}

OscilloscopeRenderer::OscilloscopeRenderer(Renderer &&right) : LineRenderer(std::move(right)) {
	SetBuffer(buffer, fullBufferLength);
	SetBufferLength(bufferLength, true);
	SetNumberOfChannels(numberOfChannels);
}

OscilloscopeRenderer::OscilloscopeRenderer(LineRenderer &&right) : LineRenderer(std::move(right)) {
	SetBuffer(buffer, fullBufferLength);
	SetNumberOfChannels(numberOfChannels);
}

OscilloscopeRenderer::~OscilloscopeRenderer() {
	delete[] channelLines;
}

bool OscilloscopeRenderer::IsFloatingPoint() const { return false; }

bool OscilloscopeRenderer::SetBuffer(const uint8_t *const buffer, std::size_t len, bool force) {
	auto changed = Renderer::SetBuffer(buffer, len, force);
	shortBuffer = reinterpret_cast<const short *>(buffer);

	if (changed) {
		minPoint = dynamicGain->minReset;
		maxPoint = dynamicGain->maxReset;
	}

	return changed;
}

void OscilloscopeRenderer::SetNumberOfChannels(uint8_t numberOfChannels) {
	if ((!channelLines || numberOfChannels != this->numberOfChannels) && numberOfChannels > 1) {
		if (channelLines) delete[] channelLines;

		channelLines = new Fetcko::Polyline[numberOfChannels - 1];
		for (uint8_t channel = 1; channel < numberOfChannels; ++channel)
			channelLines[channel - 1].SetWidth(line.GetWidth());
	

	} else if (channelLines && numberOfChannels <= 1) {
		delete[] channelLines;
		channelLines = nullptr;
	}

	Renderer::SetNumberOfChannels(numberOfChannels);
}

void OscilloscopeRenderer::SetWidth(float width) {
	LineRenderer::SetWidth(width);
	for (uint8_t channel = 1; channel < numberOfChannels; ++channel)
		channelLines[channel - 1].SetWidth(line.GetWidth());
}

void OscilloscopeRenderer::OnLoop(
	const Delta &time,
	bool fileLoaded,
	float hStep,
	Context &context,
	const Colour<float> &color,
	const Colour<float> &brightColor,
	float frameCount,
	float maxHeardSample,
	bool resetGain,
	bool miniPlayer
) {
	if (style == Style::Line) {
		minPoint = std::numeric_limits<float>::max();
		maxPoint = std::numeric_limits<float>::lowest();

		for (int i = 0; i < bufferLength; i++) {
			for (auto channel = 0; channel < numberOfChannels; ++channel) {
				auto &point = points[channel * (bufferLength / numberOfChannels) + (i / numberOfChannels)];

				point.x = hStep * channel * (bufferLength / numberOfChannels) + i * (hStep / numberOfChannels);
				point.y = shortBuffer[i * numberOfChannels + channel];

				if (point.y > maxPoint)
					maxPoint = point.y;
				if (point.y < minPoint)
					minPoint = point.y;
			}
		}

		CenterPoints(miniPlayer);
	} else {
		const auto radius = albumArt->GetRadius(miniPlayer) + line.GetWidth() * 2;
		const auto height = GetHeight(line.GetWidth() * 2, miniPlayer);

		for (int i = 0; i < bufferLength; i++) {
			for (auto channel = 0; channel < numberOfChannels; ++channel) {
				const auto index = channel * (bufferLength / numberOfChannels) + (i / numberOfChannels);
				const auto deg2rad = (index / static_cast<float>(bufferLength)) * 360.0f * Maths::DEG2RAD<float>;
				auto &point = points[index];
				const auto rawValue = shortBuffer[i * numberOfChannels + channel];

				if (resetGain) {
					minPoint = dynamicGain->minReset;
					maxPoint = dynamicGain->maxReset;
				}

				if (rawValue > maxPoint)
					maxPoint = rawValue;
				if (rawValue < minPoint)
					minPoint = rawValue;

				// If we're listening, skip normalization
				const auto diff = maxPoint - minPoint;
				const auto scaledValue =
					//fileLoaded ?
					diff > 0.0f ?
						std::clamp(((rawValue - minPoint) / diff) * height, 0.0f, height) : //:
						rawValue;
					//rawValue * height;
					;

				const auto sample = scaledValue + radius;

				point.x = sample * sin(deg2rad);
				point.y = sample * cos(deg2rad);
			}
		}

		newPoints = true;
	}
}

void OscilloscopeRenderer::Draw(
	const Delta &time,
	float frameCount,
	const Colour<float> &color,
	const Vector<int, 2> &blurOffset,
	Context &context
) {
	context.Use("basic"_hash);

	SetColor(color, 1.0f, context);

	if (style == Style::Line) {
		context.Translate(-blurOffset.x / 2.0f - (maxDimension - windowWidth) / 2.0f, -blurOffset.y / 2.0f + windowHeight / 2.0f, 0);

		context.Translate(maxDimension / 2.0f, 0, 0);
		context.Rotate(frameCount, 0.0f, 0.0f, 1.0f);
		context.Translate(-maxDimension / 2.0f, 0, 0);

		context.Apply();

		if (newPoints) {
			line.SetPoints<Polyline::Join::None>(points, bufferLength);
			newPoints = false;
		}

		line.Draw<true>(context);
	} else {
		context.Translate(
			-blurOffset.x / 2.0f + windowWidth / 2.0f,
			-blurOffset.y / 2.0f + windowHeight / 2.0f,
			0
		);

		context.Rotate(frameCount, 0.0f, 0.0f, 1.0f);

		context.Apply();
		if (newPoints) {
			line.SetPoints<Polyline::Join::None>(points, bufferLength / numberOfChannels);
			for (auto channel = 1; channel < numberOfChannels; ++channel)
				channelLines[channel - 1].SetPoints<Polyline::Join::None>(&points[(bufferLength / numberOfChannels) * channel], bufferLength / numberOfChannels);
			newPoints = false;
		}

		for (auto channel = 1; channel < numberOfChannels; ++channel)
			channelLines[channel - 1].Draw<false>(context);

		line.Draw<true>(context);
	}

	context.Use("texture"_hash);
}

void OscilloscopeRenderer::Reset() {
	minPoint = dynamicGain->minReset;
	maxPoint = dynamicGain->maxReset;
}