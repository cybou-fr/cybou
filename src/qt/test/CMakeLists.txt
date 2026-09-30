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
  # The live adapter test drives real core services on an authority runtime.
  cybou_node
  Boost::headers
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

if(WIN32 AND CYBOU_WINDEPLOYQT_EXECUTABLE)
  # Same Qt runtime as the linked Qt (including Qt6Test); see src/qt/CMakeLists.txt.
  add_custom_command(TARGET test_cybou-native-qt POST_BUILD
    COMMAND "${CYBOU_WINDEPLOYQT_EXECUTABLE}"
      --release
      --force
      --compiler-runtime
      --no-translations
      --include-plugins qoffscreen
      "$<TARGET_FILE:test_cybou-native-qt>"
    COMMENT "Deploying the Qt runtime for the desktop tests"
    VERBATIM
  )
endif()
