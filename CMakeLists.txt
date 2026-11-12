cmake_minimum_required(VERSION 3.16)
project(A50VoiceClean VERSION 0.2.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(a50_dsp STATIC src/dsp.cpp src/transform.cpp)
target_include_directories(a50_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(MSVC)
  target_compile_options(a50_dsp PRIVATE /W4)
else()
  target_compile_options(a50_dsp PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(a50voiceclean src/main.cpp src/wav.cpp)
target_link_libraries(a50voiceclean PRIVATE a50_dsp)

if(WIN32)
  # Pinned Windows-compatible RNNoise source, compiled statically without AVX2.
  # Downloaded at BUILD time only; application runs without Internet.
  include(FetchContent)
  FetchContent_Declare(rnnoise_vendor
    GIT_REPOSITORY https://github.com/jagger2048/rnnoise-windows.git
    GIT_TAG d24c92c4f41eb786a18d25feea350cdf3319eee5
  )
  FetchContent_GetProperties(rnnoise_vendor)
  if(NOT rnnoise_vendor_POPULATED)
    FetchContent_Populate(rnnoise_vendor)
    add_subdirectory("${rnnoise_vendor_SOURCE_DIR}/src" "${rnnoise_vendor_BINARY_DIR}/src" EXCLUDE_FROM_ALL)
  endif()

  add_executable(A50VoiceClean-GUI WIN32 src/gui_win.cpp src/wav.cpp)
  target_link_libraries(A50VoiceClean-GUI PRIVATE a50_dsp comctl32 comdlg32 winmm)
  target_compile_definitions(A50VoiceClean-GUI PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

  # Step 1: genuine WASAPI input, test WAV, live peak meter. No background audio routing yet.
  add_executable(A50VoiceClean-MicTest src/live_capture_win.cpp)
  target_link_libraries(A50VoiceClean-MicTest PRIVATE ole32 uuid)
  target_compile_definitions(A50VoiceClean-MicTest PRIVATE UNICODE _UNICODE)
  if(MSVC)
    target_compile_options(A50VoiceClean-MicTest PRIVATE /W4 /EHsc)
  endif()
  add_executable(A50VoiceClean-Live src/live_route_win.cpp src/rnnoise_filter.cpp)
  target_include_directories(A50VoiceClean-Live PRIVATE "${rnnoise_vendor_SOURCE_DIR}/include")
  target_link_libraries(A50VoiceClean-Live PRIVATE a50_dsp ole32 uuid rnnoise_static)
  target_compile_definitions(A50VoiceClean-Live PRIVATE UNICODE _UNICODE)
  if(MSVC)
    target_compile_options(A50VoiceClean-Live PRIVATE /W4 /EHsc)
  endif()
endif()

include(CTest)
if(BUILD_TESTING)
  add_executable(a50_dsp_tests tests/test_dsp.cpp)
  target_link_libraries(a50_dsp_tests PRIVATE a50_dsp)
  add_test(NAME a50_dsp_tests COMMAND a50_dsp_tests)
  add_executable(a50_live_buffer_tests tests/test_live_buffer.cpp)
  target_include_directories(a50_live_buffer_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  add_test(NAME a50_live_buffer_tests COMMAND a50_live_buffer_tests)
  if(WIN32)
    # Does not depend on audio hardware in the CI runner.
    add_test(NAME a50_capture_cli_help COMMAND A50VoiceClean-MicTest --help)
    add_test(NAME a50_live_cli_help COMMAND A50VoiceClean-Live --help)
    add_executable(a50_rnnoise_tests tests/test_rnnoise.cpp src/rnnoise_filter.cpp)
    target_include_directories(a50_rnnoise_tests PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/src" "${rnnoise_vendor_SOURCE_DIR}/include")
    target_link_libraries(a50_rnnoise_tests PRIVATE rnnoise_static)
    add_test(NAME a50_rnnoise_tests COMMAND a50_rnnoise_tests)
  endif()
endif()
