# Installs the driver from the build tree BIN into BIN/install-test, then configures and builds the consumer in
# tests/install_consumer against it. Fails if a header the public headers include is not installed, if the
# exported target is not hf::bno08x, or if the installed library does not link.
#   cmake -DBIN=<build dir> -DSRC=<repo root> -DCXX=<C++ compiler> -DCC=<C compiler> -P tests/install_consumer.cmake
set(prefix "${BIN}/install-test")
file(REMOVE_RECURSE "${prefix}" "${BIN}/install-consumer")

execute_process(COMMAND "${CMAKE_COMMAND}" --install "${BIN}" --prefix "${prefix}" RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "cmake --install failed (${rc})")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -S "${SRC}/tests/install_consumer" -B "${BIN}/install-consumer"
          "-DCMAKE_PREFIX_PATH=${prefix}" "-DCMAKE_CXX_COMPILER=${CXX}" "-DCMAKE_C_COMPILER=${CC}"
  RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "configuring the consumer against the installed package failed (${rc})")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" --build "${BIN}/install-consumer" RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "building the consumer against the installed package failed (${rc})")
endif()
