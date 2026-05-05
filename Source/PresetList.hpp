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

	std::optional<std::size_t> OnMouseClicked(const Vector2i &mousePos) const {
		return (!miniPlayer || alpha == 0.0f || hoveredOffset == -1) ? static_cast<std::optional<std::size_t>>(std::nullopt) : indices.at(hoveredOffset + scrollOffset);
	}
};