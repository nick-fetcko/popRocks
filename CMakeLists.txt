# 3.11.0 required for cxx_std_17 and FetchContent
cmake_minimum_required(VERSION 3.11.0 FATAL_ERROR)
project(popRocks VERSION 0.0.1 LANGUAGES C CXX)

# Allows usage of configure time content
include(FetchContent)

include(GNUInstallDirs)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})

if (WIN32)
	set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/icon.rc")
	set(VERSION_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/version.rc")
endif()

# Since this is Windows only (for now), install these via vcpkg:
#	vcpkg install sdl2:x64-windows
#	vcpkg install sdl2-mixer:x64-windows
#	vcpkg install libjpeg-turbo:x64-windows
#	vcpkg install sdl2-image[libjpeg-turbo]:x64-windows
#	vcpkg install sdl2-net:x64-windows
#	vcpkg install fftw3:x64-windows
#	vcpkg install sdl2-ttf:x64-windows
#
#	fftw3 requires 3 configuration / build steps (for different precisions)
find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_mixer CONFIG REQUIRED)
find_package(JPEG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
find_package(SDL2_net CONFIG REQUIRED)
find_package(SDL2_ttf CONFIG REQUIRED)
find_package(FFTW3 CONFIG REQUIRED)

add_subdirectory(MathCPP)
add_subdirectory(Serial/src)

set(_poprocks_cpp_headers
	AlbumArt.hpp
	AutoFader.hpp
	BeatDetect.hpp
	Bicubic.hpp
	Buffer.hpp
	CApp.h
	CConsole.h
	ColorChangeListener.hpp
	Controls.hpp
	Cue.hpp
	DynamicGain.hpp
	ExclusiveIndicator.hpp
	FFTLineRenderer.hpp
	FFTRenderer.hpp
	FPSCounter.hpp
	Gaussian.hpp
	Hash.hpp
	ID3V2.hpp
	LightPack.hpp
	LineRenderer.hpp
	Mappings.h
	Metadata.hpp
	MP4.hpp
	OscilloscopeRenderer.hpp
	Playlist.hpp
	Polyline.hpp
	Preset.hpp
	RecordAudioStream.h
	Renderer.hpp
	Settings.hpp
	TagLoader.hpp
	Text.hpp
	Utils.hpp
	Volume.hpp
	)
set(_poprocks_cpp_sources
	AlbumArt.cpp
	BeatDetect.cpp
	Bicubic.cpp
	main.cpp
	CApp.cpp
	CApp_Commands.cpp
	CConsole.cpp
	Controls.cpp
	Cue.cpp
	FFTRenderer.cpp
	Gaussian.cpp
	ID3V2.cpp
	LightPack.cpp
	Mappings.cpp
	Metadata.cpp
	MP4.cpp
	Playlist.cpp
	Preset.cpp
	Settings.cpp
	Text.cpp
	Utils.cpp
	Volume.cpp
	)

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

set(_glad_dir
	third_party/glad)

set(_glad_headers
	${_glad_dir}/glad/glad.h
	${_glad_dir}/KHR/khrplatform.h
	)
set(_glad_sources
	${_glad_dir}/glad/glad.c
	)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/third_party FILES
	${_glad_headers}
	${_glad_sources}
	)

add_executable(popRocks
	${_beatroot_headers}
	${_glad_headers}
	${_poprocks_cpp_headers}
	${_beatroot_sources}
	${_glad_sources}
	${_poprocks_cpp_sources}
	)

if (WIN32)
	target_sources(popRocks 
		PRIVATE 		
		${APP_ICON_RESOURCE_WINDOWS}
		${VERSION_RESOURCE_WINDOWS}
		)
endif()

target_include_directories(popRocks PUBLIC 
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	${_beatroot_dir}
	${_glad_dir}
	Serial/src
	)
target_compile_features(popRocks PUBLIC cxx_std_17)

# Prevent windows.h from defining min and max macros
target_compile_definitions(popRocks PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)

# We don't need GDI or bitmaps, either
target_compile_definitions(popRocks PUBLIC NOGDI NOBITMAP)

find_package(OpenGL REQUIRED)

add_library(bass SHARED IMPORTED)
set_target_properties(bass PROPERTIES
	INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bass/include"
	IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bass/lib/bass.dll"
	IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bass/lib/bass.dll"
	IMPORTED_IMPLIB_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bass/lib/bass.lib"
	IMPORTED_IMPLIB_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bass/lib/bass.lib"
	)

add_library(bassflac SHARED IMPORTED)
set_target_properties(bassflac PROPERTIES
	INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassflac/include"
	IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassflac/lib/bassflac.dll"
	IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassflac/lib/bassflac.dll"
	IMPORTED_IMPLIB_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassflac/lib/bassflac.lib"
	IMPORTED_IMPLIB_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassflac/lib/bassflac.lib"
	)

add_library(bassape SHARED IMPORTED)
set_target_properties(bassape PROPERTIES
	INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassape/include"
	IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassape/lib/bassape.dll"
	IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassape/lib/bassape.dll"
	IMPORTED_IMPLIB_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassape/lib/bassape.lib"
	IMPORTED_IMPLIB_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bassape/lib/bassape.lib"
	)

add_library(basswv SHARED IMPORTED)
set_target_properties(basswv PROPERTIES
	INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/basswv/include"
	IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/third_party/basswv/lib/basswv.dll"
	IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/basswv/lib/basswv.dll"
	IMPORTED_IMPLIB_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/third_party/basswv/lib/basswv.lib"
	IMPORTED_IMPLIB_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/basswv/lib/basswv.lib"
	)

add_library(basswasapi SHARED IMPORTED)
set_target_properties(basswasapi PROPERTIES
	INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/basswasapi/include"
	IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/third_party/basswasapi/lib/basswasapi.dll"
	IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/basswasapi/lib/basswasapi.dll"
	IMPORTED_IMPLIB_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/third_party/basswasapi/lib/basswasapi.lib"
	IMPORTED_IMPLIB_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/basswasapi/lib/basswasapi.lib"
	)

target_link_libraries(popRocks PRIVATE MathsCPP ${OPENGL_LIBRARIES} SDL2::SDL2 SDL2::SDL2main SDL2_mixer::SDL2_mixer SDL2_image::SDL2_image SDL2_net::SDL2_net SDL2_ttf::SDL2_ttf bass bassflac bassape basswv basswasapi FFTW3::fftw3 serial ${JPEG_LIBRARIES})
target_include_directories(popRocks PRIVATE ${JPEG_INCLUDE_DIR})

add_custom_command(TARGET popRocks POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CONFIGURATION_TYPES}
	COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:bass> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CONFIGURATION_TYPES}
	COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:bassflac> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CONFIGURATION_TYPES}
	COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:bassape> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CONFIGURATION_TYPES}
	COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:basswv> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CONFIGURATION_TYPES}
	COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:basswasapi> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CONFIGURATION_TYPES}
	)