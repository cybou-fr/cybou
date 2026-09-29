# Copyright (c) 2023-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  enable_language(OBJCXX)
  set(CMAKE_OBJCXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
  set(CMAKE_OBJCXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
  set(CMAKE_OBJCXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  set(CMAKE_OBJCXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL}")
  string(APPEND CMAKE_OBJCXX_COMPILE_OBJECT " ${APPEND_CPPFLAGS} ${APPEND_CXXFLAGS}")
endif()

get_target_property(qt_lib_type Qt6::Core TYPE)

function(import_plugins target)
  if(qt_lib_type STREQUAL "STATIC_LIBRARY")
    set(plugins Qt6::QMinimalIntegrationPlugin)
    if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|FreeBSD)$")
      list(APPEND plugins Qt6::QXcbIntegrationPlugin)
    elseif(WIN32)
      list(APPEND plugins Qt6::QWindowsIntegrationPlugin Qt6::QModernWindowsStylePlugin)
    elseif(APPLE)
      list(APPEND plugins Qt6::QCocoaIntegrationPlugin Qt6::QMacStylePlugin)
    endif()
    qt6_import_plugins(${target}
      INCLUDE ${plugins}
      EXCLUDE_BY_TYPE
        accessiblebridge
        platforms
        platforms_darwin
        xcbglintegrations
        platformthemes
        platforminputcontexts
        generic
        iconengines
        imageformats
        egldeviceintegrations
        styles
        networkaccess
        networkinformation
        tls
    )
  endif()
endfunction()

# For Qt-specific commands and variables, please consult:
#  - https://cmake.org/cmake/help/latest/manual/cmake-qt.7.html
#  - https://doc.qt.io/qt-5/cmake-manual.html

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOMOC_MOC_OPTIONS "-p${CMAKE_CURRENT_SOURCE_DIR}")
set(CMAKE_AUTORCC OFF)
qt6_add_resources(CYBOU_QRC cybou.qrc)
# CYBOU's native desktop target contains only its own UI and runtime.
add_library(cybou_qt STATIC
  cyboudesktopcontroller.cpp
  cyboudesktopcontroller.h
  cyboudesktopmodel.cpp
  cyboudesktopmodel.h
  cyboumainwindow.cpp
  cyboumainwindow.h
  cyboustrip.h
  cyboutheme.cpp
  cyboutheme.h
  cybouproduct.h
  cybouui.h
  pages/backuppage.cpp
  pages/backuppage.h
  pages/emailpage.cpp
  pages/emailpage.h
  pages/homepage.cpp
  pages/homepage.h
  pages/identitypage.cpp
  pages/identitypage.h
  pages/networkpage.cpp
  pages/networkpage.h
  pages/settingspage.cpp
  pages/settingspage.h
  pages/storagepage.cpp
  pages/storagepage.h
  pages/walletpage.cpp
  pages/walletpage.h
  recoveryphrasedialog.cpp
  recoveryphrasedialog.h
  ${CYBOU_QRC}
)
target_compile_definitions(cybou_qt PUBLIC QT_NO_KEYWORDS QT_USE_QSTRINGBUILDER)
target_include_directories(cybou_qt PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>)
target_link_libraries(cybou_qt PUBLIC Qt6::Widgets Qt6::Svg PRIVATE cybou_node)

add_executable(cybou
  main.cpp
  cybouapplication.cpp
  cybouapplication.h
)

add_windows_resources(cybou res/cybou-res.rc)
add_windows_application_manifest(cybou)

target_link_libraries(cybou
  cybou_qt
)

import_plugins(cybou)
install_binary_component(cybou)
if(WIN32)
  set_target_properties(cybou PROPERTIES WIN32_EXECUTABLE TRUE)

  find_program(CYBOU_WINDEPLOYQT_EXECUTABLE
    NAMES windeployqt
    HINTS "${Qt6_DIR}/../../../bin"
    REQUIRED
  )
  add_custom_command(TARGET cybou POST_BUILD
    COMMAND "${CYBOU_WINDEPLOYQT_EXECUTABLE}"
      --release
      --compiler-runtime
      --no-translations
      "$<TARGET_FILE:cybou>"
    COMMENT "Deploying the Qt runtime for CYBOU"
    VERBATIM
  )
endif()

if(BUILD_GUI_TESTS)
  add_subdirectory(test)
endif()
