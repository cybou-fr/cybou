# Copyright (c) 2026 Stanislav SAVELIEV
# SPDX-License-Identifier: Apache-2.0

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
#  - https://doc.qt.io/qt-6/cmake-manual.html

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOMOC_MOC_OPTIONS "-p${CMAKE_CURRENT_SOURCE_DIR}")
set(CMAKE_AUTORCC OFF)
qt6_add_resources(CYBOU_QRC cybou.qrc)
# CYBOU's native desktop target contains only its own UI and runtime.
add_library(cybou_qt STATIC
  cybouobservationchart.cpp
  cybouobservationchart.h
  cybouapplicationbackend.h
  cyboucoreapplicationadapter.cpp
  cyboucoreapplicationadapter_identity_session.cpp
  cyboucoreapplicationadapter_session_scheduler.cpp
  cyboucoreapplicationadapter_mail.cpp
  cyboucoreapplicationadapter_files.cpp
  cyboucoreapplicationadapter_storage.cpp
  cyboucoreapplicationadapter_internal.h
  cyboucoreapplicationadapter.h
  cyboudesktopcontroller.cpp
  cyboudesktopcontroller.h
  cyboudesktopmodel.cpp
  cyboudesktopmodel.h
  cyboufixturebackend.cpp
  cyboufixturebackend.h
  cyboumainwindow.cpp
  cybouscreenshotharness.cpp
  cybouscreenshotharness.h
  cybouactivity.cpp
  cybouactivity.h
  cyboumainwindow.h
  cybounotifier.h
  cyboutheme.cpp
  cyboutheme.h
  cybouproduct.h
  cybouui.h
  franceoutline.h
  benchmarkreference.h
  benchmarkreference.cpp
  cybouuifixtures.cpp
  cybouuifixtures.h
  pages/diagnosticspage.cpp
  pages/diagnosticspage.h
  pages/networkpage.cpp
  pages/networkpage.h
  pages/networkauthoritypage.cpp
  pages/networkauthoritypage.h
  pages/emailpage.cpp
  pages/emailpage.h
  pages/homepage.cpp
  pages/homepage.h
  pages/mailcompose.cpp
  pages/mailcompose.h
  pages/mailreader.cpp
  pages/mailreader.h
  pages/identitypage.cpp
  pages/identitypage.h
  pages/onboardingview.cpp
  pages/onboardingview.h
  pages/settingspage.cpp
  pages/settingspage.h
  pages/storagepage.cpp
  pages/storagepage.h
  pages/walletpage.cpp
  pages/walletpage.h
  recoveryphrasedialog.cpp
  recoveryphrasedialog.h
  cybouconsoledialog.cpp
  cybouconsoledialog.h
  authorityreview.cpp
  authorityreview.h
  ${CYBOU_QRC}
)
target_compile_definitions(cybou_qt PUBLIC QT_NO_KEYWORDS QT_USE_QSTRINGBUILDER)
target_include_directories(cybou_qt PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>)
target_link_libraries(cybou_qt PUBLIC Qt6::Widgets Qt6::Svg PRIVATE cybou_node OpenSSL::Crypto)

qt_add_translations(cybou_qt
  TS_FILES translations/cybou_fr.ts
  RESOURCE_PREFIX /i18n
)

add_executable(cybou
  main.cpp
  cybouapplication.cpp
  cybouapplication.h
)

cybou_windows_manifest(cybou res/cybou-res.rc)

target_link_libraries(cybou
  cybou_qt
  cybou_cli
)

import_plugins(cybou)
cybou_install(cybou)
if(WIN32)
  set_target_properties(cybou PROPERTIES WIN32_EXECUTABLE TRUE)

  find_program(CYBOU_WINDEPLOYQT_EXECUTABLE
    NAMES windeployqt
    HINTS "${Qt6_DIR}/../../../bin"
    REQUIRED
  )
  # --force: always install the Qt the binary was linked against. Without it
  # windeployqt keeps any DLL that looks newer, e.g. from another Qt build,
  # and cybou.exe fails with "entry point not found".
  add_custom_command(TARGET cybou POST_BUILD
    COMMAND "${CYBOU_WINDEPLOYQT_EXECUTABLE}"
      --release
      --force
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
