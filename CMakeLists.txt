#===============================================================================
# BNO08x Driver - Root CMake
# Defines the main build target for the driver. Minimal, platform-agnostic.
#===============================================================================

cmake_minimum_required(VERSION 3.16)

project(hf_bno08x_driver LANGUAGES C CXX)

include(GNUInstallDirs)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/hf_bno08x_build_settings.cmake)

if(HF_BNO08X_SOURCE_FILES)
    add_library(${HF_BNO08X_TARGET_NAME} STATIC ${HF_BNO08X_SOURCE_FILES})
    target_include_directories(${HF_BNO08X_TARGET_NAME}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/inc>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src/dfu>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src/sh2>
            $<BUILD_INTERFACE:${HF_BNO08X_VERSION_HEADER_DIR}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/dfu>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/sh2>
    )
    target_compile_features(${HF_BNO08X_TARGET_NAME} PUBLIC cxx_std_20)
else()
    add_library(${HF_BNO08X_TARGET_NAME} INTERFACE)
    target_include_directories(${HF_BNO08X_TARGET_NAME}
        INTERFACE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/inc>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src/dfu>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src/sh2>
            $<BUILD_INTERFACE:${HF_BNO08X_VERSION_HEADER_DIR}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/dfu>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/sh2>
    )
    target_compile_features(${HF_BNO08X_TARGET_NAME} INTERFACE cxx_std_20)
endif()

add_library(hf::bno08x ALIAS ${HF_BNO08X_TARGET_NAME})
# Installed copies export the same name: hf:: + EXPORT_NAME = hf::bno08x (was hf::hf_bno08x).
set_target_properties(${HF_BNO08X_TARGET_NAME} PROPERTIES EXPORT_NAME bno08x)

set_target_properties(${HF_BNO08X_TARGET_NAME} PROPERTIES VERSION ${HF_BNO08X_VERSION} SOVERSION ${HF_BNO08X_VERSION_MAJOR})

option(HF_BNO08X_ENABLE_WARNINGS "Enable extra warnings for BNO08x driver" OFF)
if(HF_BNO08X_ENABLE_WARNINGS)
    if(TARGET ${HF_BNO08X_TARGET_NAME})
        target_compile_options(${HF_BNO08X_TARGET_NAME} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

# ── Host unit tests (mock transport; no hardware) ────────────────────────────
option(HF_BNO08X_BUILD_TESTS "Build the host unit tests in tests/" OFF)
if(HF_BNO08X_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(CMakePackageConfigHelpers)

install(TARGETS ${HF_BNO08X_TARGET_NAME} EXPORT hf_bno08xTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Every header the public headers include, in the layout they include it with: bno08x.hpp pulls in
# bno08x.ipp, dfu/HcBin.h, sh2/*.h and the generated bno08x_version.h; bno08x.ipp pulls in
# dfu/MemoryFirmware.hpp.
install(FILES
    inc/bno08x.hpp
    inc/bno08x_comm_interface.hpp
    inc/bno08x_shtp_uart.hpp
    src/bno08x.ipp
    "${HF_BNO08X_VERSION_HEADER}"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES src/dfu/HcBin.h src/dfu/MemoryFirmware.hpp src/dfu/firmware.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/dfu)
install(DIRECTORY src/sh2/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/sh2
    FILES_MATCHING PATTERN "*.h")
install(FILES src/sh2/NOTICE.txt
    DESTINATION ${CMAKE_INSTALL_DATADIR}/hf_bno08x/sh2)

install(EXPORT hf_bno08xTargets
    FILE hf_bno08xTargets.cmake
    NAMESPACE hf::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hf_bno08x
)

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/cmake/hf_bno08xConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/hf_bno08xConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hf_bno08x
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/hf_bno08xConfigVersion.cmake"
    VERSION ${HF_BNO08X_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/hf_bno08xConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/hf_bno08xConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hf_bno08x
)
