#pragma once

#include "Linux.hpp"

class Flatpak : public Linux {
public:
	Flatpak(CApp *app);
	~Flatpak() override;

	// =====================================================
	// =================== Pure Virtuals ===================
	// =====================================================

	// BASS
	void LoadBassPlugins() override;

protected:

private:
	static bool Register();
	static bool registered;
};