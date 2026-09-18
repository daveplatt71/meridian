cmake_minimum_required(VERSION 3.21)
project(Meridian VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

option(MERIDIAN_WITH_LAYER_SHELL
  "Build the opt-in one-output Wayland layer-shell proof"
  OFF)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Quick Qml Test)
qt_add_executable(meridian src/main.cpp src/atlas.cpp src/atlas.h src/solar.h src/clock.h)
target_link_libraries(meridian PRIVATE Qt6::Quick Qt6::Qml)

if(MERIDIAN_WITH_LAYER_SHELL)
  target_compile_definitions(meridian PRIVATE MERIDIAN_WITH_LAYER_SHELL=1)
  enable_language(C)

  find_program(MERIDIAN_WAYLAND_SCANNER
    NAMES wayland-scanner
    DOC "wayland-scanner executable used to generate the layer-shell client protocol")
  if(NOT MERIDIAN_WAYLAND_SCANNER)
    message(FATAL_ERROR
      "MERIDIAN_WITH_LAYER_SHELL=ON requires wayland-scanner, but it was not found. "
      "Install the Wayland development tools or configure with -DMERIDIAN_WITH_LAYER_SHELL=OFF.")
  endif()

  set(MERIDIAN_LAYER_SHELL_XML
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/protocols/wlr-layer-shell-unstable-v1.xml")
  if(NOT EXISTS "${MERIDIAN_LAYER_SHELL_XML}")
    message(FATAL_ERROR
      "MERIDIAN_WITH_LAYER_SHELL=ON requires the pinned protocol XML at "
      "${MERIDIAN_LAYER_SHELL_XML}.")
  endif()

  find_package(PkgConfig QUIET)
  if(NOT PkgConfig_FOUND)
    message(FATAL_ERROR
      "MERIDIAN_WITH_LAYER_SHELL=ON requires pkg-config to locate wayland-client.")
  endif()
  pkg_check_modules(MERIDIAN_WAYLAND_CLIENT QUIET IMPORTED_TARGET wayland-client)
  if(NOT MERIDIAN_WAYLAND_CLIENT_FOUND)
    message(FATAL_ERROR
      "MERIDIAN_WITH_LAYER_SHELL=ON requires the wayland-client development package "
      "(pkg-config module wayland-client).")
  endif()
  pkg_check_modules(MERIDIAN_WAYLAND_PROTOCOLS QUIET wayland-protocols)
  if(NOT MERIDIAN_WAYLAND_PROTOCOLS_FOUND)
    message(FATAL_ERROR
      "MERIDIAN_WITH_LAYER_SHELL=ON requires the wayland-protocols package "
      "(pkg-config module wayland-protocols).")
  endif()
  pkg_get_variable(MERIDIAN_WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
  if(NOT MERIDIAN_WAYLAND_PROTOCOLS_DIR OR
     NOT EXISTS "${MERIDIAN_WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml")
    message(FATAL_ERROR
      "Could not locate stable xdg-shell.xml from wayland-protocols.")
  endif()

  set(MERIDIAN_LAYER_SHELL_GENERATED_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/generated/wayland")
  set(MERIDIAN_LAYER_SHELL_HEADER
    "${MERIDIAN_LAYER_SHELL_GENERATED_DIR}/wlr-layer-shell-unstable-v1-client-protocol.h")
  set(MERIDIAN_LAYER_SHELL_SOURCE
    "${MERIDIAN_LAYER_SHELL_GENERATED_DIR}/wlr-layer-shell-unstable-v1-protocol.c")
  set(MERIDIAN_XDG_SHELL_XML
    "${MERIDIAN_WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml")
  set(MERIDIAN_XDG_SHELL_HEADER
    "${MERIDIAN_LAYER_SHELL_GENERATED_DIR}/xdg-shell-client-protocol.h")
  set(MERIDIAN_XDG_SHELL_SOURCE
    "${MERIDIAN_LAYER_SHELL_GENERATED_DIR}/xdg-shell-protocol.c")

  add_custom_command(
    OUTPUT "${MERIDIAN_XDG_SHELL_HEADER}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${MERIDIAN_LAYER_SHELL_GENERATED_DIR}"
    COMMAND "${MERIDIAN_WAYLAND_SCANNER}" client-header
            "${MERIDIAN_XDG_SHELL_XML}" "${MERIDIAN_XDG_SHELL_HEADER}"
    DEPENDS "${MERIDIAN_XDG_SHELL_XML}"
    VERBATIM
    COMMENT "Generating Wayland xdg-shell client header")

  add_custom_command(
    OUTPUT "${MERIDIAN_XDG_SHELL_SOURCE}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${MERIDIAN_LAYER_SHELL_GENERATED_DIR}"
    COMMAND "${MERIDIAN_WAYLAND_SCANNER}" private-code
            "${MERIDIAN_XDG_SHELL_XML}" "${MERIDIAN_XDG_SHELL_SOURCE}"
    DEPENDS "${MERIDIAN_XDG_SHELL_XML}" "${MERIDIAN_XDG_SHELL_HEADER}"
    VERBATIM
    COMMENT "Generating Wayland xdg-shell client source")

  add_custom_command(
    OUTPUT "${MERIDIAN_LAYER_SHELL_HEADER}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${MERIDIAN_LAYER_SHELL_GENERATED_DIR}"
    COMMAND "${MERIDIAN_WAYLAND_SCANNER}" client-header
            "${MERIDIAN_LAYER_SHELL_XML}" "${MERIDIAN_LAYER_SHELL_HEADER}"
    DEPENDS "${MERIDIAN_LAYER_SHELL_XML}"
    VERBATIM
    COMMENT "Generating Wayland layer-shell client header")

  add_custom_command(
    OUTPUT "${MERIDIAN_LAYER_SHELL_SOURCE}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${MERIDIAN_LAYER_SHELL_GENERATED_DIR}"
    COMMAND "${MERIDIAN_WAYLAND_SCANNER}" private-code
            "${MERIDIAN_LAYER_SHELL_XML}" "${MERIDIAN_LAYER_SHELL_SOURCE}"
    DEPENDS "${MERIDIAN_LAYER_SHELL_XML}" "${MERIDIAN_LAYER_SHELL_HEADER}"
    VERBATIM
    COMMENT "Generating Wayland layer-shell client source")

  add_custom_target(meridian-layer-shell-protocols
    DEPENDS
      "${MERIDIAN_LAYER_SHELL_HEADER}"
      "${MERIDIAN_LAYER_SHELL_SOURCE}"
      "${MERIDIAN_XDG_SHELL_HEADER}"
      "${MERIDIAN_XDG_SHELL_SOURCE}")
  add_dependencies(meridian meridian-layer-shell-protocols)
  add_executable(layer-shell-header-test tests/layer_shell_header.cpp)
  add_dependencies(layer-shell-header-test meridian-layer-shell-protocols)
  target_sources(layer-shell-header-test PRIVATE
    "${MERIDIAN_LAYER_SHELL_SOURCE}"
    "${MERIDIAN_XDG_SHELL_SOURCE}")
  target_include_directories(layer-shell-header-test PRIVATE
    "${MERIDIAN_LAYER_SHELL_GENERATED_DIR}")
  target_link_libraries(layer-shell-header-test PRIVATE
    PkgConfig::MERIDIAN_WAYLAND_CLIENT)
  add_test(NAME layer-shell-header COMMAND layer-shell-header-test)
  target_sources(meridian PRIVATE
    src/layer_shell.cpp
    src/layer_shell.h
    "${MERIDIAN_LAYER_SHELL_HEADER}"
    "${MERIDIAN_LAYER_SHELL_SOURCE}"
    "${MERIDIAN_XDG_SHELL_HEADER}"
    "${MERIDIAN_XDG_SHELL_SOURCE}")
  target_include_directories(meridian PRIVATE
    "${MERIDIAN_LAYER_SHELL_GENERATED_DIR}")
  target_link_libraries(meridian PRIVATE PkgConfig::MERIDIAN_WAYLAND_CLIENT)

  message(STATUS "Meridian layer-shell protocol generation: ENABLED")
  message(STATUS "  wayland-scanner: ${MERIDIAN_WAYLAND_SCANNER}")
  message(STATUS "  protocol XML:    ${MERIDIAN_LAYER_SHELL_XML}")
  message(STATUS "  generated C:     ${MERIDIAN_LAYER_SHELL_SOURCE}")
  message(STATUS "  generated header:${MERIDIAN_LAYER_SHELL_HEADER}")
else()
  message(STATUS "Meridian layer-shell protocol generation: disabled")
endif()

qt_add_resources(meridian assets PREFIX "/" FILES qml/Main.qml qml/MeridianScene.qml assets/land.geojson assets/countries.geojson assets/relief.png)
enable_testing()
add_executable(solar-tests tests/solar.cpp)
target_include_directories(solar-tests PRIVATE src)
target_link_libraries(solar-tests PRIVATE Qt6::Core)
add_test(NAME solar COMMAND solar-tests)
qt_add_executable(ui-tests tests/ui.cpp src/atlas.cpp src/atlas.h src/clock.h src/solar.h)
target_include_directories(ui-tests PRIVATE src)
target_link_libraries(ui-tests PRIVATE Qt6::Quick Qt6::Qml Qt6::Test)
qt_add_resources(ui-tests test-assets PREFIX "/" FILES qml/Main.qml qml/MeridianScene.qml assets/land.geojson assets/countries.geojson assets/relief.png)
add_test(NAME preview-ui COMMAND ui-tests)
set_tests_properties(preview-ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORMTHEME=basic;QT_QUICK_BACKEND=software")
