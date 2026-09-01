#include "LineRenderer.hpp"

#include "AlbumArt.hpp"
#include "Settings.hpp"

LineRenderer::LineRenderer(
	const DynamicGain<float> *dynamicGain,
	const AlbumArt *albumArt
) : Renderer(dynamicGain, albumArt), line(Fetcko::Polyline(Settings::settings.GetWidth())), style(Settings::settings.GetLineRendererStyle()) {
}

LineRenderer::LineRenderer(Renderer &&other) : Renderer(std::move(other)), line(Fetcko::Polyline(Settings::settings.GetWidth())), style(Settings::settings.GetLineRendererStyle()) {

}

LineRenderer::LineRenderer(LineRenderer &&other) noexcept :
	Renderer(std::move(other)) {
	points = std::move(other.points);
	other.points = nullptr;
	line = std::move(other.line);
	style = std::move(other.style);
}

void LineRenderer::OnDestroy() {
	line.OnDestroy();
}

LineRenderer::~LineRenderer() {
	delete[] points;
}

void LineRenderer::SetBufferLength(std::size_t bufferLength, bool changed) {
	Renderer::SetBufferLength(bufferLength, changed);

	if (changed) {
		delete[] points;
		points = new Vector2f[bufferLength + 1];
	}
}

void LineRenderer::SetWidth(float width) {
	line.SetWidth(width);
	Settings::settings.SetWidth(width);
}

void LineRenderer::SetStyle(Style style) {
	this->style = style;
}

std::pair<float, float> LineRenderer::CenterPoints(bool miniPlayer) {
	std::pair<float, float> ret{ std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest() };

	// Center / scale points within our album art circle
	for (auto i = 0; i < bufferLength; ++i) {
		points[i].y = (((points[i].y - minPoint) / (maxPoint - minPoint)) * (albumArt->GetRadius(miniPlayer) * 2) + albumArt->GetRadius(miniPlayer) * -1) * scale;

		if (points[i].y < ret.first)
			ret.first = points[i].y;
		if (points[i].y > ret.second)
			ret.second = points[i].y;
	}

	newPoints = true;

	return ret;
}