# 3.26.0 required for copy_directory_if_different
cmake_minimum_required(VERSION 3.26.0 FATAL_ERROR)
project(popRocks VERSION 0.1.0 LANGUAGES C CXX)

# Allows usage of configure time content
include(FetchContent)

include(GNUInstallDirs)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})

if (WIN32)
	set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.rc")
	set(VERSION_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/version.rc")
endif()

# Since this is Windows only (for now), install these via vcpkg:
#	vcpkg install sdl2:x64-windows
#	vcpkg install sdl2-mixer:x64-windows
#	vcpkg install libjpeg-turbo:x64-windows
#	vcpkg install sdl2-image[libjpeg-turbo]:x64-windows
#	vcpkg install sdl2-net:x64-windows
#	vcpkg install fftw3:x64-windows
#	vcpkg install freetype:x64-windows
#
#	fftw3 requires 3 configuration / build steps (for different precisions)
find_package(SDL3 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
#find_package(SDL2_mixer CONFIG REQUIRED)
#find_package(JPEG REQUIRED)
find_package(SDL3_image CONFIG REQUIRED)
if (WIN32)
	find_package(FFTW3f CONFIG REQUIRED)
	find_package(Freetype CONFIG REQUIRED)
else ()
	find_package(PkgConfig REQUIRED)
	pkg_search_module(FFTW REQUIRED fftw3f IMPORTED_TARGET)
	include_directories(PkgConfig::FFTW)
	link_libraries(PkgConfig::FFTW)
	pkg_search_module(Freetype REQUIRED freetype2 IMPORTED_TARGET)
	include_directories(PkgConfig::Freetype)
	link_libraries(PkgConfig::Freetype)
	pkg_search_module(Wayland REQUIRED wayland-client IMPORTED_TARGET)
	link_libraries(PkgConfig::Wayland)
	pkg_search_module(dbus REQUIRED dbus-1 IMPORTED_TARGET)
	include_directories(PkgConfig::dbus)
	link_libraries(PkgConfig::dbus)
	pkg_search_module(Pipewire REQUIRED libpipewire-0.3 IMPORTED_TARGET)
	include_directories(PkgConfig::Pipewire)
	link_libraries(PkgConfig::Pipewire)
	pkg_search_module(SPA REQUIRED libspa-0.2 IMPORTED_TARGET)
	include_directories(PkgConfig::SPA)
	link_libraries(PkgConfig::SPA)
	find_path(PULSEAUDIO_INCLUDE_DIR
        NAMES pulse/pulseaudio.h
        DOC "The PulseAudio include directory"
        )

	find_library(PULSEAUDIO_LIBRARY
        NAMES pulse
        DOC "The PulseAudio library"
        )
endif()

add_subdirectory(Serial/src)
add_subdirectory(OpenGL)
add_subdirectory(Utils)
add_subdirectory(nativefiledialog-extended)
# There's no vcpkg port for SDL3_net
add_subdirectory(third_party/SDL_net-main EXCLUDE_FROM_ALL)

set(_poprocks_cpp_headers
	Source/AlbumArt.hpp
	Source/APE.hpp
	Source/AutoFader.hpp
	Source/BeatDetect.hpp
	Source/Bicubic.hpp
	Source/CApp.h
	Source/Checkbox.hpp
	Source/Circle.hpp
	Source/Close.hpp
	Source/ColorChangeListener.hpp
	Source/Controls.hpp
	Source/Cue.hpp
	Source/DoubleClick.hpp
	Source/DynamicGain.hpp
	Source/ExclusiveIndicator.hpp
	Source/FFTLineRenderer.hpp
	Source/FFTRenderer.hpp
	Source/FLAC.hpp
	Source/FPSCounter.hpp
	Source/Gaussian.hpp
	Source/Hamburger.hpp
	Source/HDR.hpp
	Source/Help.hpp
	Source/ID3V2.hpp
	Source/imconfig.h
	Source/JPEG.hpp
	Source/LightPack.hpp
	Source/LineRenderer.hpp
	Source/Loading.hpp
	Source/Mappings.h
	Source/Menu.hpp
	Source/Metadata.hpp
	Source/MetadataReader.hpp
	Source/MiniPlayerCheckboxList.hpp
	Source/MiniPlayerList.hpp
	Source/MP3.hpp
	Source/MP4.hpp
	Source/MT19937.hpp
	Source/Next.hpp
	Source/OGG.hpp
	Source/OscilloscopeRenderer.hpp
	Source/Pause.hpp
	Source/Play.hpp
	Source/Playlist.hpp
	Source/Preset.hpp
	Source/PresetList.hpp
	Source/Previous.hpp
	Source/PRNG.hpp
	Source/RecordAudioStream.h
	Source/Renderer.hpp
	Source/SampleRNG.hpp
	Source/ScrollingText.hpp
	Source/Settings.hpp
	Source/Symbol.hpp
	Source/TagLoader.hpp
	Source/Text.hpp
	Source/Volume.hpp
	Source/WV.hpp
	Source/Integrations/Integration.hpp
	Source/Platforms/Platform.hpp
	Source/Platforms/Desktop.hpp
	)
set(_poprocks_cpp_sources
	Source/AlbumArt.cpp
	Source/BeatDetect.cpp
	Source/Bicubic.cpp
	Source/main.cpp
	Source/CApp.cpp
	Source/CApp_Commands.cpp
	Source/Controls.cpp
	Source/Cue.cpp
	Source/FFTLineRenderer.cpp
	Source/FFTRenderer.cpp
	Source/Gaussian.cpp
	Source/ID3V2.cpp
	Source/LightPack.cpp
	Source/LineRenderer.cpp
	Source/Mappings.cpp
	Source/Metadata.cpp
	Source/MiniPlayerCheckboxList.cpp
	Source/MiniPlayerList.cpp
	Source/MP4.cpp
	Source/MT19937.cpp
	Source/OscilloscopeRenderer.cpp
	Source/RecordAudioStream.cpp
	Source/Renderer.cpp
	Source/Playlist.cpp
	Source/Preset.cpp
	Source/SampleRNG.cpp
	Source/ScrollingText.cpp
	Source/Settings.cpp
	Source/Text.cpp
	Source/Volume.cpp
	Source/Integrations/Integration.cpp
	Source/Platforms/Platform.cpp
	Source/Platforms/Desktop.cpp
	)

if (WIN32)
	list(APPEND _poprocks_cpp_headers Source/Platforms/Windows.hpp)
	list(APPEND _poprocks_cpp_sources Source/Platforms/Windows.cpp)
else()
	list(APPEND _poprocks_cpp_headers Source/Integrations/MPRIS.hpp)
	list(APPEND _poprocks_cpp_sources Source/Integrations/MPRIS.cpp)
	list(APPEND _poprocks_cpp_headers Source/Platforms/Linux.hpp)
	list(APPEND _poprocks_cpp_sources Source/Platforms/Linux.cpp)
	list(APPEND _poprocks_cpp_headers Source/Platforms/Flatpak.hpp)
	list(APPEND _poprocks_cpp_sources Source/Platforms/Flatpak.cpp)
endif()

# Beatroot source obtained from
#   https://code.soundsoftware.ac.uk/projects/beatroot-vamp
set(_beatroot_dir
	third_party/beatroot)

set(_beatroot_headers
	${_beatroot_dir}/Agent.h
	${_beatroot_dir}/AgentList.h
	${_beatroot_dir}/BeatRootProcessor.h
	${_beatroot_dir}/BeatTracker.h
	${_beatroot_dir}/Event.h
	${_beatroot_dir}/Induction.h
	${_beatroot_dir}/Peaks.h
	)
set(_beatroot_sources
	${_beatroot_dir}/Agent.cpp
	${_beatroot_dir}/AgentList.cpp
	${_beatroot_dir}/BeatRootProcessor.cpp
	${_beatroot_dir}/BeatTracker.cpp
	${_beatroot_dir}/Induction.cpp
	${_beatroot_dir}/Peaks.cpp
	)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/third_party FILES
	${_beatroot_headers}
	${_beatroot_sources}
	)

set(_imgui_dir
	imgui)

set(_imgui_headers
	${_imgui_dir}/imgui.h
	${_imgui_dir}/imgui_internal.h
	${_imgui_dir}/imstb_rectpack.h
	${_imgui_dir}/imstb_textedit.h
	${_imgui_dir}/imstb_truetype.h
	${_imgui_dir}/misc/cpp/imgui_stdlib.h
	${_imgui_dir}/misc/freetype/imgui_freetype.h
	)

set(_imgui_sources
	${_imgui_dir}/imgui.cpp
	${_imgui_dir}/imgui_demo.cpp
	${_imgui_dir}/imgui_draw.cpp
	${_imgui_dir}/imgui_tables.cpp
	${_imgui_dir}/imgui_widgets.cpp
	${_imgui_dir}/misc/cpp/imgui_stdlib.cpp
	${_imgui_dir}/misc/freetype/imgui_freetype.cpp
	)

set(_imgui_backend_headers
	${_imgui_dir}/backends/imgui_impl_opengl3_loader.h
	${_imgui_dir}/backends/imgui_impl_opengl3.h
	${_imgui_dir}/backends/imgui_impl_sdl3.h
	)

set(_imgui_backend_sources
	${_imgui_dir}/backends/imgui_impl_opengl3.cpp
	${_imgui_dir}/backends/imgui_impl_sdl3.cpp
	)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES
	${_imgui_headers}
	${_imgui_backend_headers}
	${_imgui_sources}
	${_imgui_backend_sources}
	)

add_executable(popRocks
	${_beatroot_headers}
	${_poprocks_cpp_headers}
	${_imgui_headers}
	${_imgui_backend_headers}
	${_beatroot_sources}
	${_poprocks_cpp_sources}
	${_imgui_sources}
	${_imgui_backend_sources}
	)

if (WIN32)
	# Use Windows subsystem instead of Console
	if (${CMAKE_CONFIGURATION_TYPES} STREQUAL "Release")
		set_target_properties(popRocks PROPERTIES 
			WIN32_EXECUTABLE TRUE
			LINK_FLAGS "/ENTRY:mainCRTStartup"
			)
	endif()
	target_sources(popRocks 
		PRIVATE 		
		${APP_ICON_RESOURCE_WINDOWS}
		${VERSION_RESOURCE_WINDOWS}
		)
else()
	target_sources(popRocks
		PRIVATE
		third_party/xdg/xdg-shell-client-protocol.h
		third_party/xdg/xdg-shell-protocol.c
		)
	target_include_directories(popRocks PRIVATE
		third_party/xdg
		)
endif()

target_include_directories(popRocks PUBLIC 
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	${_beatroot_dir}
	${_imgui_dir}
	${_imgui_dir}/misc/cpp
	${_imgui_dir}/misc/freetype
	Serial/src
	)
target_compile_features(popRocks PUBLIC cxx_std_17)

# Prevent windows.h from defining min and max macros
target_compile_definitions(popRocks PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)

find_package(OpenGL REQUIRED)

add_library(bass SHARED IMPORTED)
if (WIN32)
	set_target_properties(bass PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bass/include"
		IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bass/lib/bass.dll"
		IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bass/lib/bass.dll"
		IMPORTED_IMPLIB_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bass/lib/bass.lib"
		IMPORTED_IMPLIB_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bass/lib/bass.lib"
		)
else()
	set_target_properties(bass PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bass/include"
		IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bass/lib/libbass.so"
		)
endif()

add_library(bassflac SHARED IMPORTED)
if (WIN32)
	set_target_properties(bassflac PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassflac/include"
		IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassflac/lib/bassflac.dll"
		IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassflac/lib/bassflac.dll"
		IMPORTED_IMPLIB_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassflac/lib/bassflac.lib"
		IMPORTED_IMPLIB_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassflac/lib/bassflac.lib"
		)
else()
	set_target_properties(bassflac PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassflac/include"
		IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassflac/lib/libbassflac.so"
		)
endif()

add_library(bassape SHARED IMPORTED)
if (WIN32)
	set_target_properties(bassape PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassape/include"
		IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassape/lib/bassape.dll"
		IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassape/lib/bassape.dll"
		IMPORTED_IMPLIB_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassape/lib/bassape.lib"
		IMPORTED_IMPLIB_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassape/lib/bassape.lib"
		)
else()
	set_target_properties(bassape PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassape/include"
		IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassape/lib/libbassape.so"
		)
endif()

add_library(basswv SHARED IMPORTED)
if (WIN32)
	set_target_properties(basswv PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/basswv/include"
		IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/third_party/basswv/lib/basswv.dll"
		IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/basswv/lib/basswv.dll"
		IMPORTED_IMPLIB_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/third_party/basswv/lib/basswv.lib"
		IMPORTED_IMPLIB_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/basswv/lib/basswv.lib"
		)
else()
	set_target_properties(basswv PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/basswv/include"
		IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/third_party/basswv/lib/libbasswv.so"
		)
endif()

add_library(basstta SHARED IMPORTED)
if (WIN32)
	
	set_target_properties(basstta PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/basstta/include"
		IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/third_party/basstta/lib/bass_tta.dll"
		IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/basstta/lib/bass_tta.dll"
		IMPORTED_IMPLIB_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/third_party/basstta/lib/bass_tta.lib"
		IMPORTED_IMPLIB_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/basstta/lib/bass_tta.lib"
		)
else()
	set_target_properties(basstta PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/basstta/include"
		IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/third_party/basstta/lib/libbass_tta.so"
		)
endif()

if (WIN32)
	add_library(basswasapi SHARED IMPORTED)
	set_target_properties(basswasapi PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/basswasapi/include"
		IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/third_party/basswasapi/lib/basswasapi.dll"
		IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/basswasapi/lib/basswasapi.dll"
		IMPORTED_IMPLIB_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/third_party/basswasapi/lib/basswasapi.lib"
		IMPORTED_IMPLIB_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/basswasapi/lib/basswasapi.lib"
		)
else()
	add_library(bassalac SHARED IMPORTED)
	set_target_properties(bassalac PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassalac/include"
		IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassalac/lib/libbassalac.so"
		)
	add_library(bassaac SHARED IMPORTED)
	set_target_properties(bassaac PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassaac/include"
		IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassaac/lib/libbass_aac.so"
		)
endif()

target_include_directories(popRocks PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/third_party/bass/include"
		"${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassflac/include"
		"${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassape/include"
		"${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassalac/include"
		"${CMAKE_CURRENT_SOURCE_DIR}/third_party/basswv/include"
		"${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassaac/include"
		"${CMAKE_CURRENT_SOURCE_DIR}/third_party/basstta/include"
		)

if (USING_FLATPAK)
	target_compile_definitions(popRocks PUBLIC USING_FLATPAK)
	target_link_libraries(popRocks PRIVATE
			Utils 
			${OPENGL_LIBRARIES}
			OpenGL
			Vulkan::Vulkan
			$<IF:$<TARGET_EXISTS:SDL3_image::SDL3_image-shared>,SDL3_image::SDL3_image-shared,SDL3_image::SDL3_image-static> 
			SDL3_net::SDL3_net 
			freetype 
			"${CMAKE_CURRENT_SOURCE_DIR}/third_party/bass/lib/libbass.so" 
			"${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassflac/lib/libbassflac.so"
			"${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassape/lib/libbassape.so"
			"${CMAKE_CURRENT_SOURCE_DIR}/third_party/basswv/lib/libbasswv.so"
			"${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassaac/lib/libbass_aac.so"
			"${CMAKE_CURRENT_SOURCE_DIR}/third_party/basstta/lib/libbass_tta.so"
			serial
			nfd
			SDL3::SDL3
			)
else()
	target_link_libraries(popRocks PRIVATE
			Utils 
			${OPENGL_LIBRARIES}
			OpenGL
			Vulkan::Vulkan
			$<IF:$<TARGET_EXISTS:SDL3_image::SDL3_image-shared>,SDL3_image::SDL3_image-shared,SDL3_image::SDL3_image-static> 
			SDL3_net::SDL3_net 
			freetype 
			bass
			bassflac
			bassape
			basswv
			basstta
			serial
			nfd
			SDL3::SDL3
			)
endif()

if (WIN32)
	target_link_libraries(popRocks PRIVATE FFTW3::fftw3f basswasapi)

	# For some reason multicore compilation
	# is still not the default for MSVC
	if (MSVC)
		target_compile_options(popRocks PRIVATE "/MP")
	endif()
else()
	if (USING_FLATPAK)
		target_link_libraries(popRocks PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassalac/lib/libbassalac.so" "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassaac/lib/libbass_aac.so" pulse)
	else()
		target_link_libraries(popRocks PRIVATE bassalac bassaac pulse)
	endif()

	# Here lies wstring_convert
	# C++11 - C++17
	# Gone Too Soon
	target_compile_options(popRocks PRIVATE -Wno-deprecated-declarations)
endif()

add_custom_command(TARGET popRocks POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CONFIGURATION_TYPES}
	COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:bass> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CONFIGURATION_TYPES}
	COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:bassflac> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CONFIGURATION_TYPES}
	COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:bassape> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CONFIGURATION_TYPES}
	COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:basswv> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CONFIGURATION_TYPES}
	COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:basstta> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CONFIGURATION_TYPES}
	)

if (WIN32)
	add_custom_command(TARGET popRocks POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:basswasapi> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CONFIGURATION_TYPES}
		)
else()
	add_custom_command(TARGET popRocks POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:bassalac> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CONFIGURATION_TYPES}
		COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:bassaac> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CONFIGURATION_TYPES}
		)

	install(IMPORTED_RUNTIME_ARTIFACTS bass bassflac bassape basswv bassalac bassaac basstta)
	install(TARGETS popRocks)
endif()

# Copy data files when building in Release
if (WIN32)
	if (${CMAKE_CONFIGURATION_TYPES} STREQUAL "Release")
		add_custom_command(TARGET popRocks POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${CMAKE_CURRENT_SOURCE_DIR}/Data" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CONFIGURATION_TYPES}/Data"
			COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/Data/Uninstall.bat" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CONFIGURATION_TYPES}/Uninstall.bat"
			)
	endif()
elseif(USING_FLATPAK)
	add_custom_command(TARGET popRocks POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${CMAKE_CURRENT_SOURCE_DIR}/Data" "${CMAKE_INSTALL_PREFIX}/bin/Data"
	)
	add_custom_command(TARGET popRocks POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bass/lib/libbass.so" ${CMAKE_INSTALL_PREFIX}/bin/libbass.so
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassflac/lib/libbassflac.so" ${CMAKE_INSTALL_PREFIX}/bin/libbassflac.so
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassape/lib/libbassape.so" ${CMAKE_INSTALL_PREFIX}/bin/libbassape.so
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/third_party/basswv/lib/libbasswv.so" ${CMAKE_INSTALL_PREFIX}/bin/libbasswv.so
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassalac/lib/libbassalac.so" ${CMAKE_INSTALL_PREFIX}/bin/libbassalac.so
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassaac/lib/libbass_aac.so" ${CMAKE_INSTALL_PREFIX}/bin/libbass_aac.so
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/third_party/basstta/lib/libbass_tta.so" ${CMAKE_INSTALL_PREFIX}/bin/libbass_tta.so
	)
else()
	add_custom_command(TARGET popRocks POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${CMAKE_CURRENT_SOURCE_DIR}/Data" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CONFIGURATION_TYPES}/Data"
	)
endif()
