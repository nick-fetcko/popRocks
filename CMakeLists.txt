# 3.26.0 required for copy_directory_if_different
cmake_minimum_required(VERSION 3.26.0 FATAL_ERROR)
project(popRocks VERSION 0.0.1 LANGUAGES C CXX)

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
	Source/AlbumArt.hpp
	Source/AutoFader.hpp
	Source/BeatDetect.hpp
	Source/Bicubic.hpp
	Source/Buffer.hpp
	Source/CApp.h
	Source/CConsole.h
	Source/ColorChangeListener.hpp
	Source/Controls.hpp
	Source/Cue.hpp
	Source/DynamicGain.hpp
	Source/ExclusiveIndicator.hpp
	Source/FFTLineRenderer.hpp
	Source/FFTRenderer.hpp
	Source/FPSCounter.hpp
	Source/Gaussian.hpp
	Source/Hash.hpp
	Source/ID3V2.hpp
	Source/LightPack.hpp
	Source/LineRenderer.hpp
	Source/Mappings.h
	Source/Metadata.hpp
	Source/MP4.hpp
	Source/OscilloscopeRenderer.hpp
	Source/Playlist.hpp
	Source/Polyline.hpp
	Source/Preset.hpp
	Source/RecordAudioStream.h
	Source/Renderer.hpp
	Source/Settings.hpp
	Source/TagLoader.hpp
	Source/Text.hpp
	Source/Utils.hpp
	Source/Volume.hpp
	)
set(_poprocks_cpp_sources
	Source/AlbumArt.cpp
	Source/BeatDetect.cpp
	Source/Bicubic.cpp
	Source/main.cpp
	Source/CApp.cpp
	Source/CApp_Commands.cpp
	Source/CConsole.cpp
	Source/Controls.cpp
	Source/Cue.cpp
	Source/FFTRenderer.cpp
	Source/Gaussian.cpp
	Source/ID3V2.cpp
	Source/LightPack.cpp
	Source/Mappings.cpp
	Source/Metadata.cpp
	Source/MP4.cpp
	Source/Playlist.cpp
	Source/Preset.cpp
	Source/Settings.cpp
	Source/Text.cpp
	Source/Utils.cpp
	Source/Volume.cpp
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

# Copy data files when building in Release
if (${CMAKE_CONFIGURATION_TYPES} STREQUAL "Release")
	add_custom_command(TARGET popRocks POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${CMAKE_CURRENT_SOURCE_DIR}/Data" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CONFIGURATION_TYPES}/Data"
		)
endif()