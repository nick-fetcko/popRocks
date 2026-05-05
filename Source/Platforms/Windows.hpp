#ifdef WIN32

#pragma once

#include "Desktop.hpp"

#include <SDL3/SDL.h>

#include <atlstr.h>
#include <Windows.h>
#include <shlobj.h>
#include <Mmdeviceapi.h>

#include <basswasapi.h>

#include "OpenGL/Interops/DXGI.hpp"
#include "Source/RecordAudioStream.h"

using namespace Fetcko;

class Windows : public Desktop {
public:
	Windows(CApp *app);
	~Windows() override;

	// =====================================================
	// =================== Pure Virtuals ===================
	// =====================================================

	// CApp helpers
	void OnInit(Interop::InitArgs args, Context &context) override;
	void OnDestroy() override;
	void OnResize(int windowWidth, int windowHeight) override;
	std::optional<bool> OnLoop() override;
	void SwapBuffers() override;

	// Keyboard hooks
	void HookKeyboard() override;

	// OpenGL
	bool CreateOpenGlContext() override;
	void OpenOpenGlWindow(SDL_PropertiesID &props) override;

	// Interops
	void DestroyInterop() override;
	void CreateInterop() override;

	// Device listening
	void Listen(bool loopback) override;
	void StopListening() override;
	void LoadHeardSamples(Renderer *renderer, float *floatBuffer, short *shortBuffer, const std::size_t bufferLength) override;
	
	// Exclusive mode
	bool OpenExclusive(const std::filesystem::path &path, const std::string &extension, bool exclusive, HSTREAM &target, bool force, const BASS_CHANNELINFO &channelInfo, void *data) override;
	void StopExclusive(bool reset) override;
	
	// HDR
	std::optional<std::tuple<bool, float, float>> GetHdrProperties(int display, bool force = false) override;
	void SetHdr(bool enabled, void *hwnd, int width, int height) override;
	void UpdateHdrProperties(bool force = false) override;

	// File opening
	HSTREAM OpenWithFlags(const std::filesystem::path &path, const std::string &extension, DWORD flags) override;

	// BASS
	void LoadBassPlugins() override;

	// Fullscreen
	void ToggleFullscreen() override;

	// Miniplayer
	void SetMiniPlayer(bool miniPlayer, uint8_t chromaKey = 0) override;
	void SetChromaKey(bool enabled) override;
	bool SetTransparent(bool transparent) override;

	// Window management
	std::optional<Vector2i> SetWindowPos(int x, int y, int width, int height) override;

	// =====================================================
	// ===================== Virtuals ======================
	// =====================================================

	// CApp helpers
	bool OnMouseClicked(const Vector2i &mousePos) override;

	// Display properties
	int GetAdapterIndex() override;
	int GetDefaultFramebuffer() override;

	// Exclusive mode
	bool LoadExclusive(double pos) override;
	bool ScaleExclusive(Renderer *renderer, uint8_t *buffer, float *floatBuffer, short *shortBuffer) override;
	bool StartPlayingExclusive(bool fromPlaylist, bool fileLoaded, bool advanceOnNextLoop) override;
	bool StartExclusive() override;
	bool StopPlayingExclusive() override;

	// Filepaths
	std::filesystem::path GetNativePath(const std::filesystem::path &path) override;
	void GetPicturesPath(std::stringstream &filename) override;
	
	// Audio
	void Unmute() override;

protected:
	// Polymorphic helper for GetDeviceIndex<Output>
	bool GetDeviceIndex(int &index, const std::string &device) override;

private:
	static bool Register();
	static bool registered;

	// =====================================================
	// ================= Private Functions =================
	// =====================================================
	void UpdateKeyboardHookMode();

	DXGI dxgi;

	IMMDevice *audioDevice = nullptr;
	MyAudioSink *audioSink = nullptr;

	BASS_WASAPI_INFO wasapiInfo{ 0 };

	bool transparent = false;
	bool colorKeyEnabled = false;
};

// =====================================================
// ===================== Callbacks =====================
// =====================================================
LRESULT CALLBACK LowLevelKeyboardProc(
	_In_ int    nCode,
	_In_ WPARAM wParam,
	_In_ LPARAM lParam
);

// WASAPI input processing function
DWORD CALLBACK InWasapiProc(void *buffer, DWORD length, void *user);
DWORD CALLBACK OutputWasapiProc(void *buffer, DWORD length, void *user);

#endif