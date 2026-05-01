#pragma once

#include "Platform.hpp"

#include <thread>

#include "fftw3.h"

class Desktop : public Platform {
public:
	Desktop(CApp *app);
	~Desktop() override;

	// =====================================================
	// =================== Pure Virtuals ===================
	// =====================================================

	// CApp helpers
	void OnInit(Interop::InitArgs args, Context &context) override;
	void OnResize(int windowWidth, int windowHeight) override;

	// OpenGL
	void SetGlAttributes() override;
	void OpenOpenGlWindow(SDL_PropertiesID &props) override;
	bool LoadGlad() override;

	// HDR
	void SetHdr(bool enabled, void *hwnd, int width, int height) override;

	// =====================================================
	// ===================== Virtuals ======================
	// =====================================================

	// OpenGL
	const char *GetOpenGlContextError() override;

	// FBO blitting
	void BlitBlurFbo() override;

	// Miniplayer
	void SetBgr(bool enabled, Context &context) override;

protected:
	// Helper for UpdateHdrProperties()
	void UpdateHdrProperties(int displayId, bool force = false);

	float *in = nullptr;
	fftwf_complex *out = nullptr;
	fftwf_plan plan = nullptr;

	std::thread listenThread;
};