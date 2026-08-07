#pragma once

#include "MiniPlayerList.hpp"

class PresetList : public MiniPlayerList {
public:
	PresetList(AlbumArt *const albumArt, const bool &vulkan) : MiniPlayerList(Direction::Up, albumArt, vulkan) {
		albumArt->AddBlackChangedListener(this);
	}

	virtual ~PresetList() {
		albumArt->RemoveBlackChangedListener(this);
	}

	std::optional<std::size_t> OnMouseClicked(const Vector2i &mousePos) {
		const auto ret = (!miniPlayer || alpha == 0.0f || hoveredOffset < 0) ? 
			static_cast<std::optional<std::size_t>>(std::nullopt) :
			indices.at(hoveredOffset + scrollOffset);

		// Clicking without a hover target dismisses the PresetList
		if (hovered && hoveredOffset < 0 && alpha == 1.0f && alpha == targetAlpha) {
			hovered = false;
			hoverTimer = std::nullopt;
			targetAlpha = 0.0f;

			clickTimer = std::chrono::system_clock::now();
		}

		return ret;
	}
};