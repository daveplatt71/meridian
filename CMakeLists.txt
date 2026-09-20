cmake_minimum_required(VERSION 3.21)
project(Omaridian VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

option(OMARIDIAN_WITH_LAYER_SHELL
  "Build the opt-in Wayland layer-shell wallpaper renderer"
  OFF)
option(OMARIDIAN_WITH_FRACTIONAL_SCALE
  "Enable optional fractional-scale/viewporter support in the layer-shell renderer"
  OFF)
if(OMARIDIAN_WITH_FRACTIONAL_SCALE AND NOT OMARIDIAN_WITH_LAYER_SHELL)
  message(FATAL_ERROR "OMARIDIAN_WITH_FRACTIONAL_SCALE=ON requires OMARIDIAN_WITH_LAYER_SHELL=ON.")
endif()

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Quick Qml Test)
qt_add_executable(omaridian src/main.cpp src/atlas.cpp src/atlas.h src/solar.h src/clock.h)
target_link_libraries(omaridian PRIVATE Qt6::Quick Qt6::Qml)

if(OMARIDIAN_WITH_LAYER_SHELL)
  target_compile_definitions(omaridian PRIVATE OMARIDIAN_WITH_LAYER_SHELL=1)
  enable_language(C)

  find_program(OMARIDIAN_WAYLAND_SCANNER
    NAMES wayland-scanner
    DOC "wayland-scanner executable used to generate the layer-shell client protocol")
  if(NOT OMARIDIAN_WAYLAND_SCANNER)
    message(FATAL_ERROR
      "OMARIDIAN_WITH_LAYER_SHELL=ON requires wayland-scanner, but it was not found. "
      "Install the Wayland development tools or configure with -DOMARIDIAN_WITH_LAYER_SHELL=OFF.")
  endif()

  set(OMARIDIAN_LAYER_SHELL_XML
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/protocols/wlr-layer-shell-unstable-v1.xml")
  if(NOT EXISTS "${OMARIDIAN_LAYER_SHELL_XML}")
    message(FATAL_ERROR
      "OMARIDIAN_WITH_LAYER_SHELL=ON requires the pinned protocol XML at "
      "${OMARIDIAN_LAYER_SHELL_XML}.")
  endif()

  find_package(PkgConfig QUIET)
  if(NOT PkgConfig_FOUND)
    message(FATAL_ERROR
      "OMARIDIAN_WITH_LAYER_SHELL=ON requires pkg-config to locate wayland-client.")
  endif()
  pkg_check_modules(OMARIDIAN_WAYLAND_CLIENT QUIET IMPORTED_TARGET wayland-client)
  if(NOT OMARIDIAN_WAYLAND_CLIENT_FOUND)
    message(FATAL_ERROR
      "OMARIDIAN_WITH_LAYER_SHELL=ON requires the wayland-client development package "
      "(pkg-config module wayland-client).")
  endif()
  pkg_check_modules(OMARIDIAN_WAYLAND_PROTOCOLS QUIET wayland-protocols)
  if(NOT OMARIDIAN_WAYLAND_PROTOCOLS_FOUND)
    message(FATAL_ERROR
      "OMARIDIAN_WITH_LAYER_SHELL=ON requires the wayland-protocols package "
      "(pkg-config module wayland-protocols).")
  endif()
  pkg_get_variable(OMARIDIAN_WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
  if(NOT OMARIDIAN_WAYLAND_PROTOCOLS_DIR OR
     NOT EXISTS "${OMARIDIAN_WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml")
    message(FATAL_ERROR
      "Could not locate stable xdg-shell.xml from wayland-protocols.")
  endif()

  set(OMARIDIAN_LAYER_SHELL_GENERATED_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/generated/wayland")
  set(OMARIDIAN_LAYER_SHELL_HEADER
    "${OMARIDIAN_LAYER_SHELL_GENERATED_DIR}/wlr-layer-shell-unstable-v1-client-protocol.h")
  set(OMARIDIAN_LAYER_SHELL_SOURCE
    "${OMARIDIAN_LAYER_SHELL_GENERATED_DIR}/wlr-layer-shell-unstable-v1-protocol.c")
  set(OMARIDIAN_XDG_SHELL_XML
    "${OMARIDIAN_WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml")
  set(OMARIDIAN_XDG_SHELL_HEADER
    "${OMARIDIAN_LAYER_SHELL_GENERATED_DIR}/xdg-shell-client-protocol.h")
  set(OMARIDIAN_XDG_SHELL_SOURCE
    "${OMARIDIAN_LAYER_SHELL_GENERATED_DIR}/xdg-shell-protocol.c")

  add_custom_command(
    OUTPUT "${OMARIDIAN_XDG_SHELL_HEADER}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${OMARIDIAN_LAYER_SHELL_GENERATED_DIR}"
    COMMAND "${OMARIDIAN_WAYLAND_SCANNER}" client-header
            "${OMARIDIAN_XDG_SHELL_XML}" "${OMARIDIAN_XDG_SHELL_HEADER}"
    DEPENDS "${OMARIDIAN_XDG_SHELL_XML}"
    VERBATIM
    COMMENT "Generating Wayland xdg-shell client header")

  add_custom_command(
    OUTPUT "${OMARIDIAN_XDG_SHELL_SOURCE}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${OMARIDIAN_LAYER_SHELL_GENERATED_DIR}"
    COMMAND "${OMARIDIAN_WAYLAND_SCANNER}" private-code
            "${OMARIDIAN_XDG_SHELL_XML}" "${OMARIDIAN_XDG_SHELL_SOURCE}"
    DEPENDS "${OMARIDIAN_XDG_SHELL_XML}" "${OMARIDIAN_XDG_SHELL_HEADER}"
    VERBATIM
    COMMENT "Generating Wayland xdg-shell client source")

  add_custom_command(
    OUTPUT "${OMARIDIAN_LAYER_SHELL_HEADER}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${OMARIDIAN_LAYER_SHELL_GENERATED_DIR}"
    COMMAND "${OMARIDIAN_WAYLAND_SCANNER}" client-header
            "${OMARIDIAN_LAYER_SHELL_XML}" "${OMARIDIAN_LAYER_SHELL_HEADER}"
    DEPENDS "${OMARIDIAN_LAYER_SHELL_XML}"
    VERBATIM
    COMMENT "Generating Wayland layer-shell client header")

  add_custom_command(
    OUTPUT "${OMARIDIAN_LAYER_SHELL_SOURCE}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${OMARIDIAN_LAYER_SHELL_GENERATED_DIR}"
    COMMAND "${OMARIDIAN_WAYLAND_SCANNER}" private-code
            "${OMARIDIAN_LAYER_SHELL_XML}" "${OMARIDIAN_LAYER_SHELL_SOURCE}"
    DEPENDS "${OMARIDIAN_LAYER_SHELL_XML}" "${OMARIDIAN_LAYER_SHELL_HEADER}"
    VERBATIM
    COMMENT "Generating Wayland layer-shell client source")

  add_custom_target(omaridian-layer-shell-protocols
    DEPENDS
      "${OMARIDIAN_LAYER_SHELL_HEADER}"
      "${OMARIDIAN_LAYER_SHELL_SOURCE}"
      "${OMARIDIAN_XDG_SHELL_HEADER}"
      "${OMARIDIAN_XDG_SHELL_SOURCE}")
  add_dependencies(omaridian omaridian-layer-shell-protocols)
  add_executable(layer-shell-header-test tests/layer_shell_header.cpp)
  add_dependencies(layer-shell-header-test omaridian-layer-shell-protocols)
  target_sources(layer-shell-header-test PRIVATE
    "${OMARIDIAN_LAYER_SHELL_SOURCE}"
    "${OMARIDIAN_XDG_SHELL_SOURCE}")
  target_include_directories(layer-shell-header-test PRIVATE
    "${OMARIDIAN_LAYER_SHELL_GENERATED_DIR}")
  target_link_libraries(layer-shell-header-test PRIVATE
    PkgConfig::OMARIDIAN_WAYLAND_CLIENT)
  add_test(NAME layer-shell-header COMMAND layer-shell-header-test)
  target_sources(omaridian PRIVATE
    src/layer_shell.cpp
    src/layer_shell.h
    "${OMARIDIAN_LAYER_SHELL_HEADER}"
    "${OMARIDIAN_LAYER_SHELL_SOURCE}"
    "${OMARIDIAN_XDG_SHELL_HEADER}"
    "${OMARIDIAN_XDG_SHELL_SOURCE}")
  target_include_directories(omaridian PRIVATE
    "${OMARIDIAN_LAYER_SHELL_GENERATED_DIR}")
  target_link_libraries(omaridian PRIVATE PkgConfig::OMARIDIAN_WAYLAND_CLIENT)

  if(OMARIDIAN_WITH_FRACTIONAL_SCALE)
    foreach(protocol IN ITEMS fractional-scale-v1 viewporter)
      if(protocol STREQUAL "fractional-scale-v1")
        set(protocol_xml "${OMARIDIAN_WAYLAND_PROTOCOLS_DIR}/staging/fractional-scale/fractional-scale-v1.xml")
      else()
        set(protocol_xml "${OMARIDIAN_WAYLAND_PROTOCOLS_DIR}/stable/viewporter/viewporter.xml")
      endif()
      if(NOT EXISTS "${protocol_xml}")
        message(FATAL_ERROR
          "OMARIDIAN_WITH_FRACTIONAL_SCALE=ON requires ${protocol_xml}. "
          "Install a wayland-protocols package containing this protocol or disable fractional scaling.")
      endif()
      set(protocol_header "${OMARIDIAN_LAYER_SHELL_GENERATED_DIR}/${protocol}-client-protocol.h")
      set(protocol_source "${OMARIDIAN_LAYER_SHELL_GENERATED_DIR}/${protocol}-protocol.c")
      add_custom_command(
        OUTPUT "${protocol_header}" "${protocol_source}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${OMARIDIAN_LAYER_SHELL_GENERATED_DIR}"
        COMMAND "${OMARIDIAN_WAYLAND_SCANNER}" client-header "${protocol_xml}" "${protocol_header}"
        COMMAND "${OMARIDIAN_WAYLAND_SCANNER}" private-code "${protocol_xml}" "${protocol_source}"
        DEPENDS "${protocol_xml}"
        VERBATIM
        COMMENT "Generating Wayland ${protocol} client bindings")
      list(APPEND OMARIDIAN_FRACTIONAL_SCALE_BINDINGS "${protocol_header}" "${protocol_source}")
    endforeach()
    add_custom_target(omaridian-fractional-scale-protocols DEPENDS ${OMARIDIAN_FRACTIONAL_SCALE_BINDINGS})
    foreach(target IN ITEMS omaridian layer-shell-header-test)
      add_dependencies(${target} omaridian-fractional-scale-protocols)
      target_sources(${target} PRIVATE ${OMARIDIAN_FRACTIONAL_SCALE_BINDINGS})
      target_compile_definitions(${target} PRIVATE OMARIDIAN_WITH_FRACTIONAL_SCALE=1)
    endforeach()
  endif()
  message(STATUS "Omaridian fractional-scale support: ${OMARIDIAN_WITH_FRACTIONAL_SCALE}")

  message(STATUS "Omaridian layer-shell protocol generation: ENABLED")
  message(STATUS "  wayland-scanner: ${OMARIDIAN_WAYLAND_SCANNER}")
  message(STATUS "  protocol XML:    ${OMARIDIAN_LAYER_SHELL_XML}")
  message(STATUS "  generated C:     ${OMARIDIAN_LAYER_SHELL_SOURCE}")
  message(STATUS "  generated header:${OMARIDIAN_LAYER_SHELL_HEADER}")
else()
  message(STATUS "Omaridian layer-shell protocol generation: disabled")
endif()

qt_add_resources(omaridian assets PREFIX "/" FILES qml/Main.qml qml/OmaridianScene.qml assets/land.geojson assets/countries.geojson assets/relief.png)
enable_testing()
add_executable(solar-tests tests/solar.cpp)
target_include_directories(solar-tests PRIVATE src)
target_link_libraries(solar-tests PRIVATE Qt6::Core)
add_test(NAME solar COMMAND solar-tests)
qt_add_executable(ui-tests tests/ui.cpp src/atlas.cpp src/atlas.h src/clock.h src/solar.h)
target_include_directories(ui-tests PRIVATE src)
target_link_libraries(ui-tests PRIVATE Qt6::Quick Qt6::Qml Qt6::Test)
qt_add_resources(ui-tests test-assets PREFIX "/" FILES qml/Main.qml qml/OmaridianScene.qml assets/land.geojson assets/countries.geojson assets/relief.png)
add_test(NAME preview-ui COMMAND ui-tests)
set_tests_properties(preview-ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORMTHEME=basic;QT_QUICK_BACKEND=software")
add_test(NAME omarchy-integration COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/test-omarchy-integration.sh")
