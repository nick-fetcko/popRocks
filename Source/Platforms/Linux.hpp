#ifdef __linux__

#pragma once

#include "Desktop.hpp"

#include "fftw3.h"

#include <bassalac.h>
#include <bass_aac.h>

class Linux : public Desktop {
public:
	Linux(CApp *app);
	~Linux() override;

	// =====================================================
	// =================== Pure Virtuals ===================
	// =====================================================

	// CApp helpers
	void OnDestroy() override;
	std::optional<bool> OnLoop() override;
	void SwapBuffers() override;

	// Keyboard hooks
	void HookKeyboard() override;

	// OpenGL
	bool CreateOpenGlContext() override;

	// Device listening
	void Listen(bool loopback) override;
	void StopListening() override;
	void LoadHeardSamples(Renderer *renderer, float *floatBuffer, short *shortBuffer, const std::size_t bufferLength) override;
	
	// Exclusive mode
	bool OpenExclusive(const std::filesystem::path &path, const std::string &extension, bool exclusive, HSTREAM &target, bool force, const BASS_CHANNELINFO &channelInfo, void *data) override;
	void StopExclusive(bool reset) override;
	
	// HDR
	std::optional<std::tuple<bool, float, float>> GetHdrProperties(int display) override;
	void UpdateHdrProperties() override;

	// File opening
	HSTREAM OpenWithFlags(const std::filesystem::path &path, const std::string &extension, DWORD flags) override;

	// BASS
	void LoadBassPlugins() override;

	// Fullscreen
	void ToggleFullscreen() override;

	// =====================================================
	// ===================== Virtuals ======================
	// =====================================================

	// CApp helpers
	bool OnMouseClicked(const Vector2i &mousePos) override;

	// Display properties
	int GetDefaultFramebuffer() override;

protected:
	// Polymorphic helper for GetDeviceIndex<Output>
	bool GetDeviceIndex(int &index, const std::string &device) override { return false; }

private:
    static bool Register();
	static bool registered;

	std::thread listenThread;

	float *in = nullptr;
	fftwf_complex *out = nullptr;
	fftwf_plan plan = nullptr;
};

#endif