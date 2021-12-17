cmake_minimum_required(VERSION 3.7)

project(PCem)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PCEM_VERSION_STRING "vNext" CACHE STRING "PCem Version String")

set(PCEM_DEFINES PCEM_VERSION_STRING="${PCEM_VERSION_STRING}")

message("PCem ${PCEM_VERSION_STRING}")
message("***************************************************************")
message("OS Building For: ${CMAKE_SYSTEM_NAME}")
message("System Processor: ${CMAKE_SYSTEM_PROCESSOR}")
message("***************************************************************")

option(USE_NETWORKING "Build PCem with networking support" OFF)
message("Networking Support: ${USE_NETWORKING}")

option(USE_ALSA "Build PCem with MIDI output using ALSA" OFF)
message("ALSA MIDI Support: ${USE_ALSA}")

message("***************************************************************")

set(OpenGL_GL_PREFERENCE GLVND)
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

find_package(OpenAL REQUIRED)
include_directories(${OPENAL_INCLUDE_DIR})

if(WIN32)
    set(wxWidgets_CONFIGURATION mswu)
endif()
find_package(wxWidgets REQUIRED COMPONENTS core base xrc)
include(${wxWidgets_USE_FILE})

if(USE_ALSA)
    find_package(ALSA REQUIRED)
    include_directories(${ALSA_INCLUDE_DIRS})
endif()

find_package(OpenGL REQUIRED)

add_subdirectory(src)
