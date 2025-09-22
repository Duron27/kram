cmake_minimum_required(VERSION 3.19.1)

#-----------------------------------------------------
# Encoder options
option(ATE "Compile ATE Encoder" OFF)
option(ETCENC "Compile ETC2Comp Encoder" OFF)
option(SQUISH "Compile Squish Encoder" OFF)
option(ASTCENC "Compile ASTCenc Encoder" ON)
option(BCENC "Compile BCenc Encoder" OFF)
option(COMP "Compile Compressonator Encoder" OFF)

option(EASTL "Compile EASTL" OFF)

# Convert ON to 1
set(COMPILE_ATE 0)
set(COMPILE_BCENC 0)
set(COMPILE_ETCENC 0)
set(COMPILE_SQUISH 0)
set(COMPILE_ASTCENC 0)
set(COMPILE_COMP 0)

if (ATE AND BUILD_MAC)
    set(COMPILE_ATE 1)
endif()

if (BCENC)
    set(COMPILE_BCENC 1)
endif()

if (ETCENC)
    set(COMPILE_ETCENC 1)
endif()

if (SQUISH)
    set(COMPILE_SQUISH 1)
endif()

if (ASTCENC)
    set(COMPILE_ASTCENC 1)
endif()

if (COMP)
    set(COMPILE_COMP 1)
endif()

#-----------------------------------------------------
# STL used
set(COMPILE_EASTL 0)
if (EASTL)
    set(COMPILE_EASTL 1)
endif()

#-----------------------------------------------------
# libkram
set(myTargetLib libkram)

# Build as shared library for Android, static for others
if (ANDROID)
    add_library(${myTargetLib} SHARED)
else()
    add_library(${myTargetLib} STATIC)
endif()

if (CMAKE_BUILD_TYPE STREQUAL "DEBUG")
    set(DEBUG 1)
endif()

if (DEBUG)
    set_target_properties(${myTargetLib} PROPERTIES
        OUTPUT_NAME kramd
    )
else()
    set_target_properties(${myTargetLib} PROPERTIES
        OUTPUT_NAME kram
    )
endif()

set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE libSources CONFIGURE_DEPENDS 
    "${SOURCE_DIR}/astc-encoder/*.cpp"
    "${SOURCE_DIR}/astc-encoder/*.h"
    "${SOURCE_DIR}/ate/ateencoder.mm"
    "${SOURCE_DIR}/ate/ateencoder.h"
    "${SOURCE_DIR}/bc7enc/*.cpp"
    "${SOURCE_DIR}/bc7enc/*.h"
    "${SOURCE_DIR}/etc2comp/*.cpp"
    "${SOURCE_DIR}/etc2comp/*.h"
    "${SOURCE_DIR}/fmt/*.cpp"
    "${SOURCE_DIR}/fmt/*.h"
    "${SOURCE_DIR}/heman/hedistance.cpp"
    "${SOURCE_DIR}/heman/hedistance.h"
    "${SOURCE_DIR}/kram/*.pch"
    "${SOURCE_DIR}/kram/*.cpp"
    "${SOURCE_DIR}/kram/*.h"
    "${SOURCE_DIR}/eastl/*.cpp"
    "${SOURCE_DIR}/eastl/*.h"
    "${SOURCE_DIR}/lodepng/lodepng.cpp"
    "${SOURCE_DIR}/lodepng/lodepng.h"
    "${SOURCE_DIR}/squish/*.cpp"
    "${SOURCE_DIR}/squish/*.h"
    "${SOURCE_DIR}/compressonator/bc6h/*.cpp"
    "${SOURCE_DIR}/compressonator/bc6h/*.h"
    "${SOURCE_DIR}/tmpfileplus/tmpfileplus.cpp"
    "${SOURCE_DIR}/tmpfileplus/tmpfileplus.h"
    "${SOURCE_DIR}/vectormath/*.h"
    "${SOURCE_DIR}/vectormath/*.cpp"
    "${SOURCE_DIR}/zstd/zstd.h"
    "${SOURCE_DIR}/zstd/zstd.cpp"
    "${SOURCE_DIR}/miniz/miniz.h"
    "${SOURCE_DIR}/miniz/miniz.cpp"
)

# No objc on Win, Linux, or Android
if (BUILD_WIN OR BUILD_LINUX OR ANDROID)
    list(FILTER libSources EXCLUDE REGEX ".*ateencoder.mm$")
    list(FILTER libSources EXCLUDE REGEX ".*ateencoder.h$")
    list(FILTER libSources EXCLUDE REGEX ".*astcenccli_.*") # Exclude all astcenc CLI files
    list(FILTER libSources EXCLUDE REGEX ".*rdo_bc_encoder.cpp$") # Exclude BC7 encoder file
    list(FILTER libSources EXCLUDE REGEX ".*utils.*") # Exclude any utils files if present
endif()

# Remove files not used
list(FILTER libSources EXCLUDE REGEX ".*test.cpp$")
list(FILTER libSources EXCLUDE REGEX ".squishgen.cpp$")
list(FILTER libSources EXCLUDE REGEX ".Etc.cpp$")
list(FILTER libSources EXCLUDE REGEX ".Etc.h$")
list(FILTER libSources EXCLUDE REGEX ".astcenccli_image_load_store.cpp$")
list(FILTER libSources EXCLUDE REGEX ".astcenccli_error_metrics.cpp$")
list(FILTER libSources EXCLUDE REGEX ".astcenccli_platform_dependents.cpp$")
list(FILTER libSources EXCLUDE REGEX ".astcenccli_toplevel_help.cpp$")
list(FILTER libSources EXCLUDE REGEX ".astcenccli_toplevel.cpp$")
list(FILTER libSources EXCLUDE REGEX ".atomic.h$")
list(FILTER libSources EXCLUDE REGEX ".atomic.cpp$")
list(FILTER libSources EXCLUDE REGEX ".fmt.cpp$")

# Preserve source hierarchy in build project
source_group(TREE "${SOURCE_DIR}" PREFIX "source" FILES ${libSources})

target_include_directories(${myTargetLib} PUBLIC
    "${INCLUDE_DIR}/kram/"
    "${INCLUDE_DIR}/eastl/include/"
    "${INCLUDE_DIR}/vectormath/"
)

target_include_directories(${myTargetLib} PRIVATE
    "${INCLUDE_DIR}/astc-encoder/"
    "${INCLUDE_DIR}/ate/"
    "${INCLUDE_DIR}/bc7enc/"
    "${INCLUDE_DIR}/compressonator/bc6h/"
    "${INCLUDE_DIR}/etc2comp/"
    "${INCLUDE_DIR}/fmt/"
    "${INCLUDE_DIR}/heman/"
    "${INCLUDE_DIR}/lodepng"
    "${INCLUDE_DIR}/miniz/"
    "${INCLUDE_DIR}/squish/"
    "${INCLUDE_DIR}/tmpfileplus/"
    "${INCLUDE_DIR}/zstd/"
)

# Add sources to the library
target_sources(${myTargetLib} PRIVATE ${libSources})

# Link liblog and libtrace for Android
if (ANDROID)
    target_link_libraries(${myTargetLib} PUBLIC log android)
    set_source_files_properties(${SOURCE_DIR}/zstd/zstd.cpp PROPERTIES COMPILE_FLAGS "-Wno-implicit-int-conversion")
endif()

#-----------------------------------------------------
if (BUILD_LINUX)
    target_compile_options(${myTargetLib} PRIVATE -W -Wall 
        -fno-exceptions -fno-rtti
        -fdata-sections -ffunction-sections
    )
    
    if (NOT CMAKE_SYSTEM_PROCESSOR OR CMAKE_SYSTEM_PROCESSOR STREQUAL "")
        execute_process(COMMAND uname -m OUTPUT_VARIABLE CMAKE_SYSTEM_PROCESSOR OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()
    
    message(STATUS "Detected processor: ${CMAKE_SYSTEM_PROCESSOR}")
    
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        message(STATUS "Configuring for ARM64 with NEON")
        target_compile_options(${myTargetLib} PRIVATE -march=armv8-a+simd)
        target_compile_definitions(${myTargetLib} PRIVATE __ARM_NEON=1 __ARM_NEON__=1)
    else()
        message(STATUS "Configuring for x86_64 with AVX2")
        target_compile_options(${myTargetLib} PRIVATE -mavx2 -mfma -mf16c)
    endif()

    target_compile_options(${myTargetLib} PUBLIC -include KramConfig.h)
endif()

if (BUILD_MAC)
    set_target_properties(${myTargetLib} PROPERTIES
        XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym"
        XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH "NO"
    )

    target_compile_options(${myTargetLib} PRIVATE -W -Wall 
        -fobjc-arc
        -fno-exceptions -fno-rtti
        -fdata-sections -ffunction-sections
    )
    
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        target_compile_options(${myTargetLib} PRIVATE -march=armv8-a+simd)
        target_compile_definitions(${myTargetLib} PRIVATE __ARM_NEON=1 __ARM_NEON__=1)
    else()
        target_compile_options(${myTargetLib} PRIVATE -mavx2 -mfma -mf16c)
    endif()

    target_compile_options(${myTargetLib} PUBLIC -include KramConfig.h)
endif()

if (BUILD_WIN)
    set_property(TARGET ${myTargetLib} PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )

    target_compile_options(${myTargetLib} PUBLIC /FIKramConfig.h)
       
    target_compile_options(${myTargetLib} PRIVATE /W3 
        -march=haswell -mf16c -mfma 
        /GR- /EHs-c-
        /MP /GF /FC
    )
    
    target_compile_definitions(${myTargetLib} PRIVATE _ITERATOR_DEBUG_LEVEL=0 _HAS_EXCEPTIONS=0)
endif()

target_precompile_headers(${myTargetLib} PRIVATE
    ${SOURCE_DIR}/kram/KramPrefix.h
)

target_compile_definitions(${myTargetLib}
    PUBLIC
    COMPILE_EASTL=${COMPILE_EASTL}
   
    PRIVATE
    COMPILE_ATE=${COMPILE_ATE}
    COMPILE_BCENC=${COMPILE_BCENC}
    COMPILE_ETCENC=${COMPILE_ETCENC}
    COMPILE_SQUISH=${COMPILE_SQUISH}
    COMPILE_ASTCENC=${COMPILE_ASTCENC}
    COMPILE_COMP=${COMPILE_COMP}
)
