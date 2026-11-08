cmake_minimum_required(VERSION 3.20)
project(BONKR VERSION 0.1.2 LANGUAGES CXX)

# Keep windows.h from defining the min/max macros no matter which header pulls it in.
add_compile_definitions($<$<CXX_COMPILER_ID:MSVC>:NOMINMAX>)
# The tests and tools hold engines on the stack, as Linux's and macOS's 8 MB stacks allow;
# Windows gives an executable 1 MB, so its programs get 16 MB. The plug-in keeps its engine on
# the heap and is unaffected.
if(MSVC)
  string(APPEND CMAKE_EXE_LINKER_FLAGS " /STACK:16777216")
endif()

option(BONKR_COVERAGE "Instrument the tests for line coverage (GCC/Clang)" OFF)
option(BONKR_EDITOR "Build the native editor" ON)

function(bonkr_warnings target)
  target_compile_options(${target} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wno-sign-conversion>)
endfunction()
# The tests use CHECK-style soft assertions plus a few hard asserts; never strip them.
function(bonkr_enable_assertions target)
  target_compile_options(${target} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
endfunction()
function(bonkr_coverage target)
  if(BONKR_COVERAGE AND NOT MSVC)
    target_compile_options(${target} PRIVATE --coverage -O0 -g)
    target_link_options(${target} PUBLIC --coverage)
  endif()
endfunction()

# --- third party --------------------------------------------------------------------------------
# The CLAP headers are pinned. Offline builds point FETCHCONTENT_SOURCE_DIR_CLAP at an existing
# checkout (see README).
include(FetchContent)
FetchContent_Declare(clap
  GIT_REPOSITORY https://github.com/free-audio/clap.git
  GIT_TAG 1.2.7
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(clap)

# --- the engine ---------------------------------------------------------------------------------
# Everything that makes sound, apart from the CLAP glue and the editor, so the tests can link
# it without loading the plug-in. The DSP is header-only; core.cpp compiles it once as a check.
find_package(Threads REQUIRED)
add_library(bonkr_core STATIC src/core.cpp)
target_link_libraries(bonkr_core PUBLIC Threads::Threads)
target_compile_features(bonkr_core PUBLIC cxx_std_20)
target_include_directories(bonkr_core PUBLIC src)
set_target_properties(bonkr_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
bonkr_warnings(bonkr_core)
bonkr_coverage(bonkr_core)

# --- the plug-in --------------------------------------------------------------------------------
if(BONKR_EDITOR AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_package(X11 REQUIRED)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(XFT REQUIRED IMPORTED_TARGET xft)
endif()

add_library(bonkr_clap MODULE src/bonkr.cpp)
target_compile_features(bonkr_clap PRIVATE cxx_std_20)
target_include_directories(bonkr_clap PRIVATE "${clap_SOURCE_DIR}/include")
target_compile_definitions(bonkr_clap PRIVATE BONKR_VERSION="${PROJECT_VERSION}")
target_link_libraries(bonkr_clap PRIVATE bonkr_core)
bonkr_warnings(bonkr_clap)
bonkr_coverage(bonkr_clap)
if(BONKR_EDITOR)
  target_compile_definitions(bonkr_clap PRIVATE BONKR_EDITOR=1)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(bonkr_clap PRIVATE X11::X11 X11::Xrender PkgConfig::XFT)
  elseif(WIN32)
    target_link_libraries(bonkr_clap PRIVATE user32 gdi32)
  elseif(APPLE)
    enable_language(OBJCXX)
    set_source_files_properties(src/bonkr.cpp PROPERTIES LANGUAGE OBJCXX)
    target_link_libraries(bonkr_clap PRIVATE "-framework Cocoa")
  endif()
endif()
set_target_properties(bonkr_clap PROPERTIES
  PREFIX "" OUTPUT_NAME "BONKR" SUFFIX ".clap"
  CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
if(APPLE)
  set_target_properties(bonkr_clap PROPERTIES BUNDLE TRUE BUNDLE_EXTENSION clap SUFFIX "")
endif()
install(TARGETS bonkr_clap BUNDLE DESTINATION clap LIBRARY DESTINATION clap RUNTIME DESTINATION clap)

# --- tests --------------------------------------------------------------------------------------
enable_testing()

function(bonkr_test name)
  add_executable(${name} tests/${name}.cpp ${ARGN})
  target_compile_features(${name} PRIVATE cxx_std_20)
  target_include_directories(${name} PRIVATE "${clap_SOURCE_DIR}/include" src tests)
  target_link_libraries(${name} PRIVATE bonkr_core)
  bonkr_warnings(${name})
  bonkr_enable_assertions(${name})
  bonkr_coverage(${name})
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES TIMEOUT 900)
endfunction()

bonkr_test(dsp_tests)
bonkr_test(physics_tests)
bonkr_test(kit_tests)
bonkr_test(air_tests)
bonkr_test(engine_tests)
# BONKR against recordings of the real instruments: every referenced sound must pass the
# quality gate in references/gate.tsv and may not score worse than references/baseline.tsv.
bonkr_test(reference_tests)
set_tests_properties(reference_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Suites that load the built .clap and drive it through the public CLAP interfaces, as a host does.
function(bonkr_plugin_test name)
  add_executable(${name} tests/${name}.cpp)
  target_compile_features(${name} PRIVATE cxx_std_20)
  target_include_directories(${name} PRIVATE "${clap_SOURCE_DIR}/include" src tests)
  target_link_libraries(${name} PRIVATE ${CMAKE_DL_LIBS})
  bonkr_warnings(${name})
  bonkr_enable_assertions(${name})
  add_test(NAME ${name} COMMAND ${name} $<TARGET_FILE:bonkr_clap>)
  set_tests_properties(${name} PROPERTIES TIMEOUT 600)
endfunction()
bonkr_plugin_test(plugin_smoke)
bonkr_plugin_test(note_tests)
bonkr_plugin_test(state_tests)
# One proof per parameter, in its advertised unit, from audio. Fails if any parameter has none.
bonkr_plugin_test(advertised_tests)
set_tests_properties(advertised_tests PROPERTIES TIMEOUT 1800)
target_link_libraries(advertised_tests PRIVATE bonkr_core)

# The editor without a window: the real page table and widgets against a fake host.
bonkr_test(ui_tests)

# Renders every preset to WAV and measures it against docs/QUALITY.md; a review tool, not a test.
add_executable(bonkr-preset-audit tools/preset_audit.cpp)
target_compile_features(bonkr-preset-audit PRIVATE cxx_std_20)
target_include_directories(bonkr-preset-audit PRIVATE src tests)
target_link_libraries(bonkr-preset-audit PRIVATE bonkr_core)
bonkr_warnings(bonkr-preset-audit)

# BONKR against recordings of the real instruments: extracts the recordings' features and scores
# the model against them (references/README.md); a review tool. reference_tests holds the scores.
add_executable(bonkr-reference tools/reference.cpp)
target_compile_features(bonkr-reference PRIVATE cxx_std_20)
target_include_directories(bonkr-reference PRIVATE src tests tools)
target_link_libraries(bonkr-reference PRIVATE bonkr_core)
bonkr_warnings(bonkr-reference)

# Renders strokes of any sound or kit key straight from the engine, for listening; a review tool,
# not a test.
add_executable(bonkr-note-render tools/note_render.cpp)
target_compile_features(bonkr-note-render PRIVATE cxx_std_20)
target_include_directories(bonkr-note-render PRIVATE src tests)
target_link_libraries(bonkr-note-render PRIVATE bonkr_core)
bonkr_warnings(bonkr-note-render)

if(BONKR_EDITOR AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  # The plug-in side of the editor (host, gestures, relevance, artwork) compiled with the plug-in
  # source and driven without a window. Linux because the backend is chosen at compile time.
  add_executable(frontend_tests tests/frontend_tests.cpp)
  target_compile_features(frontend_tests PRIVATE cxx_std_20)
  target_include_directories(frontend_tests PRIVATE "${clap_SOURCE_DIR}/include" src tests)
  target_compile_definitions(frontend_tests PRIVATE BONKR_EDITOR=1 BONKR_VERSION="${PROJECT_VERSION}")
  target_link_libraries(frontend_tests PRIVATE bonkr_core X11::X11 X11::Xrender PkgConfig::XFT)
  bonkr_warnings(frontend_tests)
  bonkr_enable_assertions(frontend_tests)
  bonkr_coverage(frontend_tests)
  add_test(NAME frontend_tests COMMAND frontend_tests)
  set_tests_properties(frontend_tests PROPERTIES TIMEOUT 120)

  # Offscreen editor renders for layout review; not a test, since it needs an X display.
  add_executable(bonkr-ui-preview tools/ui_preview.cpp)
  target_compile_features(bonkr-ui-preview PRIVATE cxx_std_20)
  target_include_directories(bonkr-ui-preview PRIVATE src)
  target_link_libraries(bonkr-ui-preview PRIVATE X11::X11 X11::Xrender PkgConfig::XFT Threads::Threads)
  bonkr_warnings(bonkr-ui-preview)
endif()
