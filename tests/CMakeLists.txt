# =============================================================================
# hf-bno08x-driver — host unit tests
# =============================================================================
# Builds the driver (and the CEVA SH-2 library it wraps) against a mock transport and runs the tests with
# CTest. No hardware and no ESP-IDF needed; the src/sh2 submodule must be checked out:
#
#   git submodule update --init src/sh2
#   cmake -S . -B build-host -DHF_BNO08X_BUILD_TESTS=ON
#   cmake --build build-host
#   ctest --test-dir build-host --output-on-failure
# =============================================================================

add_executable(hf_bno08x_host_tests
  test_main.cpp
  test_rvc.cpp
  test_driver.cpp
  test_gpio_status.cpp
  test_memory_firmware.cpp
  test_dfu_i2c.cpp
  test_sh2_session.cpp
)

target_link_libraries(hf_bno08x_host_tests PRIVATE ${HF_BNO08X_TARGET_NAME})
target_include_directories(hf_bno08x_host_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(hf_bno08x_host_tests PRIVATE cxx_std_20)
target_compile_options(hf_bno08x_host_tests PRIVATE -Wall -Wextra)

add_test(NAME hf_bno08x_host_tests COMMAND hf_bno08x_host_tests)

# The code shown on the documentation site (docs/ includes these files with pymdownx.snippets), built so a
# page cannot show code that does not compile against the current headers. Compiled, never linked or run.
add_library(hf_bno08x_doc_snippets OBJECT
  docs/quickstart.cpp
  docs/api_tasks.cpp
  docs/configuration.cpp
  docs/rvc.cpp
  docs/dfu.cpp
  docs/ports.cpp
)
target_link_libraries(hf_bno08x_doc_snippets PRIVATE ${HF_BNO08X_TARGET_NAME})
target_include_directories(hf_bno08x_doc_snippets PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/docs)
target_compile_features(hf_bno08x_doc_snippets PRIVATE cxx_std_20)
target_compile_options(hf_bno08x_doc_snippets PRIVATE -Wall -Wextra)

# `cmake --install` must ship every header the public headers include (bno08x.ipp, dfu/HcBin.h,
# dfu/MemoryFirmware.hpp, the SH-2 headers, the generated bno08x_version.h) and export the documented target
# hf::bno08x. Installs into the build tree and builds a find_package() consumer.
add_test(NAME hf_bno08x_install_consumer
  COMMAND ${CMAKE_COMMAND} -DBIN=${PROJECT_BINARY_DIR} -DSRC=${PROJECT_SOURCE_DIR}
          -DCXX=${CMAKE_CXX_COMPILER} -DCC=${CMAKE_C_COMPILER} -P ${CMAKE_CURRENT_SOURCE_DIR}/install_consumer.cmake)
