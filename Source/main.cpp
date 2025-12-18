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
#endif

using namespace MathsCPP;
using namespace Fetcko;

#ifndef __ANDROID__
// See https://stackoverflow.com/questions/30412951/unresolved-external-symbol-imp-fprintf-and-imp-iob-func-sdl2
#pragma comment(lib, "legacy_stdio_definitions.lib")
extern "C" { FILE __iob_func[3] = { *stdin,*stdout,*stderr }; }
#endif

#ifndef __ANDROID__
int main(int argc, char *argv[]) 
#else
int popRocks_main(CApp **pApp, std::function<void()> pAppSet)
#endif
{
#ifdef USING_FLATPAK
	Utils::SetResourceFolder("/app/bin");
#endif

	SDL_Event event;
	bool running = true;

	CApp app;
#ifdef __ANDROID__
	*pApp = &app;
	pAppSet();
#endif

	LoggableClass loggableClass;
	Logger logger;
	logger.SetObject(&loggableClass);
	//logger.LogDebug(argv[0]);

	app.OnInit();

#ifndef __ANDROID__
	if(argc > 1) {
		logger.LogDebug("File prepared: ", argv[1]);
		auto ascii = std::string(argv[1]);
		app.LoadFile(std::wstring(ascii.begin(), ascii.end()));
	}
#else
	dynamic_cast<Android*>(app.GetPlatform().get())->LoadFileNextLoop(Settings::settings.GetPath("../../current"), false);
#endif

	Delta time;
	Vector2i mousePos{ 0, 0 };
	bool mouseButtonDown = false;

#if GUI
	auto &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
#endif

	int w = 0, h = 0;
	while(running) {
		while(SDL_PollEvent(&event)) {
#if GUI
			ImGui_ImplSDL3_ProcessEvent(&event);
			if (io.WantCaptureKeyboard || io.WantCaptureMouse)
				app.UpdateUi();
#endif

			switch(event.type) {
				case SDL_EVENT_QUIT:
					running = false;
					break;
				case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
				case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
					auto window = SDL_GetWindowFromID(event.window.windowID);
					auto scale = app.GetScale(window, &w, &h);

					app.OnResize(w, h, scale);
					break;
				} case SDL_EVENT_WINDOW_MOVED:
					Settings::settings.SetWindowX(event.window.data1);
					Settings::settings.SetWindowY(event.window.data2);
					logger.LogDebug("Window moved to (", event.window.data1, ", ", event.window.data2, ")");
					app.UpdateHdrProperties();
					break;
				case SDL_EVENT_WINDOW_HDR_STATE_CHANGED:
					app.UpdateHdrProperties();
					break;
				case SDL_EVENT_KEY_DOWN:
#if GUI
					if (io.WantCaptureKeyboard) break;

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
					else if (event.key.key == SDLK_UP ||
						event.key.key == SDLK_W) {
						auto lock = app.GetAlbumArt().Lock();
						app.GetAlbumArt().ResetBin();
					} else if (event.key.key == SDLK_P)
						app.SaveBlurFBO();
					else if (event.key.key == SDLK_SPACE || event.key.key == SDLK_MEDIA_PLAY)
						app.TogglePlaying();
					else if (event.key.key == SDLK_RETURN && event.key.mod & SDL_KMOD_ALT)
						app.ToggleFullscreen();
					else if (event.key.key == SDLK_ESCAPE)
						running = false;
					else if (event.key.key == SDLK_V)
						app.GetControls().GetVolume().ToggleVolumeControl();
					else if (event.key.key >= SDLK_F1 && event.key.key <= SDLK_F12)
						app.LoadPreset(event.key.key - SDLK_F1);
					else if (event.key.key == SDLK_S)
						app.SyncToNearestBeat();
					else if (event.key.key == SDLK_R)
						app.LoadPreset(Preset::Random());
					break;
				case SDL_EVENT_MOUSE_MOTION:
					mousePos.x = static_cast<int32_t>(event.motion.x);
					mousePos.y = static_cast<int32_t>(event.motion.y);

					if (mouseButtonDown) {
						app.OnMouseDragged(mousePos);
					}
					break;
				case SDL_EVENT_MOUSE_BUTTON_DOWN:
					if (event.button.button == SDL_BUTTON_LEFT
#if GUI
						&& !io.WantCaptureMouse
#endif
						&& app.OnMouseDown(mousePos)) {
						mouseButtonDown = true;
					}
					break;
				case SDL_EVENT_MOUSE_BUTTON_UP:
					if (event.button.button == SDL_BUTTON_LEFT
#if GUI
						&& !io.WantCaptureMouse
#endif
						) {
						if (!mouseButtonDown)
							app.OnMouseClicked(mousePos);
						else
							mouseButtonDown = false;
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
				default:
					break;
			}

			app.FadeControls(true);
		}

		app.OnLoop(time.Update());
	}

#ifdef __ANDROID__
	*pApp = nullptr;
	pAppSet();
#endif

	app.OnDestroy();

	return 0;
}