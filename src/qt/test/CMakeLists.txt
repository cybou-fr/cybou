# Copyright (c) 2024-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

set(CMAKE_AUTOMOC_MOC_OPTIONS "-p${CMAKE_CURRENT_SOURCE_DIR}")

# Exercise the CYBOU-native window/model/controller without Bitcoin's Qt or
# node test harness.
add_executable(test_cybou-native-qt
  cybou_native_qt_tests.cpp
  cyboushelltests.cpp
)

add_windows_application_manifest(test_cybou-native-qt)

target_link_libraries(test_cybou-native-qt
  cybou_qt
  Qt6::Test
)

import_plugins(test_cybou-native-qt)

add_test(NAME test_cybou-native-qt
  COMMAND test_cybou-native-qt
)
if(WIN32 AND VCPKG_TARGET_TRIPLET)
  set(plugin_path "$<SHELL_PATH:$<PATH:GET_PARENT_PATH,$<PATH:GET_PARENT_PATH,$<TARGET_PROPERTY:Qt6::QWindowsIntegrationPlugin,LOCATION_$<CONFIG>>>>>")
  set_property(TEST test_cybou-native-qt APPEND PROPERTY
    ENVIRONMENT_MODIFICATION
      QT_QPA_PLATFORM=set:windows
      QT_PLUGIN_PATH=set:${plugin_path}
  )
  unset(plugin_path)
endif()

install_binary_component(test_cybou-native-qt INTERNAL)
