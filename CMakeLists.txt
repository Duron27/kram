```cmake
cmake_minimum_required(VERSION 3.19.1 FATAL_ERROR)

#-----------------------------------------------------

# Platform detection
set(BUILD_MAC FALSE)
set(BUILD_WIN FALSE)
set(BUILD_LINUX FALSE)
set(BUILD_ANDROID FALSE)

if (ANDROID)
    message(STATUS "build for Android ${CMAKE_SYSTEM_PROCESSOR}")
    set(BUILD_ANDROID TRUE)
elseif (APPLE)
    message(STATUS "build for macOS ${CMAKE_SYSTEM_PROCESSOR}")
    set(BUILD_MAC TRUE)
elseif (WIN32)
    message(STATUS "build for Windows ${CMAKE_SYSTEM_PROCESSOR}")
    set(BUILD_WIN TRUE)
elseif (UNIX AND NOT APPLE)
    if (NOT CMAKE_SYSTEM_PROCESSOR OR CMAKE_SYSTEM_PROCESSOR STREQUAL "")
        execute_process(COMMAND uname -m OUTPUT_VARIABLE CMAKE_SYSTEM_PROCESSOR OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()
    message(STATUS "build for Linux ${CMAKE_SYSTEM_PROCESSOR}")
    set(BUILD_LINUX TRUE)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

#-----------------------------------------------------

# Suppress ZERO_CHECK project
set(CMAKE_SUPPRESS_REGENERATION true)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release")
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_DEFAULT_STARTUP_PROJECT "kramc")

#-----------------------------------------------------
if (BUILD_ANDROID)
    set(myTargetWorkspace kramWorkspace)

    project(${myTargetWorkspace} LANGUAGES C CXX)

    # Use Clang from NDK (set by android.toolchain.cmake)
    message(STATUS "Using ${CMAKE_CXX_COMPILER_ID} compiler")

    # Build libkram as a shared library
    add_subdirectory(libkram)

    # Optionally build kram CLI
    add_subdirectory(kramc)

    set(BIN_DIR ${PROJECT_SOURCE_DIR}/bin)

    # Install libkram.so and kram to bin directory
    install(TARGETS libkram LIBRARY DESTINATION ${BIN_DIR})
    install(TARGETS kram RUNTIME DESTINATION ${BIN_DIR})

    # Link liblog for Android logging
    target_link_libraries(libkram PUBLIC log)
endif()

#-----------------------------------------------------
if (BUILD_LINUX)
    set(myTargetWorkspace kramWorkspace)

    # Don't force gcc, want clang
    SET(CMAKE_CXX_COMPILER "/usr/bin/clang++" CACHE STRING "C++ compiler" FORCE)
    SET(CMAKE_C_COMPILER "/usr/bin/clang" CACHE STRING "C compiler" FORCE)

    project(${myTargetWorkspace} LANGUAGES C CXX)

    message(STATUS "Using ${CMAKE_CXX_COMPILER_ID} compiler")

    add_subdirectory(libkram)
    add_subdirectory(kramc)

    set(BIN_DIR ${PROJECT_SOURCE_DIR}/bin)

    install(TARGETS libkram ARCHIVE DESTINATION ${BIN_DIR})
    install(TARGETS kram RUNTIME DESTINATION ${BIN_DIR})
endif()

#-----------------------------------------------------
if (BUILD_WIN)
    set(myTargetWorkspace kramWorkspace)

    project(${myTargetWorkspace} LANGUAGES C CXX)

    message(STATUS "Using ${CMAKE_CXX_COMPILER_ID} compiler")

    add_subdirectory(libkram)
    add_subdirectory(kramc)
    # add_subdirectory(kram-thumb-win) # Disabled as it requires shared libkram

    set(BIN_DIR ${PROJECT_SOURCE_DIR}/bin)

    install(TARGETS libkram ARCHIVE DESTINATION ${BIN_DIR})
    install(TARGETS kram RUNTIME DESTINATION ${BIN_DIR})
endif()

#-----------------------------------------------------
if (BUILD_MAC)
    set(myTargetWorkspace kramWorkspace)

    project(${myTargetWorkspace} LANGUAGES C CXX OBJCXX)

    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS")
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Architecture macOS")

    add_subdirectory(libkram)
    add_subdirectory(kramc)
    add_subdirectory(kramv)
    add_subdirectory(plugin)
    add_subdirectory(hlslparser)

    set(BIN_DIR ${PROJECT_SOURCE_DIR}/bin)

    install(TARGETS libkram ARCHIVE DESTINATION ${BIN_DIR})
    install(TARGETS kram RUNTIME DESTINATION ${BIN_DIR})
    install(TARGETS kramv BUNDLE DESTINATION ${BIN_DIR})
    install(TARGETS hlslparser RUNTIME DESTINATION ${BIN_DIR})
endif()
```
