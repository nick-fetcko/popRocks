#ifdef __linux__

#pragma once

#include <any>

#include "Desktop.hpp"
#include "../Integrations/MPRIS.hpp"

#include "fftw3.h"

#include <bassalac.h>
#include <bass_aac.h>

#include <wayland-client.h>
#include <wayland-client-protocol.h>

#include "xdg-shell-client-protocol.h"

#include <dbus/dbus.h>

#include <pipewire-0.3/pipewire/pipewire.h>
#include <pipewire-0.3/pipewire/device.h>
#include <pulse/pulseaudio.h>
#include <spa-0.2/spa/param/audio/format-utils.h>

class Linux : public Desktop {
public:
	Linux(CApp *app);
	~Linux() override;

	CApp *GetApp() { return app; }

	// =====================================================
	// =================== Pure Virtuals ===================
	// =====================================================

	// CApp helpers
	void OnInit(Interop::InitArgs args, Context &context) override;
	void OnResize(int windowWidth, int windowHeight) override;
	void HandleScaleDelta(float scale, std::optional<float> &scaleDelta, int &width, int &height, std::optional<Vector2i> &lastMousePos, int &windowX, int &windowY) override;
	void OnDestroy() override;
	std::optional<bool> OnLoop() override;
	void SwapBuffers() override;

	// Keyboard hooks
	void HookKeyboard() override;

	// OpenGL
	void OpenOpenGlWindow(SDL_PropertiesID &props) override;
	bool CreateOpenGlContext() override;

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
	bool SetTransparent(bool transparent) override;
	void SetChromaKey(bool enabled) override;

	// Window management
	bool AllowsWindowMovement() const override;
	std::optional<Vector2i> SetWindowPos(int x, int y, int width, int height) override;

	// =====================================================
	// ===================== Virtuals ======================
	// =====================================================

	// CApp helpers
	bool OnMouseClicked(const Vector2i &mousePos) override;
	bool OnMouseDown(const Vector2i &mousePos) override;

	// Display properties
	int GetDefaultFramebuffer() override;
	
	// Exclusive mode
	bool LoadExclusive(double pos) override;
	bool StopPlayingExclusive() override;
	bool StartPlayingExclusive(bool fromPlaylist, bool fileLoaded, bool advanceOnNextLoop) override;
	std::size_t GetAvailable() const override;

	// Audio
	std::map<std::string, OutputDevice> GetOutputDevices() override;

	// Mouse pointer
	bool IsPointerInWindow() const override;

	// Window management
	bool IsMoving() const override;
	bool IsResizing() const override;
	void UpdateWindowShape() override;
	void DestroyWindow(SDL_Window *window) override;
	void HookWindow(bool miniPlayer) override;
	void ShowDialogBox(const std::string &title, const std::string &message) override;
	bool HandleExistingWindow() override;

	// Bling
	void SetStatus(Status status, int progress) override;

	// =====================================================
	// ===================== Wayland =======================
	// =====================================================
	static void XdgSurfaceConfigure(void *data, struct xdg_surface *xdg_surface, uint32_t serial);
	static void XdgWmBasePing(void *data, struct xdg_wm_base *xdg_dm_base, uint32_t serial);
	static void XdgTopLevelConfigure(void *data, struct xdg_toplevel *toplevel, int32_t width, int32_t height, struct wl_array *states);
	static void XdgTopLevelClose(void *data, struct xdg_toplevel *topLevel);
	
	static void WaylandRegistryGlobal(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
	static void WaylandRegistryGlobalRemove(void *data, struct wl_registry *registry, uint32_t name);

	static void SeatCapabilities(void *data, struct wl_seat *seat, uint32_t caps);
	static void SeatName(void *data, struct wl_seat *seat, const char *name);

	static void PointerEnter(void *data, wl_pointer *pointer, uint serial, wl_surface *surface, wl_fixed_t surface_x, wl_fixed_t surface_y);
	static void PointerLeave(void *data, wl_pointer *pointer, uint serial, wl_surface *surface);
	static void PointerMotion(void *data, wl_pointer *pointer, uint time, wl_fixed_t surface_x, wl_fixed_t surface_y);
	static void PointerButton(void *data, wl_pointer *pointer, uint serial, uint time, uint button, uint state);
	static void PointerAxis(void *data, wl_pointer *pointer, uint time, uint axis, wl_fixed_t value);
	static void PointerFrame(void *data, wl_pointer *pointer);
	static void PointerAxisSource(void *data, wl_pointer *pointer, uint axis_source);
	static void PointerAxisStop(void *data, wl_pointer *pointer, uint time, uint axis);
	static void PointerAxisDiscrete(void *data, wl_pointer *pointer, uint axis, int discrete);
	static void PointerAxisRelativeDirection(void *data, wl_pointer *pointer, uint32_t axis, uint32_t direction);
	static void PointerAxisValue120(void *data, wl_pointer *pointer, uint32_t axis, int32_t value120);

	void SetConfigured(bool configured) { this->configured = configured; }
	const bool &GetConfigured() const { return configured; }

	void SetConfigureSerial(uint32_t configureSerial) { this->configureSerial = configureSerial; }

	void SetPointerX(int32_t pointerX) { this->pointerX = pointerX; }
	const int32_t GetPointerX() const { return pointerX; }
	void SetPointerY(int32_t pointerY) { this->pointerY = pointerY; }
	const int32_t GetPointerY() const { return pointerY; }

	void SetInWindow(bool inWindow) { this->inWindow = inWindow; }

	void SetCompositor(struct wl_compositor *compositor) { this->compositor = compositor; }
	void SetSeat(struct wl_seat *seat) { this->seat = seat; }
	void SetPointer(struct wl_pointer *pointer) { this->pointer = pointer; }
	void SetXdgWmBase(struct xdg_wm_base *xdgWmBase) { this->xdgWmBase = xdgWmBase; }

	struct wl_surface *GetSurface() { return surface; }
	struct wl_seat *GetSeat() { return seat; }
	struct xdg_wm_base *GetXdgWmBase() { return xdgWmBase; }
	struct xdg_toplevel *GetXdgTopLevel() { return xdgTopLevel; }

	struct wl_seat_listener &GetSeatListener() { return seatListener; }
	struct wl_pointer_listener &GetPointerListener() { return pointerListener; }
	struct xdg_wm_base_listener &GetXdgWmBaseListener() { return xdgWmBaseListener; }

	void SetMoving(bool moving) { this->moving = moving; }
	void SetResizing(bool resizing) { this->resizing = resizing; }

	// =====================================================
	// ==================== PipeWire =======================
	// =====================================================
	struct PipeWireData {
		struct pw_thread_loop *loop;
		struct pw_stream *stream;
	};

	const PipeWireData &GetPipeWireData() { return pwData; }

	std::string &GetDefaultSinkName() { return defaultSinkName; }

	void SetStreamState(pw_stream_state state) { streamState = state; }

	const BASS_CHANNELINFO &GetExclusiveChannelInfo() const { return exclusiveChannelInfo; }

	void SetLastQueueTime(int64_t lastQueueTime) { this->lastQueueTime = lastQueueTime; }
	void SetQueueSize(double queueSize) { this->queueSize = queueSize; }

	static void PulseAudioContextStateCallback(pa_context *c, void *data);
	static void PulseAudioGetServerInfoCallback(pa_context *c, const pa_server_info *i, void *data);
	static void PulseAudioSinkListCallback(pa_context *c, const pa_sink_info *i, int eol, void *data);

	static void PipeWireProcess(void *data);
	static void PipeWireStateChanged(void *data, pw_stream_state old, pw_stream_state state, const char *error);

	void AddOutputDevice(const std::string &description, const std::string &name, const std::string &driver);
	
protected:
	// Polymorphic helpers for GetDeviceIndex<Output>
	bool GetOutputDeviceIndex(int &index, const std::string &device) override;
	bool GetInputDeviceIndex(int &index, const std::string &device) override { return false; }

private:
    static bool Register();
	static bool registered;

	std::thread listenThread;

	float *in = nullptr;
	fftwf_complex *out = nullptr;
	fftwf_plan plan = nullptr;

	bool miniPlayer = Settings::settings.GetMiniPlayer();

	bool transparent = false;
	bool colorKeyEnabled = false;

	bool moving = false;
	bool resizing = false;

	std::string lastTempFile;

	std::map<std::string, OutputDevice> outputDevices;

	// =====================================================
	// ===================== Wayland =======================
	// =====================================================
	bool configured = false;
	uint32_t configureSerial = 0;

	int32_t pointerX = 0, pointerY = 0;

	bool inWindow = false;

	struct wl_registry *registry = nullptr;
	struct wl_compositor *compositor = nullptr;
	struct wl_seat *seat = nullptr;
	struct wl_pointer *pointer = nullptr;
	struct wl_surface *surface = nullptr;
	struct wl_display *display = nullptr;
	struct xdg_surface *xdgSurface = nullptr;
	struct xdg_wm_base *xdgWmBase = nullptr;
	struct xdg_toplevel *xdgTopLevel = nullptr;

	struct wl_registry_listener registryListener{0};

	struct wl_seat_listener seatListener{0};
	struct wl_pointer_listener pointerListener{0};

	struct xdg_surface_listener xdgSurfaceListener{0};
	struct xdg_wm_base_listener xdgWmBaseListener{0};
	struct xdg_toplevel_listener xdgTopLevelListener{0};

	// =====================================================
	// ====================== MPRIS ========================
	// =====================================================
	MPRIS mpris;

	// =====================================================
	// ==================== PipeWire =======================
	// =====================================================
	void GetDefaultDevice();
	void StopPipeWire();

	PipeWireData pwData = {0};

	struct pw_stream_events streamEvents{0};

	std::string defaultSinkName;
	
	pw_stream_state streamState = PW_STREAM_STATE_UNCONNECTED;

	BASS_CHANNELINFO exclusiveChannelInfo = {0, 0, 0, 0, 0, 0, 0, nullptr};

	int64_t lastQueueTime = 0;
	double queueSize = 0;

	// =====================================================
	// ================= Launcher Entry ====================
	// =====================================================
	DBusConnection *statusConnection = nullptr;

	int lastProgress = 0;
};

#endif