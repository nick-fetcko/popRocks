#pragma once

#include "Next.hpp"

class Previous : public Next {
public:
	void OnLoop(float x, float y, const Delta &time, Context &context) {
		context.Use("basic"_hash);
		
		context.Scale(-1.0f, 1.0f, 1.0f);
		context.Translate(-x * 2, 0, 0);

		Next::OnLoop(x, y, time, context);
	}
};