#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>

#include <bass.h>
#include <bassape.h>
#include <bassflac.h>
#include <basswv.h>
#include <bass_tta.h>

#include "OpenGL/Context.hpp"
#include "OpenGL/Interops/Interop.hpp"

#include "Source/HDR.hpp"
#include "Source/Renderer.hpp"

using namespace Fetcko;

// Forward declarations
class CApp;
class Menu;

class Platform : public LoggableClass {
public:
	Platform(CApp *app) : app(app) {
	}

	virtual ~Platform() {

	}

	// =====================================================
	// =================== Pure Virtuals ===================
	// =====================================================

	// CApp helpers
	virtual void OnInit(Interop::InitArgs args, Context &context) = 0;
	virtual void OnDestroy() = 0;
	virtual void OnResize(int windowWidth, int windowHeight) = 0;
	virtual std::optional<bool> OnLoop() = 0;
	virtual void SwapBuffers() = 0;

	// Keyboard hooks
	virtual void HookKeyboard() = 0;

	// OpenGL
	virtual void SetGlAttributes() = 0;
	virtual bool CreateOpenGlContext() = 0;
	virtual void OpenOpenGlWindow(SDL_PropertiesID &props) = 0;
	virtual bool LoadGlad() = 0;

	// Interops
	virtual void DestroyInterop() = 0;
	virtual void CreateInterop() = 0;

	// Device listening
	virtual void Listen(bool loopback = false) = 0;
	virtual void StopListening() = 0;
	virtual void LoadHeardSamples(Renderer *renderer, float *floatBuffer, short *shortBuffer, const std::size_t bufferLength) = 0;
	
	// Exclusive mode
	virtual bool OpenExclusive(const std::filesystem::path &path, const std::string &extension, bool exclusive, HSTREAM &target, bool force, const BASS_CHANNELINFO &channelInfo, void *data) = 0;
	virtual void StopExclusive(bool reset) = 0;
	
	// HDR
	virtual std::optional<std::tuple<bool, float, float>> GetHdrProperties(int display, bool force = false) = 0;
	virtual void SetHdr(bool enabled, void *hwnd, int width, int height) = 0;
	virtual void UpdateHdrProperties(bool force = false) = 0;
	
	// File opening
	virtual HSTREAM OpenWithFlags(const std::filesystem::path &path, const std::string &extension, DWORD flags) = 0;
	virtual std::filesystem::path GetTemporaryFile(const std::string &pattern) = 0;

	// BASS
	virtual void LoadBassPlugins() = 0;

	// Fullscreen
	virtual void ToggleFullscreen() = 0;

	// Miniplayer
	virtual void SetMiniPlayer(bool miniPlayer, uint8_t chromaKey = 0) = 0;
	virtual bool SetTransparent(bool transparent) = 0;
	virtual void SetChromaKey(bool enabled) = 0;
	virtual void SetBgr(bool enabled, Context &context) = 0;

	// Window management
	virtual bool AllowsWindowMovement() const = 0;
	virtual std::optional<Vector2i> SetWindowPos(int x, int y, int width, int height) = 0;

	// =====================================================
	// ===================== Virtuals ======================
	// =====================================================

	// CApp helpers
	virtual bool OnMouseClicked(const Vector2i &mousePos);

	// Display properties
	virtual void SetSafeArea(SDL_Window *window, Context &context, int w, int h);
	virtual const float GetScale(float scale) const;
	virtual const int IsAlphaPremultiplied() const;
	virtual bool IsUiInverted();
	virtual GLenum GetFboInternalFormat(bool hdrEnabled);
	virtual int GetAdapterIndex();
	virtual bool IsTouchScreen();
	virtual int GetDefaultFramebuffer();

	// OpenGL
	virtual const bool GetOpenGlProperty() const;
	virtual const char *GetOpenGlContextError();

	// Vulkan
	virtual const bool GetVulkanProperty() const;

	// Exclusive mode
	virtual bool LoadExclusive(double pos);
	virtual bool ScaleExclusive(Renderer *renderer, uint8_t *buffer, float *floatBuffer, short *shortBuffer);
	virtual bool StartPlayingExclusive(bool fromPlaylist, bool fileLoaded, bool advanceOnNextLoop);
	virtual bool StartExclusive();
	virtual bool StopPlayingExclusive();

	// Filepaths
	virtual std::filesystem::path GetNativePath(const std::filesystem::path &path);
	virtual void GetPicturesPath(std::stringstream &filename);

	// Menus
	virtual void AddMenuCallbacks(Menu *menu);
	
	// FBO blitting
	virtual void BlitBlurFbo();

	// Audio
	virtual void Unmute();
	virtual bool PlayAfterLoad();

	// Mouse pointer
	virtual bool IsPointerInWindow() const;

	// Window management
	virtual bool IsMoving() const;
	virtual bool IsResizing() const;
	virtual void UpdateWindowShape();
	virtual void DestroyWindow(SDL_Window *window);
	virtual void HookWindow(bool miniPlayer);

	// =====================================================
	// ================ Getters / Setters ==================
	// =====================================================

	// Display properties
	const bool IsBgr() const;

	// Interops
	Interop *GetInterop();

	// Device listening
	const float &GetMaxHeardSample() const;
	const bool IsListening() const;
	void SetGain(float gain);

	// Exclusive mode
	const float &GetExclusiveBufferSize() const;
	
	// Max buffer length
	void SetMaxLength(std::size_t maxLength);
	const std::size_t &GetMaxLength() const;

	// =====================================================
	// =============== Template Functions ==================
	// ============= (Can't be polymorphic) ================
	// =====================================================
	template<bool Output>
	int GetDeviceIndex(const std::string &device) {
		int index = device.empty() ? -1 : 1;
		bool found = false;

		if constexpr (Output) {
			BASS_DEVICEINFO info;

			for (; index != -1 && BASS_GetDeviceInfo(index, &info); ++index) {
				if (strlen(info.driver) && strncmp(info.driver, device.c_str(), std::min(strlen(info.driver), device.size())) == 0) {
					found = true;
					break;
				}
			}
		} else {
			found = GetDeviceIndex(index, device);
		}

		// Reset to default device if we can't find
		// the selected device anymore
		if (!found) {
			index = -1;

			if constexpr (Output)
				Settings::settings.SetOutputDevice("");
			else
				Settings::settings.SetInputDevice("");
		}

		return index;
	}

protected:
	// Polymorphic helper for GetDeviceIndex<Output>
	virtual bool GetDeviceIndex(int &index, const std::string &device) = 0;

	CApp *app = nullptr;

	Interop *interop = nullptr;

	bool bgr = false;
	bool listening = false;
	float exclusiveBufferSize = 0.25f; // in seconds

	std::size_t maxLength = 0;
	float maxHeardSample = 0.0f;

	float gain = 20.0f;
};

// =====================================================
// ====================== Factory ======================
// =====================================================
class PlatformFactory {
public:
	static void Register(std::string &&platform, std::function<std::unique_ptr<Platform>(CApp *)> &&f);

	static std::unique_ptr<Platform> Build(const std::string &platform, CApp *app);

private:
	static inline std::map<
		std::string,
		std::function<
			std::unique_ptr<Platform>(CApp *)
		>
	> builders;
};