#ifdef WIN32

#pragma once

#include "Desktop.hpp"

#include <SDL3/SDL.h>

#include <atlstr.h>
#include <Windows.h>
#include <shlobj.h>
#include <Mmdeviceapi.h>
#include <dwmapi.h>

#include <basswasapi.h>

#include "OpenGL/Interops/DXGI.hpp"
#include "Source/RecordAudioStream.h"

using namespace Fetcko;

class Windows : public Desktop {
private:
	constexpr static std::string_view GUID = "51572059-45cd-41a5-bc30-7b16419a66d2";
	constexpr static std::wstring_view WindowClassName = L"popRocksWindowClass";

public:
	Windows(CApp *app);
	~Windows() override;

	WNDPROC GetSdlWndProc() { return sdlWndProc; }

	// =====================================================
	// =================== Pure Virtuals ===================
	// =====================================================

	// CApp helpers
	void OnInit(Interop::InitArgs args, Context &context) override;
	void OnDestroy() override;
	bool NeedsToResize(int &width, int &height, int windowWidth, int windowHeight, float scale, const std::optional<float> &scaleDelta, bool force) override;
	void OnResize(int windowWidth, int windowHeight) override;
	void HandleScaleDelta(float scale, std::optional<float> &scaleDelta, int &width, int &height, std::optional<Vector2i> &lastMousePos, int &windowX, int &windowY) override;
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
	bool OpenExclusive(const std::filesystem::path &path, const std::string &extension, bool exclusive, HSTREAM &target, HSTREAM &visualTarget, bool force, const BASS_CHANNELINFO &channelInfo, void *data) override;
	void StopExclusive(bool reset) override;
	
	// HDR
	std::optional<std::tuple<bool, float, float>> GetHdrProperties(int display, bool force = false) override;
	void SetHdr(bool enabled, void *hwnd, int width, int height) override;
	void UpdateHdrProperties(bool force = false) override;

	// File opening
	HSTREAM OpenWithFlags(const std::filesystem::path &path, const std::string &extension, DWORD flags) override;
	std::filesystem::path GetTemporaryFile(const std::string &pattern) override;

	// BASS
	void LoadBassPlugins() override;

	// Fullscreen
	void ToggleFullscreen() override;

	// Miniplayer
	void SetMiniPlayer(bool miniPlayer, uint8_t chromaKey = 0) override;
	void SetChromaKey(bool enabled) override;
	bool SetTransparent(bool transparent) override;

	// Window management
	bool AllowsWindowMovement() const override;
	std::optional<Vector2i> SetWindowPos(int x, int y, int width, int height, int *windowWidth = nullptr, int *windowHeight = nullptr, bool alreadyRespawned = false) override;
	void ShowDialogBox(const std::string &title, const std::string &message) override;
	bool HandleExistingWindow(int argc, char *argv[]) override;

	// =====================================================
	// ===================== Virtuals ======================
	// =====================================================

	// CApp helpers
	bool OnMouseClicked(const Vector2i &mousePos) override;

	// Display properties
	int GetAdapterIndex() override;
	int GetDefaultFramebuffer() override;
	const float GetScale(SDL_Window *window, Context &context, int *w, int *h) override;
	const float GetScale(bool actual = false) const override;
	const float GetScaleForPoint(int x, int y) const override;

	// Exclusive mode
	bool LoadExclusive(double pos) override;
	bool StartPlayingExclusive(bool fromPlaylist, bool fileLoaded, bool advanceOnNextLoop) override;
	bool StartExclusive() override;
	bool StopPlayingExclusive() override;
	std::size_t GetAvailable() const override;

	// Filepaths
	std::filesystem::path GetNativePath(const std::filesystem::path &path) override;
	void GetPicturesPath(std::stringstream &filename) override;
	
	// Audio
	void Unmute() override;
	std::map<std::string, OutputDevice> GetOutputDevices() override;

	// Window Management
	void HookWindow(bool miniPlayer) override;

	// Bling
	void SetStatus(Status status, int progress) override;

protected:
	// Polymorphic helper for GetDeviceIndex<Output>
	bool GetInputDeviceIndex(int &index, const std::string &device) override;

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
	QWORD pausePos = 0;

	bool transparent = false;
	bool colorKeyEnabled = false;

	ITaskbarList3 *taskbar = nullptr;

	Status lastStatus = Status::Stopped;
	int lastProgress = 0;

	WNDPROC sdlWndProc = nullptr;

	float actualScale = 1.0f;

#ifndef _DEBUG
	HKEY registryKey = nullptr;
	HANDLE mutex = nullptr;
#endif
};

// =====================================================
// ===================== Callbacks =====================
// =====================================================
LRESULT CALLBACK LowLevelKeyboardProc(
	_In_ int    nCode,
	_In_ WPARAM wParam,
	_In_ LPARAM lParam
);

LRESULT CALLBACK MessageBoxCbtHookProc(int nCode, WPARAM wParam, LPARAM lParam);

// WASAPI input processing function
DWORD CALLBACK InWasapiProc(void *buffer, DWORD length, void *user);
DWORD CALLBACK OutputWasapiProc(void *buffer, DWORD length, void *user);

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

#endif