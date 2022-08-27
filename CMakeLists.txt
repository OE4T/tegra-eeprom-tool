# SPDX-License-Identifier: MIT
# Copyright (c) 2022, Matthew Madison

cmake_minimum_required(VERSION 3.8)
cmake_policy(SET CMP0048 NEW)

project(tegra_eeprom_tool LANGUAGES C VERSION 1.4.1)

include(GNUInstallDirs)

set(CMAKE_C_STANDARD 11)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEDIT REQUIRED IMPORTED_TARGET libedit)

configure_file(tegra-eeprom.pc.in tegra-eeprom.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/tegra-eeprom.pc DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

set(EEPROM_HEADERS boardspec.h cvm.h eeprom.h)
add_library(tegra-eeprom SHARED eeprom.c cvm.c boardspec.c ${EEPROM_HEADERS})
set_target_properties(tegra-eeprom PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION 1)
target_link_libraries(tegra-eeprom PUBLIC PkgConfig::LIBEDIT)
install(TARGETS tegra-eeprom LIBRARY)
install(FILES ${EEPROM_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tegra-eeprom)

add_executable(tegra-eeprom-tool tegra-eeprom-tool.c)
target_link_libraries(tegra-eeprom-tool PUBLIC tegra-eeprom PkgConfig::LIBEDIT)

add_executable(tegra-boardspec tegra-boardspec.c)
target_link_libraries(tegra-boardspec PUBLIC tegra-eeprom PkgConfig::LIBEDIT)

install(TARGETS tegra-eeprom tegra-boardspec tegra-eeprom-tool RUNTIME)
