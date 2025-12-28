cmake_minimum_required(VERSION 3.16)

# ============================================================
# Project
# ============================================================
project(Nacfetch VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================
# Static linking for MinGW
# ============================================================
if(MINGW)
    set(CMAKE_EXE_LINKER_FLAGS
        "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
endif()

# ============================================================
# Options
# ============================================================
option(BUILD_TESTS "Build test executable" ON)
option(ULTRA_FAST "Enable insane optimizations" ON)
include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT IPO_ERROR)

if(ULTRA_FAST AND IPO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
elseif(ULTRA_FAST)
    message(WARNING "IPO/LTO not supported, disabling")
endif()


# ============================================================
# Core library
# ============================================================
add_library(nacfetch STATIC)

if(WIN32)
    target_sources(nacfetch PRIVATE src/sysinfo.win.cpp)
else()
    target_sources(nacfetch PRIVATE src/sysinfo.cpp)
endif()

target_include_directories(nacfetch PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ============================================================
# Windows libraries
# ============================================================
if(WIN32)
    target_link_libraries(nacfetch PRIVATE
        ws2_32 iphlpapi ole32 oleaut32 uuid
    )

    if(MSVC)
        target_link_libraries(nacfetch PRIVATE wbemuuid pdh setupapi)
    else()
        target_compile_definitions(nacfetch PRIVATE INITGUID _WIN32_DCOM)
    endif()
endif()

# ============================================================
# ULTRA FAST FLAGS
# ============================================================
if(ULTRA_FAST AND NOT MSVC)
    set(COMMON_FAST_FLAGS
        -O3 -Ofast -flto
        -funroll-loops
        -fomit-frame-pointer
        -ffast-math
        -fno-exceptions
        -fno-rtti
        -fno-stack-protector
        -fstrict-aliasing
        -fvisibility=hidden
        -DNDEBUG
    )

    set(COMMON_LINK_FLAGS
        -flto
        -Wl,-O3
        -Wl,--strip-all
        -Wl,--as-needed
    )

    set(INTEL_FLAGS
        -march=native
        -mtune=intel
        -mavx
        -mavx2
        -mfma
    )

    set(AMD_FLAGS
        -march=znver3
        -mtune=znver3
        -mavx2
        -mfma
    )

    set(GENERIC_FLAGS
        -march=x86-64
        -mtune=generic
    )
endif()

# ============================================================
# Function: create CPU-specific executable
# ============================================================
function(make_fast_exec name cpu_flags)
    add_executable(${name} src/main.cpp)
    target_link_libraries(${name} PRIVATE nacfetch)

    if(WIN32)
        target_link_libraries(${name} PRIVATE
            ws2_32 iphlpapi ole32 oleaut32 uuid
        )
        if(MSVC)
            target_link_libraries(${name} PRIVATE wbemuuid pdh setupapi)
        endif()
    endif()

    if(ULTRA_FAST)
        if(MSVC)
            target_compile_options(${name} PRIVATE
                /O2 /GL /fp:fast /GR- /EHsc- /GS-
            )
            target_link_options(${name} PRIVATE
                /LTCG /OPT:REF /OPT:ICF
            )
        else()
            target_compile_options(${name} PRIVATE
                ${COMMON_FAST_FLAGS}
                ${cpu_flags}
            )
            target_link_options(${name} PRIVATE
                ${COMMON_LINK_FLAGS}
            )
        endif()
    endif()

    set_target_properties(${name} PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION TRUE
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output
    )
endfunction()

# ============================================================
# Build executables
# ============================================================
make_fast_exec(nacfetch-generic "${GENERIC_FLAGS}")

if(ULTRA_FAST)
    make_fast_exec(nacfetch-intel "${INTEL_FLAGS}")
    make_fast_exec(nacfetch-amd   "${AMD_FLAGS}")
endif()

# ============================================================
# Tests
# ============================================================
if(BUILD_TESTS)
    add_executable(test src/test.cpp)
    target_link_libraries(test PRIVATE nacfetch)
    set_target_properties(test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output
    )
endif()

# ============================================================
# Output dirs
# ============================================================
set_target_properties(nacfetch PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output
)

# ============================================================
# Install
# ============================================================
install(TARGETS nacfetch-generic
    RUNTIME DESTINATION bin
)

# ============================================================
# Packaging
# ============================================================
set(CPACK_PACKAGE_NAME "nacfetch")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "Aferiad Kamal")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
include(CPack)

# ============================================================
# Info
# ============================================================
message(STATUS "=================================")
message(STATUS "Nacfetch ${PROJECT_VERSION}")
message(STATUS "ULTRA_FAST: ${ULTRA_FAST}")
message(STATUS "BUILD_TESTS: ${BUILD_TESTS}")
message(STATUS "=================================")
