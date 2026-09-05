#include <iostream>
#include <locale>
#include <codecvt>

#include <SDL3/SDL.h>
#include <bass.h>
#include <bassflac.h>

#include <backends/imgui_impl_sdl3.h>

#include "MathCPP/Duration.hpp"

#include "Utils/Utils.hpp"

#include "CApp.h"
#include "FFTRenderer.hpp"

#ifdef __ANDROID__
#include "Platforms/Android.hpp"
#elif defined WIN32
#include <shellapi.h>
#elif defined __linux__
#include <malloc.h>
#endif

using namespace MathsCPP;
using namespace Fetcko;

#ifndef __ANDROID__
// See https://stackoverflow.com/questions/30412951/unresolved-external-symbol-imp-fprintf-and-imp-iob-func-sdl2
#pragma comment(lib, "legacy_stdio_definitions.lib")
extern "C" { FILE __iob_func[3] = { *stdin,*stdout,*stderr }; }
#endif

Delta timer;

// See https://stackoverflow.com/a/27195881
// 
// This allows the window to continue rendering
// while being dragged in Windows
// 
// SDL_EVENT_WINDOW_EXPOSED seems to be a more
// consistent event to hook into in SDL3 than
// SDL_EVENT_WINDOW_MOVED
// 
// SDL_EVENT_WINDOW_MOVED stops firing as soon
// as you stop moving the mouse, NOT when you
// let go of the window
bool EventFilter(void *pThis, SDL_Event *event) {
	auto *app = reinterpret_cast<CApp *>(pThis);
	const auto id = SDL_GetWindowID(app->GetSdlWindow());
	if ((!Settings::settings.GetMiniPlayer() && !app->GetPlatform()->IsFilterPaused())) {
		if (event->type == SDL_EVENT_WINDOW_EXPOSED) {
			app->OnLoop(timer.Update());
		} else if (event->type == SDL_EVENT_WINDOW_SAFE_AREA_CHANGED || 
			event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
			event->type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
			int w = 0, h = 0;
			auto scale = app->GetScale(app->GetSdlWindow(), &w, &h);

			app->OnResize(w, h, scale);
		}
	}

	return true;
}

#ifndef __ANDROID__
int main(int argc, char *argv[]) 
#else
int popRocks_main(CApp **pApp, std::function<void()> pAppSet)
#endif
{
	// Use fewer malloc arenas because
	// we aren't heavily dependent on
	// allocation speed / concurrency
#ifdef __linux__
	mallopt(M_ARENA_MAX, 2);
#endif

#ifdef USING_FLATPAK
	Utils::SetResourceFolder("/app/bin");
#endif

	SDL_Event event;
	bool running = true;

	CApp app;

	if (app.GetPlatform()->HandleExistingWindow(argc, argv))
		return 1;

#ifdef __ANDROID__
	*pApp = &app;
	pAppSet();
#else
	// Default presets require our working directory,
	// which is set in HandleExistingWindow()
	Settings::LoadPresets();
#endif

	LoggableClass loggableClass;
	Logger logger;
	logger.SetObject(&loggableClass);
	//logger.LogDebug(argv[0]);

	// Have to set preLoaded _before_ OnInit(),
	// else our integrations might try to start in
	// a "no song loaded" state
	if (argc > 1)
		app.SetPreLoaded(true);

	app.OnInit();

#ifdef WIN32
	SDL_SetEventFilter(EventFilter, &app);
#endif

	app.GetPlatform()->HookWindow(app.GetMiniPlayer());

#ifndef __ANDROID__
	if(argc > 1) {
#ifdef WIN32
		int wargc;
		if (LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), &wargc); wargv && wargc > 1)
			app.LoadFile(wargv[1]);
#else
		logger.LogDebug("File prepared: ", argv[1]);
		auto utf8 = std::string(argv[1]);
		app.LoadFile(utf8);
#endif
	}
#else
	dynamic_cast<Android*>(app.GetPlatform().get())->LoadFileNextLoop(Filesystem::GetPath("../../current"), false);
#endif

	Vector2i mousePos{ 0, 0 };
	auto mouseTimer = std::chrono::system_clock::now();
	bool mouseButtonDown = false;
	bool mouseDragged = false;
	float wheelAccum = 0.0f;

	uint16_t skipEvents = 0;

#if GUI
	auto &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
#endif

	int w = 0, h = 0;
	while(running) {
		while(SDL_PollEvent(&event)) {
#if GUI
			if (!app.GetMiniPlayer()) {
				ImGui_ImplSDL3_ProcessEvent(&event);
				if (io.WantCaptureKeyboard || io.WantCaptureMouse)
					app.UpdateUi();
			} else if (io.WantCaptureKeyboard || io.WantCaptureMouse) {
				io.WantCaptureKeyboard = false;
				io.WantCaptureMouse = false;
			}
#endif

			switch(event.type) {
				case SDL_EVENT_QUIT:
					running = false;
					break;
				case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED: {
					auto window = SDL_GetWindowFromID(event.window.windowID);
					const auto oldScale = app.GetScale();

					auto scale = app.GetScale(window, &w, &h);
					if (oldScale != scale) {
						app.OnResize(w, h, scale);
					}
					break;
				} case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
					app.OnResize(
						event.window.data1 / app.GetPlatform()->GetScale(),
						event.window.data2 / app.GetPlatform()->GetScale(),
						app.GetScale()
					);
					break;
				} case SDL_EVENT_WINDOW_MOVED:
					if (mouseButtonDown) break;

					if (!app.GetMiniPlayer()) {
						Settings::settings.SetWindowX(event.window.data1);
						Settings::settings.SetWindowY(event.window.data2);

						logger.LogDebug("Window moved to (", event.window.data1, ", ", event.window.data2, ")");
					}

					app.UpdateHdrProperties();
					break;
				case SDL_EVENT_WINDOW_HDR_STATE_CHANGED:
					app.UpdateHdrProperties(true);
					break;
				case SDL_EVENT_KEY_DOWN:
#if GUI
					if (io.WantCaptureKeyboard || app.IsEnteringText() /* Disable hotkeys on text entry */) break;

					else 
#endif
					if (event.key.key == SDLK_MEDIA_NEXT_TRACK ||
						(event.key.key == SDLK_D && (event.key.mod & SDL_KMOD_CTRL)) ||
						(event.key.key == SDLK_RIGHT && (event.key.mod & SDL_KMOD_CTRL)))
						app.NextTrack();
					else if (event.key.key == SDLK_MEDIA_PREVIOUS_TRACK ||
						(event.key.key == SDLK_A && (event.key.mod & SDL_KMOD_CTRL)) ||
						(event.key.key == SDLK_LEFT && (event.key.mod & SDL_KMOD_CTRL)))
						app.PreviousTrack();
					else if ((event.key.key == SDLK_VOLUMEUP ||
						(event.key.key == SDLK_W && (event.key.mod & SDL_KMOD_CTRL)) ||
						(event.key.key== SDLK_UP && (event.key.mod & SDL_KMOD_CTRL))) &&
						app.GetControls().GetExclusiveIndicator().IsExclusive())
						app.GetControls().GetVolume().VolumeUp();
					else if ((event.key.key == SDLK_VOLUMEDOWN ||
						(event.key.key == SDLK_S && (event.key.mod & SDL_KMOD_CTRL)) ||
						(event.key.key == SDLK_DOWN && (event.key.mod & SDL_KMOD_CTRL))) &&
						app.GetControls().GetExclusiveIndicator().IsExclusive())
						app.GetControls().GetVolume().VolumeDown();
					else if (event.key.key == SDLK_RIGHT ||
						event.key.key == SDLK_D)
						app.GetAlbumArt().NextBin();
					else if (event.key.key == SDLK_LEFT ||
						event.key.key == SDLK_A)
						app.GetAlbumArt().PreviousBin();
					else if (event.key.key == SDLK_P && !app.GetMiniPlayer())
						app.SaveBlurFBO();
					else if (event.key.key == SDLK_SPACE || event.key.key == SDLK_MEDIA_PLAY || event.key.key == SDLK_MEDIA_PLAY_PAUSE)
						app.TogglePlaying();
					else if (event.key.key == SDLK_RETURN && event.key.mod & SDL_KMOD_ALT && !app.GetMiniPlayer())
						app.ToggleFullscreen();
					else if (event.key.key == SDLK_ESCAPE)
						running = false;
					else if (event.key.key >= SDLK_F1 && event.key.key <= SDLK_F12 && !app.GetMiniPlayer())
						app.LoadPreset(event.key.key - SDLK_F1);
					else if (event.key.key == SDLK_S)
						app.SyncToNearestBeat();
					else if (event.key.key == SDLK_PAGEDOWN)
						app.GetControls().PageDown();
					else if (event.key.key == SDLK_PAGEUP)
						app.GetControls().PageUp();
					else if (event.key.key == SDLK_HOME)
						app.GetControls().Home();
					else if (event.key.key == SDLK_END)
						app.GetControls().End();
					else if (event.key.key == SDLK_UP)
						app.GetControls().AddToScrollOffset(-1);
					else if (event.key.key == SDLK_DOWN)
						app.GetControls().AddToScrollOffset(1);
					break;
				case SDL_EVENT_KEY_UP:
					if (app.IsEnteringText() /* Disable hotkeys on text entry */) break;

					// As we can potentially have multiple windows open
					// while toggling these settings, we explicitly listen
					// for key _up_ events. Key _down_ events may fire
					// for each window individually, causing an infinite
					// loop of toggling.
					if (event.key.key == SDLK_V)
						app.SetVulkan(!app.GetVulkan());
					else if (event.key.key == SDLK_M)
						app.SetMiniPlayer(!app.GetMiniPlayer());
					else if (event.key.key == SDLK_R)
						app.ResetWindow();
					break;
				case SDL_EVENT_MOUSE_MOTION:
					mousePos.x = static_cast<int32_t>(event.motion.x);
					mousePos.y = static_cast<int32_t>(event.motion.y);

#ifdef __linux__
					mousePos.x *= app.GetScale();
					mousePos.y *= app.GetScale();
#endif

					if (mouseButtonDown) {
						if (!skipEvents) {
							if (mouseDragged && app.OnMouseDragged(mousePos)) {
								skipEvents = 1;
							}
						} else --skipEvents;

						// Only consider our mouse dragged if we've held the
						// button for more than 100ms
						if (!mouseDragged && std::chrono::system_clock::now() - mouseTimer > 100ms)
							mouseDragged = true;
					} else app.OnMouseMoved(mousePos);
					break;
				case SDL_EVENT_MOUSE_BUTTON_DOWN:
					if (event.button.button == SDL_BUTTON_LEFT
#if GUI
						&& !io.WantCaptureMouse
#endif
						&& app.OnMouseDown(mousePos)) {
						mouseButtonDown = true;
						mouseDragged = false;
						mouseTimer = std::chrono::system_clock::now();
					} else if (event.button.button == SDL_BUTTON_RIGHT) {
						app.OnMouseRightClicked(mousePos);
					}
					break;
				case SDL_EVENT_MOUSE_BUTTON_UP:
					if (event.button.button == SDL_BUTTON_LEFT
#if GUI
						&& !io.WantCaptureMouse
#endif
						) {
						if (!mouseDragged) {
							if (app.OnMouseClicked(mousePos))
								running = false;
						} else app.OnMouseUp(mousePos);

						if (!app.GetPlatform()->IsResizing())
							app.GetAlbumArt().OnMouseUp(mousePos);

						mouseButtonDown = false;
						mouseDragged = false;
					}
					break;
				case SDL_EVENT_DROP_FILE: {
					app.LoadFile(
#ifdef WIN32
						Utils::ToUTF16(const_cast<const char*>(
#endif
							event.drop.data
#ifdef WIN32
						))
#endif
					);
					break;
				}
				case SDL_EVENT_DISPLAY_ADDED:
				case SDL_EVENT_DISPLAY_REMOVED:
				case SDL_EVENT_DISPLAY_MOVED:
					app.UpdateDisplayBoundingBox();
					break;
				case SDL_EVENT_MOUSE_WHEEL:
					// If we changed directions, reset to 0
					if ((wheelAccum > 0 && event.wheel.y < 0) ||
						(wheelAccum < 0 && event.wheel.y > 0))
						wheelAccum = event.wheel.y;
					else
						wheelAccum += event.wheel.y;

					if (wheelAccum + event.wheel.y >= 1) {
						app.AddToScrollOffset((
							event.wheel.direction == SDL_MOUSEWHEEL_NORMAL ?
							-1 :
							// Linux already inverts our Y direction on
							// inverted scroll, so don't change anything
#ifdef WIN32
							1
#else
							-1
#endif
						));
						wheelAccum -= 1.0f;
					} else if (wheelAccum + event.wheel.y <= -1) {
						app.AddToScrollOffset((
							event.wheel.direction == SDL_MOUSEWHEEL_NORMAL ?
							1 :
							// Linux already inverts our Y direction on
							// inverted scroll, so don't change anything
#ifdef WIN32
							-1
#else
							1
#endif
						));
						wheelAccum += 1.0f;
					}
					
					break;
				default:
					break;
			}

			// Mini-player fades controls based
			// on mouse enter / leave, not on
			// interaction itself.
			if (!app.GetMiniPlayer())
				app.FadeControls(true);
		}

		app.OnLoop(timer.Update());
	}

#ifdef __ANDROID__
	*pApp = nullptr;
	pAppSet();
#endif

	app.OnDestroy();

	return 0;
}