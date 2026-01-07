#pragma once

#include <filesystem>

#include <imgui.h>
#include <imgui_stdlib.h>
#include <imgui_freetype.h>
#include <imgui_internal.h>

#include <bass.h>
#ifdef WIN32
#include <basswasapi.h>
#endif

#ifndef __ANDROID__
#include <nfd.hpp>
#endif

#include "Utils/Utils.hpp"

#include "Controls.hpp"
#include "HDR.hpp"
#include "LightPack.hpp"
#include "Playlist.hpp"
#include "Preset.hpp"

using namespace Fetcko;

class Menu : public LoggableClass, public ColorChangeListener {
public:
	Menu(const std::string fontRoot = "KurintoSans") : FontRoot(fontRoot) {
#ifndef __ANDROID__
		NFD_Init();
#endif
	}

	~Menu() {
		delete[] presetSelections;
	}

	void OnResize(int width, int height, float scale, float safeAreaPadding, bool isTouchscreen = false) {
		this->width = this->windowWidth = width;
		this->windowHeight = height;
		this->scale = scale;
		this->isTouchscreen = isTouchscreen;

		if (!font) {
			ImFontConfig fontConfig;

			// Find all fonts that start with FontRoot
			for (const auto &iter : std::filesystem::directory_iterator(Utils::GetResourceFolder())) {
				const auto stem = iter.path().stem().u8string();

				if (stem.find(FontRoot) == 0 &&
					iter.path().extension().u8string() == ".ttf") {
					if (stem.find("JP") != std::string::npos)
						ImGui::GetIO().Fonts->AddFontFromFileTTF(iter.path().u8string().c_str(), 0.0f, &fontConfig, ImGui::GetIO().Fonts->GetGlyphRangesJapanese());
					else if (stem.find("KR"))
						ImGui::GetIO().Fonts->AddFontFromFileTTF(iter.path().u8string().c_str(), 0.0f, &fontConfig, ImGui::GetIO().Fonts->GetGlyphRangesKorean());
					else if (stem.find(FontRoot + "-Rg") == 0)
						font = ImGui::GetIO().Fonts->AddFontFromFileTTF(iter.path().u8string().c_str(), 0.0f, &fontConfig);

					fontConfig.MergeMode = true;
				}
			}
		}

		if (!originalStyle)
			originalStyle = ImGui::GetStyle();

		if (scale != lastScale) {
			// https://github.com/ocornut/imgui/issues/5452
			ImGui::GetStyle() = *originalStyle;
			ImGui::GetStyle().ScaleAllSizes(scale);

			ImGui::GetStyle().FontScaleMain = scale;

			lastScale = scale;
		}

		// Make scrollbars larger on touchscreens
		if (isTouchscreen)
			ImGui::GetStyle().ScrollbarSize = originalStyle->ScrollbarSize * scale * 2.5f;

		this->safeAreaPadding = safeAreaPadding;

		ImGui::GetStyle().Colors[ImGuiCol_PopupBg].w = 1.0f;
		ImGui::GetStyle().Colors[ImGuiCol_FrameBg].w = 1.0f;
		ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	void OnColorChanged(const MathsCPP::Colour<float> &color, bool silent = false) override {
		ImGui::GetStyle().Colors[ImGuiCol_Button] = { color.r, color.g, color.b, 0.75f };
		ImGui::GetStyle().Colors[ImGuiCol_ButtonActive] = { color.r, color.g, color.b, color.a };

		auto hsv = color.ToHsv();
		hsv.v = 1.0f;
		auto brightColor = Colour<float>::FromHsv(hsv);

		ImGui::GetStyle().Colors[ImGuiCol_SliderGrab] = { brightColor.r, brightColor.g, brightColor.b, brightColor.a };
		ImGui::GetStyle().Colors[ImGuiCol_SliderGrabActive] = { brightColor.r, brightColor.g, brightColor.b, brightColor.a };
		ImGui::GetStyle().Colors[ImGuiCol_FrameBgHovered] = { brightColor.r, brightColor.g, brightColor.b, brightColor.a - 0.25f };
		ImGui::GetStyle().Colors[ImGuiCol_CheckMark] = { brightColor.r, brightColor.g, brightColor.b, brightColor.a };
		ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered] = { brightColor.r, brightColor.g, brightColor.b, brightColor.a };

		hsv.v = 0.5f;
		auto darkColor = Colour<float>::FromHsv(hsv);

		ImGui::GetStyle().Colors[ImGuiCol_FrameBgActive] = { darkColor.r, darkColor.g, darkColor.b, darkColor.a - 0.25f };
		ImGui::GetStyle().Colors[ImGuiCol_Header] = { darkColor.r, darkColor.g, darkColor.b, darkColor.a };
		ImGui::GetStyle().Colors[ImGuiCol_FrameBg] = { darkColor.r, darkColor.g, darkColor.b, darkColor.a };
		ImGui::GetStyle().Colors[ImGuiCol_TitleBgActive] = { darkColor.r, darkColor.g, darkColor.b, darkColor.a };

		hsv = color.ToHsv();
		if (hsv.v > 0.66f && darkenPulseOnBrightColors)
			ImGui::GetStyle().Colors[ImGuiCol_HeaderHovered] = { darkColor.r, darkColor.g, darkColor.b, darkColor.a };
		else
			ImGui::GetStyle().Colors[ImGuiCol_HeaderHovered] = { color.r, color.g, color.b, color.a };

		colorChanged = true;
	}

	bool HasColorChanged() {
		auto ret = colorChanged;
		colorChanged = false;
		return ret;
	}

	bool OnLoop(const LightPack &lightPack, AlbumArt &albumArt, Context &context) {
		bool open = false;

		//ImGui::ShowStyleEditor();

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

		ImGui::SetWindowPos({ 0.0f, safeAreaPadding });
		ImGui::SetWindowSize({ static_cast<float>(width), 0 });

		ImGui::BeginMenuBar();

		const auto maxHeight = windowHeight - height - Controls::SeekbarSize * 2 - safeAreaPadding;
		const auto maxWidth = windowWidth - ImGui::GetStyle().ScrollbarSize;

		ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(maxWidth, maxHeight));
		if (ImGui::BeginMenu("File")) {
			open = true;

#ifndef __ANDROID__
			if (ImGui::MenuItem("Open File", "Ctrl-O", false, true)) {
				nfdnchar_t *outPath;

#ifdef WIN32
				std::wstring extensions;
#else
				std::string extensions;
#endif
				for (const auto &[i, extension] : Utils::Enumerate(Playlist::GetSupportedExtensions())) {
					const auto noDot = extension.substr(1);
					extensions += 
#ifdef WIN32
						std::wstring(noDot.begin(), noDot.end()) + (i == Playlist::GetSupportedExtensions().size() - 1 ? L"" : L",");
#else
						std::string(noDot.begin(), noDot.end()) + (i == Playlist::GetSupportedExtensions().size() - 1 ? "" : ",");
#endif
				}

#ifdef WIN32
				nfdnfilteritem_t filters[2] = { { L"Music", extensions.c_str() }, { L"Cue", L"cue" }};
#else
				nfdnfilteritem_t filters[2] = { { "Music", extensions.c_str() }, { "Cue", "cue" }};
#endif
				nfdopendialognargs_t args = { 0 };
				args.filterList = filters;
				args.filterCount = 2;
				nfdresult_t result = NFD_OpenDialogN_With(&outPath, &args);
				if (result == NFD_OKAY) {
					if (onOpen) onOpen(outPath);
					NFD_FreePathN(outPath);
				}	
			}
#endif

			if (ImGui::MenuItem("Open Folder", "Ctrl-Shift-O", false, true)) {
#ifndef __ANDROID__
				nfdnchar_t *outPath;
				nfdresult_t result = NFD_PickFolderN(&outPath, nullptr);
				if (result == NFD_OKAY) {
					if (onOpen) onOpen(outPath);
					NFD_FreePathN(outPath);
				}
#else
				if (fileOpenFunc)
					fileOpenFunc();
#endif
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Quit", "Alt-F4", false, true)) {
				if (onQuit)
					onQuit();
			}

			ImGui::EndMenu();
		}

		randomize = Settings::settings.GetRandomize();
		randomizePresets = Settings::settings.GetRandomizePresets();
		randomizePresetsByBeats = Settings::settings.GetRandomizePresetsByBeats();

		ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(maxWidth, maxHeight));
		if (ImGui::BeginMenu("Visualizer")) {
			open = true;

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

			if (fftLine) {
				rendererOffset = Settings::settings.GetRendererOffset();
				if (ImGui::SliderInt("Visualization offset (%)", &rendererOffset, -100, 100)) {
					if (onRendererOffsetChanged)
						onRendererOffsetChanged(rendererOffset);
				}
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

			visualizerScale = Settings::settings.GetScale();
			if (ImGui::SliderFloat("Scale", &visualizerScale, 0.01, 5.0, "%.2f")) {
				if (onScaleChanged)
					onScaleChanged(visualizerScale);
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

			if (HDR::Enabled && pulse) {
				pulseMaxBrightness = Settings::settings.GetPulseMaxBrightness();
				if (ImGui::MenuItem("\tPulse at max HDR brightness?", nullptr, &pulseMaxBrightness)) {
					if (onPulseMaxBrightnessChanged)
						onPulseMaxBrightnessChanged(pulseMaxBrightness);
				}
			}

			pulseBackground = Settings::settings.GetPulseBackground();
			if (ImGui::MenuItem("Pulse background", nullptr, &pulseBackground)) {
				if (onPulseBackgroundChanged)
					onPulseBackgroundChanged(pulseBackground);
			}
			//ImGui::EndDisabled();

			darkenPulseOnBrightColors = Settings::settings.GetDarkenPulseOnBrightColors();
			if (ImGui::MenuItem("Darken pulse on brighter colors?", nullptr, &darkenPulseOnBrightColors)) {
				if (onDarkenPulseOnBrightColorsChanged)
					onDarkenPulseOnBrightColorsChanged(darkenPulseOnBrightColors);
			}

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

			if (ImGui::MenuItem("Reset rotation...")) {
				if (onResetRotation)
					onResetRotation();
			}

			ImGui::Separator();

			detectBpm = Settings::settings.GetDetectBpm();
			if (ImGui::MenuItem("Detect BPM", nullptr, &detectBpm)) {
				if (onDetectBpmChanged)
					onDetectBpmChanged(!Settings::settings.GetDetectBpm());
			}

			cacheDetectionResults = Settings::settings.GetCacheDetectionResults();
			if (ImGui::MenuItem("Cache detection results?", nullptr, &cacheDetectionResults)) {
				Settings::settings.SetCacheDetectionResults(cacheDetectionResults);
			}

			if (ImGui::MenuItem("Clear detection cache")) {
				std::error_code ec;
				if (std::filesystem::remove_all(Settings::GetPath("cache"), ec) == static_cast<std::uintmax_t>(-1))
					LogError("Could not clear detection cache! ", ec.message());
			}

			// Only update our cache info every second
			if (auto now = std::chrono::system_clock::now(); Duration<Microseconds>(now - lastFrame).AsSeconds() > 1.0 && std::filesystem::exists(Settings::GetPath("cache"))) {
				auto dirIter = std::filesystem::directory_iterator(Settings::GetPath("cache"));
				std::size_t bytes = 0;

				cacheFileCount = std::count_if(
					begin(dirIter),
					end(dirIter),
					[&](auto &entry) {
						if (entry.is_regular_file()) {
							bytes += std::filesystem::file_size(entry.path());
							return true;
						}
						return false;
					}
				);

				cacheSize = Utils::GetFriendlyBytes(bytes);

				lastFrame = now;
			}

			ImGui::Text("\tCached songs: %ld", cacheFileCount);
			ImGui::Text("\tCache size: %s", cacheSize.c_str());

			halveBpm = Settings::settings.GetHalveBpm();
			if (ImGui::MenuItem("Halve detected BPM?", nullptr, &halveBpm)) {
				if (onHalveBpmChanged)
					onHalveBpmChanged(halveBpm);
			}

			ImGui::Separator();

			blur = Settings::settings.GetBlur();
			if (ImGui::MenuItem("Blur", nullptr, &blur)) {
				if (onBlurChanged)
					onBlurChanged(!Settings::settings.GetBlur());
			}

			sourceFactor = Settings::settings.GetSourceFactor();
			if (ImGui::BeginMenu("Blending source factor")) {
				for (const auto &[factor, name] : Settings::BlendModes) {
					bool selected = sourceFactor == factor;

					if (ImGui::MenuItem(name.c_str(), nullptr, &selected)) {
						if (onSourceFactorChanged)
							onSourceFactorChanged(factor);
					}
				}
				ImGui::EndMenu();
			}

			destFactor = Settings::settings.GetDestFactor();
			if (ImGui::BeginMenu("Blending destination factor")) {
				for (const auto &[factor, name] : Settings::BlendModes) {
					bool selected = destFactor == factor;

					if (ImGui::MenuItem(name.c_str(), nullptr, &selected)) {
						if (onDestFactorChanged)
							onDestFactorChanged(factor);
					}
				}
				ImGui::EndMenu();
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

			ImGui::BeginDisabled(!blur);
			if (ImGui::MenuItem("Reset blur...")) {
				if (onClearBlurFbo)
					onClearBlurFbo();
			}
			ImGui::EndDisabled();

			ImGui::Separator();

			if (ImGui::BeginMenu("Effect")) {
				auto files = Utils::GetFiles(Utils::GetResourceFolder() / "Effects");

				std::vector<std::pair<std::string, std::string>> effectNames;
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

					// Make sure "No Effect" is at index 0
					if (effectName == "noeffect")
						effectNames.emplace(effectNames.begin(), std::make_pair(effectName, friendlyName));
					else
						effectNames.emplace_back(std::make_pair(effectName, friendlyName));
				}

				for (const auto &[effectName, friendlyName] : effectNames) {
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

				effectHorizontalSpread = Settings::settings.GetEffectHorizontalSpread();
				if (ImGui::SliderFloat("Horizontal Spread", &effectHorizontalSpread, -10.0f, 10.0f, "%.2f")) {
					if (onEffectHorizontalSpreadChanged)
						onEffectHorizontalSpreadChanged(effectHorizontalSpread);
				}

				effectVerticalSpread = Settings::settings.GetEffectVerticalSpread();
				if (ImGui::SliderFloat("Vertical Spread", &effectVerticalSpread, -10.0f, 10.0f, "%.2f")) {
					if (onEffectVerticalSpreadChanged)
						onEffectVerticalSpreadChanged(effectVerticalSpread);
				}

				effectRotation = Settings::settings.GetEffectRotation();
				if (ImGui::SliderFloat("Rotation", &effectRotation, -5.0f, 5.0f, "%.2f")) {
					if (onEffectRotationChanged)
						onEffectRotationChanged(effectRotation);
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

			frameLimit = Settings::settings.GetFrameLimit();
			
			bool limitFramerate = Settings::settings.GetLimitFramerate();
			if (ImGui::MenuItem("Limit framerate?", nullptr, &limitFramerate)) {
				if (onLimitFramerateChanged)
					onLimitFramerateChanged(limitFramerate);
			}
			ImGui::BeginDisabled(!limitFramerate);
			if (ImGui::SliderInt("Limit", &frameLimit, 5, 360)) {
				if (onFrameLimitChanged)
					onFrameLimitChanged(frameLimit);
			}
			ImGui::EndDisabled();

			ImGui::Separator();

			if (ImGui::MenuItem("Randomize")) {
				if (onRandom)
					onRandom();
			}

			ImGui::BeginDisabled(randomizePresets || randomizePresetsByBeats);
			if (ImGui::MenuItem("Randomize every...", nullptr, &randomize)) {
				if (onRandomizeChanged)
					onRandomizeChanged(randomize);
			}
			ImGui::EndDisabled();

			ImGui::BeginDisabled(!randomize || randomizePresets || randomizePresetsByBeats);
			randomizeTime = Settings::settings.GetRandomizeTime().AsSeconds();
			if (ImGui::SliderFloat("...seconds", &randomizeTime, 0.5, 10.0, "%.2f")) {
				if (onRandomizeTimeChanged)
					onRandomizeTimeChanged(randomizeTime);
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
		ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(maxWidth, maxHeight));
		if (ImGui::BeginMenu("Presets")) {
			open = true;

			presetX = ImGui::GetWindowPos().x;
			auto presets = Preset::GetPresets();
			if (presets.size() != numPresets) {
				delete[] presetSelections;
				presetSelections = new bool[presets.size()];
				numPresets = presets.size();
			}

			selectedPresets = Settings::settings.GetSelectedPresets();
			for (const auto &[i, preset] : Utils::Enumerate(presets)) {
				presetSelections[i] = presetIndex && *presetIndex == i;

				auto name = preset.GetName();
				if (selectedPresets.find(i) != selectedPresets.end())
					name = "- " + name;

				if (ImGui::MenuItem(name.c_str(), nullptr, &presetSelections[i])) {
					if (onPresetChanged)
						onPresetChanged(i);
				}

				if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
					if (selectedPresets.find(i) != selectedPresets.end())
						selectedPresets.erase(i);
					else
						selectedPresets.emplace(i);

					if (onSelectedPresetsChanged)
						onSelectedPresetsChanged(selectedPresets);
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

						// If it was a selected preset, remove it
						if (auto iter = selectedPresets.find(i); iter != selectedPresets.end())
							selectedPresets.erase(i);

						// Drop any selected presets after it down one index
						for (auto iter = selectedPresets.begin(); iter != selectedPresets.end();) {
							if (*iter > i) {
								auto temp = *iter;
								selectedPresets.erase(iter);
								iter = selectedPresets.emplace(temp - 1).first;
								std::advance(iter, 1);
							} else ++iter;
						}
						if (onSelectedPresetsChanged)
							onSelectedPresetsChanged(selectedPresets);
					}
					ImGui::EndPopup();
				}
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Save current settings as preset...")) {
				currentPresetName.clear();
				newPresetPopup = true;
			}

			ImGui::Separator();

			ImGui::BeginDisabled(randomize);
			if (ImGui::MenuItem("Randomize selected (middle-click) every...", nullptr, &randomizePresets, !randomizePresetsByBeats)) {
				if (onRandomizePresetsChanged)
					onRandomizePresetsChanged(randomizePresets);
			}
			ImGui::EndDisabled();

			ImGui::BeginDisabled(!randomizePresets || randomize);
			randomizePresetsTime = Settings::settings.GetRandomizePresetsTime().AsSeconds();
			if (ImGui::SliderFloat("...seconds", &randomizePresetsTime, 0.5, 10.0, "%.2f")) {
				if (onRandomizePresetsTimeChanged)
					onRandomizePresetsTimeChanged(randomizePresetsTime);
			}
			ImGui::EndDisabled();

			ImGui::BeginDisabled(randomize);
			if (ImGui::MenuItem("Randomize selected (middle-click) every...##", nullptr, &randomizePresetsByBeats, !randomizePresets)) {
				if (onRandomizePresetsByBeatsChanged)
					onRandomizePresetsByBeatsChanged(randomizePresetsByBeats);
			}
			ImGui::EndDisabled();

			ImGui::BeginDisabled(!randomizePresetsByBeats || randomize);

			randomizePresetsBeats = Settings::settings.GetRandomizePresetsBeats();

			// As power-of-two sliders are not yet supported in ImGui,
			// this is a bit of a hack.
			//
			// https://github.com/ocornut/imgui/issues/1815
			if (ImGui::SliderInt("...beats", &randomizePresetsBeats, 1, 1024, std::to_string(randomizePresetsBeats).c_str(), ImGuiSliderFlags_Logarithmic)) {
				randomizePresetsBeats--;
				randomizePresetsBeats |= randomizePresetsBeats >> 1;
				randomizePresetsBeats |= randomizePresetsBeats >> 2;
				randomizePresetsBeats |= randomizePresetsBeats >> 4;
				randomizePresetsBeats |= randomizePresetsBeats >> 8;
				randomizePresetsBeats |= randomizePresetsBeats >> 16;
				randomizePresetsBeats++;

				if (onRandomizePresetsBeatsChanged)
					onRandomizePresetsBeatsChanged(randomizePresetsBeats);
			}

			ImGui::EndDisabled();

			ImGui::EndMenu();
		}

		// See https://github.com/ocornut/imgui/issues/5684#issuecomment-1247928651
		if (newPresetPopup) {
			open = true;

			ImGui::SetNextWindowPos(ImVec2(presetX, context.GetYOffset()));
			ImGui::OpenPopup("Enter preset name...");

			bool open = true;
			if (ImGui::BeginPopupModal("Enter preset name...", &open, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize)) {
				
				ImGui::InputText("Preset name", &currentPresetName);

				saveRenderer = Settings::settings.GetSaveRenderer();
				if (ImGui::Checkbox("Save visualization type?", &saveRenderer))
					Settings::settings.SetSaveRenderer(saveRenderer);

				saveScale = Settings::settings.GetSaveScale();
				if (ImGui::Checkbox("Save scale?", &saveScale))
					Settings::settings.SetSaveScale(saveScale);

				if (ImGui::Button("Cancel")) {
					saveRenderer = lastSaveRenderer;
					Settings::settings.SetSaveRenderer(lastSaveRenderer);

					saveScale = lastSaveScale;
					Settings::settings.SetSaveScale(lastSaveScale);

					open = false;
				}

				ImGui::SameLine();

				if (ImGui::Button("Save")) {
					Preset preset(
						currentPresetName,
						Settings::settings.GetBufferLength(),
						Settings::settings.GetDecayTime(),
						Settings::settings.GetFadeTime(),
						Settings::settings.GetPulse(),
						Settings::settings.GetPulseBackground(),
						Settings::settings.GetPulseTime(),
						Settings::settings.GetStrobe(),
						Settings::settings.GetStrobeIntensity(),
						Settings::settings.GetRotating(),
						Settings::settings.GetRotationSpeed(),
						Settings::settings.GetBlur(),
						Settings::settings.GetSourceFactor(),
						Settings::settings.GetDestFactor(),
						Settings::settings.GetBlurIntensity(),
						Settings::settings.GetBlurOpacity(),
						Settings::settings.GetEffect(),
						Settings::settings.GetEffectIntensity(),
						Settings::settings.GetEffectXOffset(),
						Settings::settings.GetEffectYOffset(),
						Settings::settings.GetEffectRadiation(),
						Settings::settings.GetEffectHorizontalSpread(),
						Settings::settings.GetEffectVerticalSpread(),
						Settings::settings.GetEffectRotation(),
						saveRenderer ? Settings::settings.GetRenderer() : static_cast<std::optional<std::string>>(std::nullopt),
						saveScale ? Settings::settings.GetScale() : static_cast<std::optional<float>>(std::nullopt),
						fftLine ? Settings::settings.GetRendererOffset() : static_cast<std::optional<int>>(std::nullopt)
					);

					Preset::AddPreset(std::move(preset));

					if (onPresetChanged)
						onPresetChanged(Preset::GetPresets().size() - 1);

					lastSaveRenderer = saveRenderer;
					lastSaveScale = saveScale;

					open = false;
				}

				ImGui::EndPopup();
			}

			newPresetPopup = open;
		}

		ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(maxWidth, maxHeight));
		if (ImGui::BeginMenu(isTouchscreen ? "UI" : "Interface")) {
			open = true;

			autoFade = Settings::settings.GetAutoFade();
			waitTime = Settings::settings.GetWaitTime().AsSeconds();
			autoFadeSpeed = Settings::settings.GetAutoFadeSpeed();

			pulseUi = Settings::settings.GetPulseUi();
			if (ImGui::MenuItem("Pulse?", nullptr, &pulseUi)) {
				if (onPulseUiChanged)
					onPulseUiChanged(pulseUi);
			}

			if (ImGui::MenuItem("Autofade?", nullptr, &autoFade)) {
				if (onAutoFadeChanged)
					onAutoFadeChanged(autoFade);
			}

			if (ImGui::SliderFloat("Autofade wait time", &waitTime, 1.0, 10.0, "%.2f")) {
				if (onWaitTimeChanged)
					onWaitTimeChanged(waitTime);
			}
			if (ImGui::SliderFloat("Autofade speed", &autoFadeSpeed, 0.5, 5.0, "%.2f")) {
				if (onAutoFadeSpeedChanged)
					onAutoFadeSpeedChanged(autoFadeSpeed);
			}

			if (HDR::Enabled) {
				ImGui::Separator();

				uiGamma = Settings::settings.GetUiGamma();
				if (ImGui::SliderFloat("Gamma", &uiGamma, 0.1f, 3.0f, "%.2f")) {
					if (onUiGammaChanged)
						onUiGammaChanged(uiGamma);
				}

				uiContrast = Settings::settings.GetUiContrast();
				if (ImGui::SliderFloat("Contrast", &uiContrast, 0.1f, 3.0f, "%.2f")) {
					if (onUiContrastChanged)
						onUiContrastChanged(uiContrast);
				}

				uiBrightness = Settings::settings.GetUiBrightness();
				if (ImGui::SliderFloat("Brightness", &uiBrightness, -HDR::WhiteLevel * HDR::Headroom, HDR::WhiteLevel * HDR::Headroom, "%.2f")) {
					if (onUiBrightnessChanged)
						onUiBrightnessChanged(uiBrightness);
				}

				hdrWhitePoint = Settings::settings.GetHdrWhitePoint();
				if (hdrWhitePoint) {
					ImGui::Separator();
					if (ImGui::SliderFloat("White Point", &*hdrWhitePoint, 0.1f, HDR::WhiteLevel * HDR::Headroom, "%.2f")) {
						if (onHdrWhitePointChanged)
							onHdrWhitePointChanged(hdrWhitePoint);
					}
				}
			}

			ImGui::EndMenu();
		}

		ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(maxWidth, maxHeight));
		if (ImGui::BeginMenu("Playlist")) {
			open = true;

			currentSongVisible = Settings::settings.GetCurrentSongVisible();
			if (ImGui::MenuItem("Current track always visible?", nullptr, &currentSongVisible)) {
				if (onCurrentSongVisibleChanged)
					onCurrentSongVisibleChanged(currentSongVisible);
			}
			ImGui::EndMenu();
		}

		ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(maxWidth, maxHeight));
		if (ImGui::BeginMenu(isTouchscreen ? "Art" : "Album Art")) {
			open = true;

			bool none = !albumArt.Loaded() || albumArt.IsHidden();
			if (ImGui::MenuItem("None", nullptr, &none))
				albumArt.SetHidden(true);

			if (albumArt.HasEmbedded()) {
				bool embedded = albumArt.GetCurrentFile().empty() && !albumArt.IsHidden();
				if (ImGui::MenuItem("Embedded", nullptr, &embedded)) {
					albumArt.SetHidden(false);
					albumArt.LoadEmbedded();
					albumArt.Scale();
				}
			}

			for (const auto &art : albumArt.GetPreferred()) {
				auto name = art.second.u8string().substr(albumArt.GetSearchFolder().u8string().size() + 1);
				std::replace(name.begin(), name.end(), '\\', '/');

				bool selected = albumArt.GetCurrentFile() == art.second && !albumArt.IsHidden();
				if (ImGui::MenuItem(name.c_str(), nullptr, &selected)) {
					albumArt.SetHidden(false);
					albumArt.Load(art.second, art.second.parent_path(), true);
					albumArt.Scale();
				}
			}
			for (const auto &art : albumArt.GetFound()) {
				auto name = art.u8string().substr(albumArt.GetSearchFolder().u8string().size() + 1);
				std::replace(name.begin(), name.end(), '\\', '/');

				bool selected = albumArt.GetCurrentFile() == art && !albumArt.IsHidden();
				if (ImGui::MenuItem(name.c_str(), nullptr, &selected)) {
					albumArt.SetHidden(false);
					albumArt.Load(art, art.parent_path(), true);
					albumArt.Scale();
				}
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Rescan album art...")) {
				if (onRescanAlbumArt)
					onRescanAlbumArt();
			}

			if (HDR::Enabled) {
				ImGui::Separator();

				if (ImGui::BeginMenu("LUT")) {
					lut = Settings::settings.GetLut();

					auto files = Utils::GetFiles(Utils::GetResourceFolder() / "LUTs");

					for (auto &file : files) {
						bool selected = file.filename().u8string() == lut;

						if (ImGui::MenuItem(file.filename().u8string().c_str(), nullptr, &selected)) {
							if (onLutChanged)
								onLutChanged(file.filename().u8string());
						}
					}

					ImGui::EndMenu();
				}

				albumArtGamma = Settings::settings.GetAlbumArtGamma();
				if (ImGui::SliderFloat("Gamma", &albumArtGamma, 0.1f, 3.0f, "%.2f")) {
					if (onAlbumArtGammaChanged)
						onAlbumArtGammaChanged(albumArtGamma);
				}
				albumArtContrast = Settings::settings.GetAlbumArtContrast();
				if (ImGui::SliderFloat("Contrast", &albumArtContrast, 0.1f, 3.0f, "%.2f")) {
					if (onAlbumArtContrastChanged)
						onAlbumArtContrastChanged(albumArtContrast);
				}
				albumArtBrightness = Settings::settings.GetAlbumArtBrightness();
				if (ImGui::SliderFloat("Brightness", &albumArtBrightness, -HDR::WhiteLevel * HDR::Headroom, HDR::WhiteLevel * HDR::Headroom, "%.2f")) {
					if (onAlbumArtBrightnessChanged)
						onAlbumArtBrightnessChanged(albumArtBrightness);
				}

				if (ImGui::MenuItem("Reset to default...")) {
					if (onLutChanged)
						onLutChanged("BT709_to_HLG.cube");
					if (onAlbumArtGammaChanged)
						onAlbumArtGammaChanged(0.5f);
					if (onAlbumArtContrastChanged)
						onAlbumArtContrastChanged(1.25f);
					if (onAlbumArtBrightnessChanged)
						onAlbumArtBrightnessChanged(1.50f);
				}
			}

			ImGui::EndMenu();
		}

		ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(maxWidth, maxHeight));
		if (ImGui::BeginMenu(isTouchscreen ? "Colors" : "Color Selection")) {
			open = true;

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

			ImGui::SeparatorText("Black & white detection");

			maxAverageColorVariance = Settings::settings.GetColorSelection().maxAverageColorVariance;
			if (ImGui::SliderFloat("Maximum (average) color variation", &maxAverageColorVariance, 0.5, 4.0f, "%.2f")) {
				if (onColorSelectionChanged) {
					auto newColorSelection = Settings::settings.GetColorSelection();
					newColorSelection.maxAverageColorVariance = maxAverageColorVariance;
					onColorSelectionChanged(newColorSelection);
				}
			}

			maxPerPixelColorVariance = Settings::settings.GetColorSelection().maxPerPixelColorVariance;
			if (ImGui::SliderFloat("Maximum (per-pixel) color variation", &maxPerPixelColorVariance, 0.25f, 3.0f, "%.2f")) {
				if (onColorSelectionChanged) {
					auto newColorSelection = Settings::settings.GetColorSelection();
					newColorSelection.maxPerPixelColorVariance = maxPerPixelColorVariance;
					onColorSelectionChanged(newColorSelection);
				}
			}

			ImGui::Separator();

			if (albumArt.IsBlackAndWhite())
				ImGui::Text("Black and white:");
			else
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

		ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(maxWidth, maxHeight));
#ifndef __ANDROID__
		if (ImGui::BeginMenu("Device")) {
			open = true;

			if (ImGui::BeginMenu("Input device")) {
#ifdef WIN32
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
#endif
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

			ImGui::Separator();

			exclusive = Settings::settings.GetExclsuive();
			if (ImGui::MenuItem("Exclusive output?", nullptr, &exclusive)) {
				if (onExclusiveChanged)
					onExclusiveChanged(exclusive);
			}

			exclusiveVolume = Settings::settings.GetVolume() * 100;
			if (ImGui::SliderInt("Exclusive volume", &exclusiveVolume, 0, 100)) {
				if (onExclusiveVolumeChanged)
					onExclusiveVolumeChanged(exclusiveVolume);
			}

			ImGui::EndMenu();
		}
#else
		if (HDR::Capable) {
			if (ImGui::BeginMenu("Display")) {
				open = true;
				
				hdr = Settings::settings.GetHdr();
				if (ImGui::MenuItem("HDR?", nullptr, &hdr)) {
					if (onHdrChanged)
						onHdrChanged(hdr);
				}

				if (!hdr) ImGui::BeginDisabled();
				ImGui::SeparatorText("Colorspace");
				colorspace = Settings::settings.GetColorspace();
				for (const auto &option: colorspaces) {
					bool selected = option == colorspace;
					if (ImGui::MenuItem(option.c_str(), nullptr, &selected)) {
						if (onColorspaceChanged)
							onColorspaceChanged(option);
					}
				}
				if (!hdr) ImGui::EndDisabled();

				ImGui::EndMenu();
			}
		}
#endif

		ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(maxWidth, maxHeight));
		if (!isTouchscreen && ImGui::BeginMenu("LightPack", lightPack.IsActive())) {
			open = true;

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
				defaultMapping = Settings::settings.GetLightPackMapping() == "default";
				mine = Settings::settings.GetLightPackMapping() == "mine";
				topToBottom = Settings::settings.GetLightPackMapping() == "ttb";
				bottomToTop = Settings::settings.GetLightPackMapping() == "btt";

				if (ImGui::MenuItem("Default", nullptr, &defaultMapping)) {
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

		if (auto height = ImGui::GetFrameHeight(); height + safeAreaPadding != context.GetYOffset()) {
			this->height = height;
			context.SetYOffset(safeAreaPadding + height);
		}

		ImGui::EndMenuBar();
		ImGui::End();

		return open;
	}

	void OnDestroy() {
#ifndef __ANDROID__
		NFD_Quit();
#endif
	}

	const int &GetHeight() const { return height; }

	const bool &IsPresetPopupVisible() const { return newPresetPopup; }

	void SetOnOpen(std::function<void(const std::filesystem::path &)> f) { onOpen = f; }

	void SetOnVisualizationTypeChanged(std::function<void(const std::string &)> f) { onVisualizationTypeChanged = f; }
	void SetOnLightPackVisualizationTypeChanged(std::function<void(const std::string &)> f) { onLightPackVisualizationTypeChanged = f; }
	void SetOnLightPackMappingChanged(std::function<void(const std::string &)> f) { onLightPackMappingChanged = f; }
	void SetOnLightPackFocusAreaChanged(std::function<void(const std::string &)> f) { onLightPackFocusAreaChanged = f; }

	void SetOnBlurChanged(std::function<void(bool)> f) { onBlurChanged = f; }
	void SetOnSourceFactorChanged(std::function<void(GLenum)> f) { onSourceFactorChanged = f; }
	void SetOnDestFactorChanged(std::function<void(GLenum)> f) { onDestFactorChanged = f; }
	void SetOnRotatingChanged(std::function<void(bool)> f) { onRotatingChanged = f; }
	void SetOnDetectBpmChanged(std::function<void(bool)> f) { onDetectBpmChanged = f; }
	void SetOnHalveBpmChanged(std::function<void(bool)> f) { onHalveBpmChanged = f; }

	void SetOnPulseChanged(std::function<void(bool)> f) { onPulseChanged = f; }
	void SetOnPulseBackgroundChanged(std::function<void(bool)> f) { onPulseBackgroundChanged = f; }
	void SetOnDarkenPulseOnBrightColorsChanged(std::function<void(bool)> f) { onDarkenPulseOnBrightColorsChanged = f; }
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
	void SetOnEffectHorizontalSpreadChanged(std::function<void(float)> f) { onEffectHorizontalSpreadChanged = f; }
	void SetOnEffectVerticalSpreadChanged(std::function<void(float)> f) { onEffectVerticalSpreadChanged = f; }
	void SetOnEffectRotationChanged(std::function<void(float)> f) { onEffectRotationChanged = f; }

	void SetOnLimitFramerateChanged(std::function<void(bool)> f) { onLimitFramerateChanged = f; }
	void SetOnFrameLimitChanged(std::function<void(int)> f) { onFrameLimitChanged = f; }

	void SetOnRandomizeChanged(std::function<void(bool)> f) { onRandomizeChanged = f; }
	void SetOnRandomizeTimeChanged(std::function<void(float)> f) { onRandomizeTimeChanged = f; }

	void SetOnScaleChanged(std::function<void(float)> f) { onScaleChanged = f; }

	void SetOnSelectedPresetsChanged(std::function<void(const std::set<std::size_t> &)> f) { onSelectedPresetsChanged = f; }
	void SetOnRandomizePresetsChanged(std::function<void(bool)> f) { onRandomizePresetsChanged = f; }
	void SetOnRandomizePresetsTimeChanged(std::function<void(float)> f) { onRandomizePresetsTimeChanged = f; }
	void SetOnRandomizePresetsByBeatChanged(std::function<void(bool)> f) { onRandomizePresetsByBeatsChanged = f; }
	void SetOnRandomizePresetsBeatsChanged(std::function<void(int)> f) { onRandomizePresetsBeatsChanged = f; }

	void SetOnResetRotation(std::function<void()> f) { onResetRotation = f; }
	void SetOnClearBlurFbo(std::function<void()> f) { onClearBlurFbo = f; }

	void SetOnResetWindow(std::function<void()> f) { onResetWindow = f; }

	void SetOnQuit(std::function<void()> f) { onQuit = f; }

	void SetOnRandom(std::function<void()> f) { onRandom = f; }

	void SetOnAutoFadeChanged(std::function<void(bool)> f) { onAutoFadeChanged = f; }
	void SetOnWaitTimeChanged(std::function<void(float)> f) { onWaitTimeChanged = f; }
	void SetOnAutoFadeSpeedChanged(std::function<void(float)> f) { onAutoFadeSpeedChanged = f; }

	void SetOnExclusiveChanged(std::function<void(bool)> f) { onExclusiveChanged = f; }
	void SetOnExclusiveVolumeChanged(std::function<void(int)> f) { onExclusiveVolumeChanged = f; }

	void SetOnRendererOffsetChanged(std::function<void(int)> f) { onRendererOffsetChanged = f; }

	void SetOnLutChanged(std::function<void(const std::string &)> f) { onLutChanged = f; }
	void SetOnAlbumArtGammaChanged(std::function<void(float)> f) { onAlbumArtGammaChanged = f; }
	void SetOnAlbumArtContrastChanged(std::function<void(float)> f) { onAlbumArtContrastChanged = f; }
	void SetOnAlbumArtBrightnessChanged(std::function<void(float)> f) { onAlbumArtBrightnessChanged = f; }

	void SetOnHdrWhitePointChanged(std::function<void(std::optional<float>)> f) { onHdrWhitePointChanged = f; }

	void SetOnRescanAlbumArt(std::function<void()> f) { onRescanAlbumArt = f; }

	void SetOnPulseUiChanged(std::function<void(bool)> f) { onPulseUiChanged = f; }
	void SetOnUiGammaChanged(std::function<void(float)> f) { onUiGammaChanged = f; }
	void SetOnUiContrastChanged(std::function<void(float)> f) { onUiContrastChanged = f; }
	void SetOnUiBrightnessChanged(std::function<void(float)> f) { onUiBrightnessChanged = f; }

	void SetOnHdrChanged(std::function<void(bool)> f) { onHdrChanged = f; }

	void SetFileOpenFunc(std::function<void()> f) { fileOpenFunc = f; }

	void SetOnColorspaceChanged(std::function<void(std::string)> f) { onColorspaceChanged = f; }

	void AddColorspace(std::string &&colorspace) { colorspaces.emplace_back(std::move(colorspace)); }

	void SetOnPulseMaxBrightnessChanged(std::function<void(bool)> f) { onPulseMaxBrightnessChanged = f; }

private:
	const std::string FontRoot;

	int windowWidth = 0, windowHeight = 0;
	int width = 0, height = 0;

	bool fft = Settings::settings.GetRenderer() == "fft";
	bool fftLine = Settings::settings.GetRenderer() == "fftline";
	bool oscilloscope = Settings::settings.GetRenderer() == "osc";

	bool intensity = Settings::settings.GetLightPackVisualizationType() == "intensity";
	bool color = Settings::settings.GetLightPackVisualizationType() == "color";
	bool colorAndIntensity = Settings::settings.GetLightPackVisualizationType() == "colorintensity";

	bool defaultMapping = Settings::settings.GetLightPackMapping() == "default";
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
	bool cacheDetectionResults = Settings::settings.GetCacheDetectionResults();
	bool halveBpm = Settings::settings.GetHalveBpm();
	bool blur = Settings::settings.GetBlur();
	GLenum sourceFactor = Settings::settings.GetSourceFactor();
	GLenum destFactor = Settings::settings.GetDestFactor();

	float rpm = Settings::settings.GetRotationSpeed() / (360.0f / 60.0f);

	float blurIntensity = Settings::settings.GetBlurIntensity();
	float blurOpacity = Settings::settings.GetBlurOpacity();

	int bufferSize = Settings::settings.GetBufferLength();

	float decayTime = Settings::settings.GetDecayTime().AsSeconds();
	float fadeTime = Settings::settings.GetFadeTime().AsSeconds();

	bool pulse = Settings::settings.GetPulse();
	bool pulseBackground = Settings::settings.GetPulseBackground();
	bool darkenPulseOnBrightColors = Settings::settings.GetDarkenPulseOnBrightColors();
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
	float effectHorizontalSpread = Settings::settings.GetEffectHorizontalSpread();
	float effectVerticalSpread = Settings::settings.GetEffectVerticalSpread();
	float effectRotation = Settings::settings.GetEffectRotation();

	int minPercentage = Settings::settings.GetColorSelection().minPercentage * 100;
	int minHueSeparation = Settings::settings.GetColorSelection().minHueSeparation;
	float minValueSeparation = Settings::settings.GetColorSelection().minValueSeparation;
	float minimumDistance = Settings::settings.GetColorSelection().minRgbSeparation;
	float minimumSaturation = Settings::settings.GetColorSelection().minSaturation;
	float minimumValue = Settings::settings.GetColorSelection().minValue;
	float maxAverageColorVariance = Settings::settings.GetColorSelection().maxAverageColorVariance;
	float maxPerPixelColorVariance = Settings::settings.GetColorSelection().maxPerPixelColorVariance;

	bool limitFramerate = Settings::settings.GetLimitFramerate();
	int frameLimit = Settings::settings.GetFrameLimit();

	bool randomize = Settings::settings.GetRandomize();
	float randomizeTime = Settings::settings.GetRandomizeTime().AsSeconds();

	float visualizerScale = Settings::settings.GetScale();

	std::set<std::size_t> selectedPresets = Settings::settings.GetSelectedPresets();
	bool randomizePresets = Settings::settings.GetRandomizePresets();
	float randomizePresetsTime = Settings::settings.GetRandomizePresetsTime().AsSeconds();
	bool randomizePresetsByBeats = Settings::settings.GetRandomizePresetsByBeats();
	int randomizePresetsBeats = Settings::settings.GetRandomizePresetsBeats();

	bool autoFade = Settings::settings.GetAutoFade();
	float waitTime = Settings::settings.GetWaitTime().AsSeconds();
	float autoFadeSpeed = Settings::settings.GetAutoFadeSpeed();

	bool exclusive = Settings::settings.GetExclsuive();
	int exclusiveVolume = Settings::settings.GetVolume() * 100;

	bool saveRenderer = Settings::settings.GetSaveRenderer();
	bool lastSaveRenderer = Settings::settings.GetSaveRenderer();

	bool saveScale = Settings::settings.GetSaveScale();
	bool lastSaveScale = Settings::settings.GetSaveScale();

	int rendererOffset = Settings::settings.GetRendererOffset();

	std::function<void(const std::filesystem::path &)> onOpen;
	std::function<void(bool)> onPulseChanged;
	std::function<void(bool)> onPulseBackgroundChanged;
	std::function<void(bool)> onDarkenPulseOnBrightColorsChanged;
	std::function<void(bool)> onBlurChanged;
	std::function<void(GLenum)> onSourceFactorChanged;
	std::function<void(GLenum)> onDestFactorChanged;
	std::function<void(bool)> onRotatingChanged;
	std::function<void(bool)> onDetectBpmChanged;
	std::function<void(bool)> onHalveBpmChanged;
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
	std::function<void(float)> onEffectHorizontalSpreadChanged;
	std::function<void(float)> onEffectVerticalSpreadChanged;
	std::function<void(float)> onEffectRotationChanged;
	std::function<void(bool)> onLimitFramerateChanged;
	std::function<void(int)> onFrameLimitChanged;
	std::function<void(bool)> onRandomizeChanged;
	std::function<void(float)> onRandomizeTimeChanged;
	std::function<void(float)> onScaleChanged;
	std::function<void(const std::set<std::size_t> &)> onSelectedPresetsChanged;
	std::function<void(bool)> onRandomizePresetsChanged;
	std::function<void(float)> onRandomizePresetsTimeChanged;
	std::function<void(bool)> onRandomizePresetsByBeatsChanged;
	std::function<void(int)> onRandomizePresetsBeatsChanged;
	std::function<void(bool)> onAutoFadeChanged;
	std::function<void(float)> onWaitTimeChanged;
	std::function<void(float)> onAutoFadeSpeedChanged;
	std::function<void(bool)> onExclusiveChanged;
	std::function<void(int)> onExclusiveVolumeChanged;
	std::function<void(int)> onRendererOffsetChanged;
	std::function<void(const std::string &)> onLutChanged;
	std::function<void(float)> onAlbumArtGammaChanged;
	std::function<void(float)> onAlbumArtContrastChanged;
	std::function<void(float)> onAlbumArtBrightnessChanged;
	std::function<void(std::optional<float>)> onHdrWhitePointChanged;
	std::function<void()> onRescanAlbumArt;
	std::function<void(bool)> onPulseUiChanged;
	std::function<void(float)> onUiGammaChanged;
	std::function<void(float)> onUiContrastChanged;
	std::function<void(float)> onUiBrightnessChanged;
	std::function<void(bool)> onHdrChanged;
	std::function<void(std::string)> onColorspaceChanged;
	std::function<void(bool)> onPulseMaxBrightnessChanged;

	std::function<void()> onResetRotation;
	std::function<void()> onClearBlurFbo;

	std::function<void()> onRandom;

	std::function<void()> onQuit;
	std::function<void()> onResetWindow;

	std::function<void()> fileOpenFunc;

	float scale = 1.0f;
	float lastScale = 1.0f;
	std::optional<ImGuiStyle> originalStyle = std::nullopt;
	ImFont *font = nullptr;

	ptrdiff_t cacheFileCount = 0;
	std::string cacheSize;

	std::chrono::system_clock::time_point lastFrame = std::chrono::system_clock::now();

	std::string lut = Settings::settings.GetLut();
	float albumArtGamma = Settings::settings.GetAlbumArtGamma();
	float albumArtContrast = Settings::settings.GetAlbumArtContrast();
	float albumArtBrightness = Settings::settings.GetAlbumArtBrightness();

	std::optional<float> hdrWhitePoint = Settings::settings.GetHdrWhitePoint();

	bool colorChanged = false;
	bool pulseUi = Settings::settings.GetPulseUi();

	float uiGamma = Settings::settings.GetUiGamma();
	float uiContrast = Settings::settings.GetUiContrast();
	float uiBrightness = Settings::settings.GetUiBrightness();

	float safeAreaPadding = 0.0f;
	bool isTouchscreen = false;
	bool hdr = Settings::settings.GetHdr();

	std::vector<std::string> colorspaces;
	std::string colorspace = Settings::settings.GetColorspace();

	bool pulseMaxBrightness = Settings::settings.GetPulseMaxBrightness();
};
