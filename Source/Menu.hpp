#pragma once

#include <filesystem>

#include <imgui.h>
#include <imgui_stdlib.h>
#include <imgui_freetype.h>
#include <nfd.hpp>

#include "Utils/Utils.hpp"

#include "Playlist.hpp"
#include "Preset.hpp"

using namespace Fetcko;

class Menu {
public:
	Menu() {
		NFD_Init();
	}

	~Menu() {
		delete[] presetSelections;
	}

	void OnResize(int width, int height, float scale) {
		this->width = width;
		this->scale = scale;

		if (!font) {
			font = ImGui::GetIO().Fonts->AddFontFromFileTTF(
				Utils::GetResource("KurintoSans-Rg.ttf").u8string().c_str(),
				20
			);
		}
		
		ImGui::GetStyle().ScaleAllSizes(scale);
		ImGui::GetStyle().FontScaleMain = scale;
	}

	float OnLoop(const LightPack &lightPack, AlbumArt &albumArt, Context &context) {
		ImGui::Begin(
			"Menu",
			nullptr,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoBackground |
			ImGuiWindowFlags_MenuBar
		);

		ImGui::SetWindowPos({ 0.0f, 0.0f });
		ImGui::SetWindowSize({ static_cast<float>(width), 0 });

		ImGui::BeginMenuBar();

		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Open File", "Ctrl-O", false, true)) {
				nfdnchar_t *outPath;

				std::wstring extensions;
				for (const auto &[i, extension] : Utils::Enumerate(Playlist::GetSupportedExtensions())) {
					const auto noDot = extension.substr(1);
					extensions += std::wstring(noDot.begin(), noDot.end()) + (i == Playlist::GetSupportedExtensions().size() - 1 ? L"" : L",");
				}

				nfdnfilteritem_t filters[2] = { { L"Music", extensions.c_str() }, { L"Cue", L"cue" }};
				nfdopendialognargs_t args = { 0 };
				args.filterList = filters;
				args.filterCount = 2;
				nfdresult_t result = NFD_OpenDialogN_With(&outPath, &args);
				if (result == NFD_OKAY) {
					if (onOpen) onOpen(outPath);
					NFD_FreePathN(outPath);
				}	
			}

			if (ImGui::MenuItem("Open Folder", "Ctrl-Shift-O", false, true)) {
				nfdnchar_t *outPath;
				nfdresult_t result = NFD_PickFolderN(&outPath, nullptr);
				if (result == NFD_OKAY) {
					if (onOpen) onOpen(outPath);
					NFD_FreePathN(outPath);
				}
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Quit", "Alt-F4", false, true)) {
				if (onQuit)
					onQuit();
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Visualizer Options")) {
			if (ImGui::BeginMenu("Visualization Type")) {
				fft = Settings::settings.GetRenderer() == "fft";
				fftLine = Settings::settings.GetRenderer() == "fftline";
				oscilloscope = Settings::settings.GetRenderer() == "osc";

				if (ImGui::MenuItem("FFT", nullptr, &fft)) {
					if (onVisualizationTypeChanged)
						onVisualizationTypeChanged("fft");
				} else if (ImGui::MenuItem("FFT Line", nullptr, &fftLine)) {
					if (onVisualizationTypeChanged)
						onVisualizationTypeChanged("fftline");
				} else if (ImGui::MenuItem("Oscilloscope", nullptr, &oscilloscope)) {
					if (onVisualizationTypeChanged)
						onVisualizationTypeChanged("osc");
				}

				ImGui::EndMenu();
			}

			fftSize = Settings::settings.GetFftSize();
			if (ImGui::BeginMenu("FFT Size", !oscilloscope)) {
				for (const auto &size : { 256, 512, 1024, 4096, 8192, 16384 }) {
					bool selected = (fftSize == size);
					if (ImGui::MenuItem(std::to_string(size).c_str(), nullptr, &selected)) {
						if (onFftSizeChanged)
							onFftSizeChanged(size);
					}
				}

				ImGui::EndMenu();
			}

			ImGui::Separator();

			bufferSize = Settings::settings.GetBufferLength();
			if (ImGui::SliderInt("Buffer size", &bufferSize, 1, 4096)) {
				if (onBufferSizeChanged)
					onBufferSizeChanged(bufferSize);
			}

			decayTime = Settings::settings.GetDecayTime().AsSeconds();
			if (ImGui::SliderFloat("Decay time", &decayTime, 0.01, 10, "%.2f")) {
				if (onDecayTimeChanged)
					onDecayTimeChanged(decayTime);
			}

			fadeTime = Settings::settings.GetFadeTime().AsSeconds();
			if (ImGui::SliderFloat("Fade time", &fadeTime, 0.01, 10, "%.2f")) {
				if (onFadeTimeChanged)
					onFadeTimeChanged(fadeTime);
			}

			pulse = Settings::settings.GetPulse();
			//ImGui::BeginDisabled(!fft);
			if (ImGui::MenuItem("Pulse", nullptr, &pulse)) {
				if (onPulseChanged)
					onPulseChanged(pulse);
			}
			//ImGui::EndDisabled();

			pulseTime = Settings::settings.GetPulseTime().AsSeconds();
			ImGui::BeginDisabled(!pulse || !fft);
			if (ImGui::SliderFloat("Pulse time", &pulseTime, 0.01, 10, "%.2f")) {
				if (onPulseTimeChanged)
					onPulseTimeChanged(pulseTime);
			}
			ImGui::EndDisabled();

			ImGui::Separator();

			strobe = Settings::settings.GetStrobe();
			if (ImGui::MenuItem("Strobe", nullptr, &strobe)) {
				if (onStrobeChanged)
					onStrobeChanged(strobe);
			}

			strobeIntensity = Settings::settings.GetStrobeIntensity();
			if (ImGui::SliderFloat("Strobe intensity", &strobeIntensity, 0.0, 1.0, "%.2f")) {
				if (onStrobeIntensityChanged)
					onStrobeIntensityChanged(strobeIntensity);
			}

			ImGui::Separator();

			rotate = Settings::settings.GetRotating();
			if (ImGui::MenuItem("Rotate", nullptr, &rotate)) {
				if (onRotatingChanged)
					onRotatingChanged(!Settings::settings.GetRotating());
			}

			rpm = Settings::settings.GetRotationSpeed() / (360.0f / 60.0f);
			ImGui::BeginDisabled(!rotate);
			if (ImGui::SliderFloat("RPM", &rpm, 0.1f, 100.0f, "%.1f")) {
				if (onRpmChanged)
					onRpmChanged(rpm);
			}
			ImGui::EndDisabled();

			ImGui::Separator();

			detectBpm = Settings::settings.GetDetectBpm();
			if (ImGui::MenuItem("Detect BPM", nullptr, &detectBpm)) {
				if (onDetectBpmChanged)
					onDetectBpmChanged(!Settings::settings.GetDetectBpm());
			}

			ImGui::Separator();

			blur = Settings::settings.GetBlur();
			if (ImGui::MenuItem("Blur", nullptr, &blur)) {
				if (onBlurChanged)
					onBlurChanged(!Settings::settings.GetBlur());
			}

			blurIntensity = Settings::settings.GetBlurIntensity();
			ImGui::BeginDisabled(!blur);
			if (ImGui::SliderFloat("Blur intensity", &blurIntensity, 0.01f, 2.0f, "%.2f")) {
				if (onBlurIntensityChanged)
					onBlurIntensityChanged(blurIntensity);
			}
			ImGui::EndDisabled();

			blurOpacity = Settings::settings.GetBlurOpacity();
			ImGui::BeginDisabled(!blur);
			if (ImGui::SliderFloat("Blur opacity", &blurOpacity, 0.0f, 1.0f, "%.2f")) {
				if (onBlurOpacityChanged)
					onBlurOpacityChanged(blurOpacity);
			}
			ImGui::EndDisabled();

			ImGui::Separator();

			if (ImGui::BeginMenu("Effect")) {
				auto files = Utils::GetFiles(Utils::GetResourceFolder() / "Effects");

				for (const auto &file : files) {
					auto effectName = file.stem().u8string();
					effectName = effectName.substr(effectName.find_first_of('-') + 1);

					std::ifstream inFile(file, std::ios::in);
					std::string line;
					std::size_t lineNumber = 0;
					std::string friendlyName;
					while (std::getline(inFile, line) && ++lineNumber <= 3 /* We expect a name comment within the first 3 lines */) {
						if (line.find("// Name:") == 0) {
							if (auto split = Utils::Split(line, ':'); split.size() == 2) {
								Utils::ltrim(split[1]);
								friendlyName = split[1];
								break;
							}
						}
					}

					bool selected = Settings::settings.GetEffect() == effectName || Settings::settings.GetEffect() == friendlyName;
					if (ImGui::MenuItem(friendlyName.empty() ? effectName.c_str() : friendlyName.c_str(), nullptr, &selected)) {
						if (onEffectChanged)
							onEffectChanged(effectName);
					}
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Effect Settings")) {
				if (Settings::settings.GetEffect() != "noeffect") {
					effectIntensity = Settings::settings.GetEffectIntensity();

					if (ImGui::SliderFloat("Intensity", &effectIntensity, 1.0f, 25.0f, "%.2f")) {
						if (onEffectIntensityChanged)
							onEffectIntensityChanged(effectIntensity);
					}
				}

				effectXOffset = Settings::settings.GetEffectXOffset();
				if (ImGui::SliderFloat("X Offset", &effectXOffset, -10.0f, 10.0f, "%.2f")) {
					if (onEffectXOffsetChanged)
						onEffectXOffsetChanged(effectXOffset);
				}

				effectYOffset = Settings::settings.GetEffectYOffset();
				if (ImGui::SliderFloat("Y Offset", &effectYOffset, -10.0f, 10.0f, "%.2f")) {
					if (onEffectYOffsetChanged)
						onEffectYOffsetChanged(effectYOffset);
				}

				effectRadiation = Settings::settings.GetEffectRadiation();
				if (ImGui::SliderFloat("Radiation", &effectRadiation, -10.0f, 10.0f, "%.2f")) {
					if (onEffectRadiationChanged)
						onEffectRadiationChanged(effectRadiation);
				}

				ImGui::EndMenu();
			}

			ImGui::Separator();

			radius = Settings::settings.GetRadius();
			if (ImGui::SliderInt("Album art radius", &radius, 50, 720)) {
				if (onRadiusChanged)
					onRadiusChanged(radius);
			}

			lineWidth = Settings::settings.GetWidth();
			ImGui::BeginDisabled(fft);
			if (ImGui::SliderFloat("Line width", &lineWidth, 0.5, 10, "%.1f")) {
				if (onLineWidthChanged)
					onLineWidthChanged(lineWidth);
			}
			ImGui::EndDisabled();

			ImGui::Separator();

			if (ImGui::MenuItem("Reset window")) {
				if (onResetWindow)
					onResetWindow();
			}

			ImGui::EndMenu();
		}

		presetIndex = Settings::settings.GetPresetIndex();
		if (ImGui::BeginMenu("Presets")) {
			presetX = ImGui::GetWindowPos().x;
			auto presets = Preset::GetPresets();
			if (presets.size() != numPresets) {
				delete[] presetSelections;
				presetSelections = new bool[presets.size()];
				numPresets = presets.size();
			}
			for (const auto &[i, preset] : Utils::Enumerate(presets)) {
				presetSelections[i] = presetIndex && *presetIndex == i;
				if (ImGui::MenuItem(preset.GetName().c_str(), nullptr, &presetSelections[i])) {
					if (onPresetChanged)
						onPresetChanged(i);
				}

				if (ImGui::BeginPopupContextItem()) {
					ImGui::Text("Delete?");
					if (ImGui::Button("No"))
						ImGui::CloseCurrentPopup();
					ImGui::SameLine();
					if (ImGui::Button("Yes")) {
						Preset::RemovePreset(i);

						if (onPresetChanged && presetIndex) {
							// If we deleted the _current_ preset,
							// unselect it.
							if (*presetIndex == i)
								onPresetChanged(std::nullopt);

							// If we deleted a preset _below_ the
							// current one, shift the current one
							else if (*presetIndex > i)
								onPresetChanged(*presetIndex - 1);
						}
					}
					ImGui::EndPopup();
				}
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Save current settings as preset...")) {
				currentPresetName.clear();
				newPresetPopup = true;
			}
			
			ImGui::EndMenu();
		}

		// See https://github.com/ocornut/imgui/issues/5684#issuecomment-1247928651
		if (newPresetPopup) {
			ImGui::SetNextWindowPos(ImVec2(presetX, context.GetYOffset()));
			ImGui::OpenPopup("Enter preset name...");

			bool open = true;
			if (ImGui::BeginPopupModal("Enter preset name...", &open, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize)) {
				
				ImGui::InputText("Preset name", &currentPresetName);

				if (ImGui::Button("Cancel"))
					open = false;

				ImGui::SameLine();

				if (ImGui::Button("Save")) {
					Preset preset(
						currentPresetName,
						Settings::settings.GetBufferLength(),
						Settings::settings.GetDecayTime(),
						Settings::settings.GetFadeTime(),
						Settings::settings.GetPulse(),
						Settings::settings.GetPulseTime(),
						Settings::settings.GetRotating(),
						Settings::settings.GetRotationSpeed(),
						Settings::settings.GetBlur(),
						Settings::settings.GetBlurIntensity(),
						Settings::settings.GetBlurOpacity(),
						Settings::settings.GetEffect(),
						Settings::settings.GetEffectIntensity(),
						Settings::settings.GetEffectXOffset(),
						Settings::settings.GetEffectYOffset(),
						Settings::settings.GetEffectRadiation()
					);

					Preset::AddPreset(std::move(preset));

					if (onPresetChanged)
						onPresetChanged(Preset::GetPresets().size() - 1);

					open = false;
				}

				ImGui::EndPopup();
			}

			newPresetPopup = open;
		}

		if (ImGui::BeginMenu("Playlist Options")) {
			currentSongVisible = Settings::settings.GetCurrentSongVisible();
			if (ImGui::MenuItem("Current track always visible?", nullptr, &currentSongVisible)) {
				if (onCurrentSongVisibleChanged)
					onCurrentSongVisibleChanged(currentSongVisible);
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Color Selection Options")) {
			minPercentage = Settings::settings.GetColorSelection().minPercentage * 100;
			if (ImGui::SliderInt("Minimum % of pixels vs. dominant color", &minPercentage, 1, 100)) {
				if (onColorSelectionChanged) {
					auto newColorSelection = Settings::settings.GetColorSelection();
					newColorSelection.minPercentage = minPercentage / 100.0;
					onColorSelectionChanged(newColorSelection);
				}
			}
			minHueSeparation = Settings::settings.GetColorSelection().minHueSeparation;
			if (ImGui::SliderInt("Minimum hue separation (in degrees)", &minHueSeparation, 1, 360)) {
				if (onColorSelectionChanged) {
					auto newColorSelection = Settings::settings.GetColorSelection();
					newColorSelection.minHueSeparation = minHueSeparation;
					onColorSelectionChanged(newColorSelection);
				}
			}
			minValueSeparation = Settings::settings.GetColorSelection().minValueSeparation;
			if (ImGui::SliderFloat("Minimum value separation (0 - 1)", &minValueSeparation, 0.0f, 1.0f)) {
				if (onColorSelectionChanged) {
					auto newColorSelection = Settings::settings.GetColorSelection();
					newColorSelection.minValueSeparation = minValueSeparation;
					onColorSelectionChanged(newColorSelection);
				}
			}
			minimumDistance = Settings::settings.GetColorSelection().minRgbSeparation;
			if (ImGui::SliderFloat("Minimum distance between RGB values (0 - 1)", &minimumDistance, 0.0f, 1.0f)) {
				if (onColorSelectionChanged) {
					auto newColorSelection = Settings::settings.GetColorSelection();
					newColorSelection.minRgbSeparation = minimumDistance;
					onColorSelectionChanged(newColorSelection);
				}
			}
			minimumSaturation = Settings::settings.GetColorSelection().minSaturation;
			if (ImGui::SliderFloat("Minimum saturation (0 - 1)", &minimumSaturation, 0.0f, 1.0f)) {
				if (onColorSelectionChanged) {
					auto newColorSelection = Settings::settings.GetColorSelection();
					newColorSelection.minSaturation = minimumSaturation;
					onColorSelectionChanged(newColorSelection);
				}
			}
			minimumValue = Settings::settings.GetColorSelection().minValue;
			if (ImGui::SliderFloat("Minimum value (0 - 1)", &minimumValue, 0.0f, 1.0f)) {
				if (onColorSelectionChanged) {
					auto newColorSelection = Settings::settings.GetColorSelection();
					newColorSelection.minValue = minimumValue;
					onColorSelectionChanged(newColorSelection);
				}
			}

			ImGui::Separator();

			ImGui::Text("Currently selected colors:");

			{
				auto lock = albumArt.Lock();
				const auto &selectedColors = albumArt.GetSelectedColors();
				for (const auto &[i, color] : Utils::Enumerate(selectedColors)) {
					ImGui::ColorButton(("Color " + std::to_string(i + 1)).c_str(), ImVec4(color.r, color.g, color.b, color.a));
					if (i != selectedColors.size() - 1 && ((i + 1) % 20)) // about 20 colors fit in the existing menu's width
						ImGui::SameLine();
				}
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Reset to default...")) {
				if (onColorSelectionChanged) {
					onColorSelectionChanged(Settings::ColorSelection());
				}
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Device Options")) {
			if (ImGui::BeginMenu("Input device")) {
				BASS_WASAPI_DEVICEINFO info;
				const auto &inputDevice = Settings::settings.GetInputDevice();
				for (int i = 0; BASS_WASAPI_GetDeviceInfo(i, &info); ++i) {
					if ((info.flags & BASS_DEVICE_INPUT) && // device is an input device
					   !(info.flags & BASS_DEVICE_LOOPBACK) && // device is NOT a loopback device
						(info.flags & BASS_DEVICE_ENABLED)) { // and it is enabled
						bool selected = 
							(!inputDevice.empty() && strncmp(info.id, inputDevice.c_str(), std::min(strlen(info.id), inputDevice.size())) == 0) ||
							 (inputDevice.empty() && (info.flags & BASS_DEVICE_DEFAULT));
						if (ImGui::MenuItem(info.name, nullptr, &selected)) {
							if (onInputDeviceChanged)
								onInputDeviceChanged(std::string(info.id, info.id + strlen(info.id)));
						}
					}
				}

				ImGui::EndMenu();
			}

			listening = Settings::settings.GetListening();
			if (ImGui::MenuItem("Listen to selected input device", nullptr, &listening)) {
				if (onListeningChanged)
					onListeningChanged(listening);
			}

			if (ImGui::BeginMenu("Output device")) {
				BASS_DEVICEINFO info;
				const auto &outputDevice = Settings::settings.GetOutputDevice();
				for (int i = 1; BASS_GetDeviceInfo(i, &info); ++i) {
					if ((info.flags & BASS_DEVICE_ENABLED) && // // device is enabled
						strlen(info.driver)) { // device has a driver (this excludes the "Default" device without dealing with i18n)
						bool selected = 
							(!outputDevice.empty() && strncmp(info.driver, outputDevice.c_str(), std::min(strlen(info.driver), outputDevice.size())) == 0) ||
							 (outputDevice.empty() && (info.flags & BASS_DEVICE_DEFAULT));

						if (ImGui::MenuItem(info.name, nullptr, &selected)) {
							if (onOutputDeviceChanged)
								onOutputDeviceChanged(std::string(info.driver, info.driver + strlen(info.driver)));
						}
					}
				}

				ImGui::EndMenu();
			}

			loopback = Settings::settings.GetLoopback();
			if (ImGui::MenuItem("Listen to selected output device", nullptr, &loopback)) {
				if (onLoopbackChanged)
					onLoopbackChanged(loopback);
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("LightPack Integration", lightPack.IsActive())) {
			if (ImGui::BeginMenu("LightPack Visualization Type", lightPack.IsActive())) {
				intensity = Settings::settings.GetLightPackVisualizationType() == "intensity";
				color = Settings::settings.GetLightPackVisualizationType() == "color";
				colorAndIntensity = Settings::settings.GetLightPackVisualizationType() == "colorintensity";

				if (ImGui::MenuItem("Intensity", nullptr, &intensity)) {
					if (onLightPackVisualizationTypeChanged)
						onLightPackVisualizationTypeChanged("intensity");
				} else if (ImGui::MenuItem("Color", nullptr, &color)) {
					if (onLightPackVisualizationTypeChanged)
						onLightPackVisualizationTypeChanged("color");
				} else if (ImGui::MenuItem("Color + Intensity", nullptr, &colorAndIntensity)) {
					if (onLightPackVisualizationTypeChanged)
						onLightPackVisualizationTypeChanged("colorintensity");
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("LightPack Mapping", lightPack.IsActive())) {
				default = Settings::settings.GetLightPackMapping() == "default";
				mine = Settings::settings.GetLightPackMapping() == "mine";
				topToBottom = Settings::settings.GetLightPackMapping() == "ttb";
				bottomToTop = Settings::settings.GetLightPackMapping() == "btt";

				if (ImGui::MenuItem("Default", nullptr, &default)) {
					if (onLightPackMappingChanged)
						onLightPackMappingChanged("default");
				} else if (ImGui::MenuItem("Mine", nullptr, &mine)) {
					if (onLightPackMappingChanged)
						onLightPackMappingChanged("mine");
				} else if (ImGui::MenuItem("Top-to-bottom", nullptr, &topToBottom)) {
					if (onLightPackMappingChanged)
						onLightPackMappingChanged("ttb");
				} else if (ImGui::MenuItem("Bottom-to-top", nullptr, &bottomToTop)) {
					if (onLightPackMappingChanged)
						onLightPackMappingChanged("btt");
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("LightPack Focus Area", lightPack.IsActive())) {
				superBass = Settings::settings.GetLightPackFocusArea() == "superbass";
				subBass = Settings::settings.GetLightPackFocusArea() == "subbass";
				bass = Settings::settings.GetLightPackFocusArea() == "bass";
				bassAndMid = Settings::settings.GetLightPackFocusArea() == "bassandmid";
				bassMidAndALittleHighEnd = Settings::settings.GetLightPackFocusArea() == "bassmidandalittlehighend";
				halfNyquist = Settings::settings.GetLightPackFocusArea() == "halfnyquist";
				nyquist = Settings::settings.GetLightPackFocusArea() == "nyquist";

				if (ImGui::MenuItem("Super bass", nullptr, &superBass)) {
					if (onLightPackFocusAreaChanged)
						onLightPackFocusAreaChanged("superbass");
				} else if (ImGui::MenuItem("Sub-bass", nullptr, &subBass)) {
					if (onLightPackFocusAreaChanged)
						onLightPackFocusAreaChanged("subbass");
				} else if (ImGui::MenuItem("Bass", nullptr, &bass)) {
					if (onLightPackFocusAreaChanged)
						onLightPackFocusAreaChanged("bass");
				} else if (ImGui::MenuItem("Bass and mid", nullptr, &bassAndMid)) {
					if (onLightPackFocusAreaChanged)
						onLightPackFocusAreaChanged("bassandmid");
				} else if (ImGui::MenuItem("Bass, mid, and a little high end", nullptr, &bassMidAndALittleHighEnd)) {
					if (onLightPackFocusAreaChanged)
						onLightPackFocusAreaChanged("bassmidandalittlehighend");
				} else if (ImGui::MenuItem("Half Nyquist", nullptr, &halfNyquist)) {
					if (onLightPackFocusAreaChanged)
						onLightPackFocusAreaChanged("halfnyquist");
				} else if (ImGui::MenuItem("Nyquist", nullptr, &nyquist)) {
					if (onLightPackFocusAreaChanged)
						onLightPackFocusAreaChanged("nyquist");
				}

				ImGui::EndMenu();
			}

			ImGui::Separator();

			smooth = Settings::settings.GetSmooth();
			if (ImGui::SliderInt("Smooth", &smooth, 0, 255)) {
				if (onSmoothChanged)
					onSmoothChanged(smooth);
			}

			gamma = Settings::settings.GetGamma();
			if (ImGui::SliderFloat("Gamma", &gamma, 0.1, 3, "%.2f")) {
				if (onGammaChanged)
					onGammaChanged(gamma);
			}

			ImGui::EndMenu();
		}

		if (auto height = ImGui::GetFrameHeight() * scale; height != this->height) {
			this->height = height;
			context.SetYOffset(height);
		}

		ImGui::EndMenuBar();
		ImGui::End();

		return height;
	}

	void OnDestroy() {
		NFD_Quit();
	}

	const int &GetHeight() const { return height; }

	void SetOnOpen(std::function<void(const std::filesystem::path &)> f) { onOpen = f; }

	void SetOnVisualizationTypeChanged(std::function<void(const std::string &)> f) { onVisualizationTypeChanged = f; }
	void SetOnLightPackVisualizationTypeChanged(std::function<void(const std::string &)> f) { onLightPackVisualizationTypeChanged = f; }
	void SetOnLightPackMappingChanged(std::function<void(const std::string &)> f) { onLightPackMappingChanged = f; }
	void SetOnLightPackFocusAreaChanged(std::function<void(const std::string &)> f) { onLightPackFocusAreaChanged = f; }

	void SetOnBlurChanged(std::function<void(bool)> f) { onBlurChanged = f; }
	void SetOnRotatingChanged(std::function<void(bool)> f) { onRotatingChanged = f; }
	void SetOnDetectBpmChanged(std::function<void(bool)> f) { onDetectBpmChanged = f; }

	void SetOnPulseChanged(std::function<void(bool)> f) { onPulseChanged = f; }
	void SetOnBlurIntensityChanged(std::function<void(float)> f) { onBlurIntensityChanged = f; }
	void SetOnBlurOpacityChanged(std::function<void(float)> f) { onBlurOpacityChanged = f; }
	void SetOnRpmChanged(std::function<void(float)> f) { onRpmChanged = f; }

	void SetOnBufferSizeChanged(std::function<void(int)> f) { onBufferSizeChanged = f; }
	void SetOnDecayTimeChanged(std::function<void(float)> f) { onDecayTimeChanged = f; }
	void SetOnFadeTimeChanged(std::function<void(float)> f) { onFadeTimeChanged = f; }
	void SetOnPulseTimeChanged(std::function<void(float)> f) { onPulseTimeChanged = f; }
	void SetOnStrobeChanged(std::function<void(bool)> f) { onStrobeChanged = f; }
	void SetOnStrobeIntensityChanged(std::function<void(float)> f) { onStrobeIntensityChanged = f; }

	void SetOnRadiusChanged(std::function<void(int)> f) { onRadiusChanged = f; }
	void SetOnLineWidthChanged(std::function<void(float)> f) { onLineWidthChanged = f; }

	void SetOnSmoothChanged(std::function<void(int)> f) { onSmoothChanged = f; }
	void SetOnGammaChanged(std::function<void(float)> f) { onGammaChanged = f; }

	void SetOnPresetChanged(std::function<void(std::optional<std::size_t>)> f) { onPresetChanged = f; }

	void SetOnCurrentSongVisibleChanged(std::function<void(bool)> f) { onCurrentSongVisibleChanged = f; }

	void SetOnColorSelectionChanged(std::function<void(const Settings::ColorSelection &)> f) { onColorSelectionChanged = f; }

	void SetOnFftSizeChanged(std::function<void(int)> f) { onFftSizeChanged = f; }

	void SetOnListeningChanged(std::function<void(bool)> f) { onListeningChanged = f; }
	void SetOnLoopbackChanged(std::function<void(bool)> f) { onLoopbackChanged = f; }

	void SetOnOutputDeviceChanged(std::function<void(const std::string &)> f) { onOutputDeviceChanged = f; }
	void SetOnInputDeviceChanged(std::function<void(const std::string &)> f) { onInputDeviceChanged = f; }

	void SetOnEffectChanged(std::function<void(const std::string &)> f) { onEffectChanged = f; }
	void SetOnEffectIntensityChanged(std::function<void(float)> f) { onEffectIntensityChanged = f; }
	void SetOnEffectXOffsetChanged(std::function<void(float)> f) { onEffectXOffsetChanged = f; }
	void SetOnEffectYOffsetChanged(std::function<void(float)> f) { onEffectYOffsetChanged = f; }
	void SetOnEffectRadiationChanged(std::function<void(float)> f) { onEffectRadiationChanged = f; }

	void SetOnResetWindow(std::function<void()> f) { onResetWindow = f; }

	void SetOnQuit(std::function<void()> f) { onQuit = f; }

private:
	int width = 0, height = 0;

	bool fft = Settings::settings.GetRenderer() == "fft";
	bool fftLine = Settings::settings.GetRenderer() == "fftline";
	bool oscilloscope = Settings::settings.GetRenderer() == "osc";

	bool intensity = Settings::settings.GetLightPackVisualizationType() == "intensity";
	bool color = Settings::settings.GetLightPackVisualizationType() == "color";
	bool colorAndIntensity = Settings::settings.GetLightPackVisualizationType() == "colorintensity";

	bool default = Settings::settings.GetLightPackMapping() == "default";
	bool mine = Settings::settings.GetLightPackMapping() == "mine";
	bool topToBottom = Settings::settings.GetLightPackMapping() == "ttb";
	bool bottomToTop = Settings::settings.GetLightPackMapping() == "btt";

	bool superBass = Settings::settings.GetLightPackFocusArea() == "superbass";
	bool subBass = Settings::settings.GetLightPackFocusArea() == "subbass";
	bool bass = Settings::settings.GetLightPackFocusArea() == "bass";
	bool bassAndMid = Settings::settings.GetLightPackFocusArea() == "bassandmid";
	bool bassMidAndALittleHighEnd = Settings::settings.GetLightPackFocusArea() == "bassmidandalittlehighend";
	bool halfNyquist = Settings::settings.GetLightPackFocusArea() == "halfnyquist";
	bool nyquist = Settings::settings.GetLightPackFocusArea() == "nyquist";

	bool rotate = Settings::settings.GetRotating();
	bool detectBpm = Settings::settings.GetDetectBpm();
	bool blur = Settings::settings.GetBlur();

	float rpm = Settings::settings.GetRotationSpeed() / (360.0f / 60.0f);

	float blurIntensity = Settings::settings.GetBlurIntensity();
	float blurOpacity = Settings::settings.GetBlurOpacity();

	int bufferSize = Settings::settings.GetBufferLength();

	float decayTime = Settings::settings.GetDecayTime().AsSeconds();
	float fadeTime = Settings::settings.GetFadeTime().AsSeconds();

	bool pulse = Settings::settings.GetPulse();
	float pulseTime = Settings::settings.GetPulseTime().AsSeconds();

	bool strobe = Settings::settings.GetStrobe();
	float strobeIntensity = Settings::settings.GetStrobeIntensity();

	int radius = Settings::settings.GetRadius();

	float lineWidth = Settings::settings.GetWidth();

	int smooth = Settings::settings.GetSmooth();
	float gamma = Settings::settings.GetGamma();

	std::optional<std::size_t> presetIndex = Settings::settings.GetPresetIndex();
	std::size_t numPresets = 0;
	bool *presetSelections = nullptr;

	bool newPresetPopup = false;
	int presetX = 0;
	std::string currentPresetName;

	bool currentSongVisible = Settings::settings.GetCurrentSongVisible();

	int fftSize = Settings::settings.GetFftSize();

	bool listening = Settings::settings.GetListening();
	bool loopback = Settings::settings.GetLoopback();

	float effectIntensity = Settings::settings.GetEffectIntensity();
	float effectXOffset = Settings::settings.GetEffectXOffset();
	float effectYOffset = Settings::settings.GetEffectYOffset();
	float effectRadiation = Settings::settings.GetEffectRadiation();

	int minPercentage = Settings::settings.GetColorSelection().minPercentage * 100;
	int minHueSeparation = Settings::settings.GetColorSelection().minHueSeparation;
	float minValueSeparation = Settings::settings.GetColorSelection().minValueSeparation;
	float minimumDistance = Settings::settings.GetColorSelection().minRgbSeparation;
	float minimumSaturation = Settings::settings.GetColorSelection().minSaturation;
	float minimumValue = Settings::settings.GetColorSelection().minValue;

	std::function<void(const std::filesystem::path &)> onOpen;
	std::function<void(bool)> onPulseChanged;
	std::function<void(bool)> onBlurChanged;
	std::function<void(bool)> onRotatingChanged;
	std::function<void(bool)> onDetectBpmChanged;
	std::function<void(const std::string &)> onVisualizationTypeChanged;
	std::function<void(const std::string &)> onLightPackVisualizationTypeChanged;
	std::function<void(const std::string &)> onLightPackMappingChanged;
	std::function<void(const std::string &)> onLightPackFocusAreaChanged;
	std::function<void(float)> onBlurIntensityChanged;
	std::function<void(float)> onBlurOpacityChanged;
	std::function<void(float)> onRpmChanged;
	std::function<void(int)> onBufferSizeChanged;
	std::function<void(float)> onDecayTimeChanged;
	std::function<void(float)> onFadeTimeChanged;
	std::function<void(float)> onPulseTimeChanged;
	std::function<void(bool)> onStrobeChanged;
	std::function<void(float)> onStrobeIntensityChanged;
	std::function<void(int)> onRadiusChanged;
	std::function<void(float)> onLineWidthChanged;
	std::function<void(int)> onSmoothChanged;
	std::function<void(float)> onGammaChanged;
	std::function<void(std::optional<std::size_t>)> onPresetChanged;
	std::function<void(bool)> onCurrentSongVisibleChanged;
	std::function<void(const Settings::ColorSelection &)> onColorSelectionChanged;
	std::function<void(int)> onFftSizeChanged;
	std::function<void(bool)> onListeningChanged;
	std::function<void(bool)> onLoopbackChanged;
	std::function<void(const std::string &)> onOutputDeviceChanged;
	std::function<void(const std::string &)> onInputDeviceChanged;
	std::function<void(const std::string &)> onEffectChanged;
	std::function<void(float)> onEffectIntensityChanged;
	std::function<void(float)> onEffectXOffsetChanged;
	std::function<void(float)> onEffectYOffsetChanged;
	std::function<void(float)> onEffectRadiationChanged;

	std::function<void()> onQuit;
	std::function<void()> onResetWindow;

	float scale = 1.0f;
	ImFont *font = nullptr;
};