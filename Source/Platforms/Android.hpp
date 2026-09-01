#ifdef __ANDROID__

#pragma once

#include "Platform.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <SDL3/SDL_system.h>

#include <bassalac.h>
#include <bass_aac.h>

#include "Source/Menu.hpp"

class Android : public Platform {
public:
	Android(CApp *app);
	~Android() override;

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
	void SetGlAttributes() override;
	bool CreateOpenGlContext() override;
	void OpenOpenGlWindow(SDL_PropertiesID &props) override;

	bool LoadGlad() override;

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

	// =====================================================
	// ===================== Virtuals ======================
	// =====================================================

	// CApp helpers

	// Display properties
	void SetSafeArea(SDL_Window *window, Context &context, int w, int h) override;
	const float GetScale(float scale) const override;
	bool IsUiInverted() override;
	GLenum GetFboInternalFormat(bool hdrEnabled) override;
	bool IsTouchScreen() override;

	// OpenGL
	const bool GetOpenGlProperty() const override;

	// Vulkan
	const bool GetVulkanProperty() const override;

	// Menus
	void AddMenuCallbacks(Menu *menu) override;

	// Audio
	bool PlayAfterLoad() override;

	// =====================================================
	// ================== Android helpers ==================
	// =====================================================
	void DestroyEGLWindowSurface();
	void RecreateEGLWindowSurface();
	void LoadFileNextLoop(std::filesystem::path path, bool andPlay = true);
	void SetLibraryPath(const std::string &path) { libraryPath = path; }

protected:
	// Polymorphic helper for GetDeviceIndex<Output>
	bool GetDeviceIndex(int &index, const std::string &device) override { return false; }

private:
	static bool Register();
	static bool registered;

	// =====================================================
	// ================== Android helpers ==================
	// =====================================================
	void InitEGL();
	void CreateEGLWindowSurface();

	EGLDisplay display;
	EGLConfig config;
	EGLSurface eglSurface;
	EGLContext eglContext = nullptr;
	bool destroyNextFrame = false;
	bool createNextFrame = false;
	bool paused = false;
	std::optional<std::filesystem::path> loadNextLoop = std::nullopt;
	bool playNextLoop = false;
	std::filesystem::path libraryPath;
};

#endif