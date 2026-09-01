#pragma once

#include "Next.hpp"

class Previous : public Next {
public:
	Previous(AlbumArt *const albumArt) : Next(albumArt) {

	}

	void OnLoop(float x, float y, const Delta &time, Context &context, const float *alpha = nullptr) override {
		context.Use("basic"_hash);
		
		context.Scale(-1.0f, 1.0f, 1.0f);
		context.Translate(-x * 2, 0, 0);

		Next::OnLoop(x, y, time, context, alpha);
	}
};