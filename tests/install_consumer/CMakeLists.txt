# Consumer of an installed hf-bno08x-driver: find_package() + the documented target name. Built by the
# hf_bno08x_install_consumer test (tests/install_consumer.cmake) against a fresh `cmake --install`.
cmake_minimum_required(VERSION 3.16)
project(hf_bno08x_install_consumer LANGUAGES C CXX)

find_package(hf_bno08x CONFIG REQUIRED)

# Linked, never run: proves the headers compile and the installed library resolves the SH-2 symbols.
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE hf::bno08x)
