cmake_minimum_required(VERSION 3.21)
project(kk-sip VERSION 0.2.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Sql Network LinguistTools)
find_package(PkgConfig REQUIRED)

# Prefer the static PJSIP built by scripts/build-pjsip.sh, fall back to a
# system libpjproject (e.g. the AUR package).
set(KKSIP_PJSIP_PREFIX "${CMAKE_SOURCE_DIR}/.deps/pjsip" CACHE PATH "PJSIP install prefix")
if(EXISTS "${KKSIP_PJSIP_PREFIX}/lib/pkgconfig/libpjproject.pc")
    # pkg-config searches <prefix>/lib/pkgconfig of CMAKE_PREFIX_PATH entries first
    # (PKG_CONFIG_PATH separators differ between Linux and Windows).
    list(PREPEND CMAKE_PREFIX_PATH "${KKSIP_PJSIP_PREFIX}")
endif()
pkg_check_modules(PJSIP REQUIRED IMPORTED_TARGET libpjproject)

qt_standard_project_setup()

# Everything except main() lives in an object library so the smoke test can reuse it.
add_library(kk-sip-core OBJECT
    src/core/Settings.h src/core/Settings.cpp
    src/core/Database.h src/core/Database.cpp
    src/core/SipUri.h src/core/SipUri.cpp
    src/core/SingleInstance.h src/core/SingleInstance.cpp
    src/core/MicrosipImport.h src/core/MicrosipImport.cpp
    src/core/Autostart.h src/core/Autostart.cpp
    src/core/WindowPlacement.h src/core/WindowPlacement.cpp
    src/sip/SipEngine.h src/sip/SipEngine.cpp
    src/ui/MainWindow.h src/ui/MainWindow.cpp
    src/ui/DialerTab.h src/ui/DialerTab.cpp
    src/ui/HistoryTab.h src/ui/HistoryTab.cpp
    src/ui/ContactsTab.h src/ui/ContactsTab.cpp
    src/ui/CallPanel.h src/ui/CallPanel.cpp
    src/ui/IncomingDialog.h src/ui/IncomingDialog.cpp
    src/ui/SettingsDialog.h src/ui/SettingsDialog.cpp
    src/ui/AccountDialog.h src/ui/AccountDialog.cpp
    src/ui/Icons.h src/ui/Icons.cpp
    src/ui/Theme.h src/ui/Theme.cpp
    src/ui/ListDelegate.h src/ui/ListDelegate.cpp
    src/ui/AccountSwitcher.h src/ui/AccountSwitcher.cpp
    src/ui/ElidedLabel.h
    src/core/UpdateChecker.h src/core/UpdateChecker.cpp
    resources/resources.qrc
)
target_include_directories(kk-sip-core PUBLIC src)
target_compile_definitions(kk-sip-core PUBLIC KKSIP_VERSION="${PROJECT_VERSION}")
target_link_libraries(kk-sip-core PUBLIC Qt6::Widgets Qt6::Sql Qt6::Network PkgConfig::PJSIP)
# The bundled PJSIP is static, so its private deps (opus, ssl, asound...) must be linked too.
set(KKSIP_PJSIP_LIBS ${PJSIP_STATIC_LDFLAGS})
if(APPLE)
    # pkg-config splits "-framework CoreAudio" in two; CMake would read "CoreAudio" as a library.
    string(REPLACE "-framework;" "-framework " KKSIP_PJSIP_LIBS "${KKSIP_PJSIP_LIBS}")
endif()
target_link_libraries(kk-sip-core PUBLIC ${KKSIP_PJSIP_LIBS})

if(WIN32)
    enable_language(RC)
    configure_file(packaging/windows/kk-sip.rc.in kk-sip.rc @ONLY)
    qt_add_executable(kk-sip WIN32 src/main.cpp ${CMAKE_CURRENT_BINARY_DIR}/kk-sip.rc)
    # Static libstdc++/libgcc: fewer MinGW runtime DLLs to ship.
    target_link_options(kk-sip PRIVATE -static-libgcc -static-libstdc++)
elseif(APPLE)
    set_source_files_properties(resources/icons/kk-sip.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    qt_add_executable(kk-sip MACOSX_BUNDLE src/main.cpp resources/icons/kk-sip.icns)
    set_target_properties(kk-sip PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/packaging/macos/Info.plist.in)
    # Only the SVG plugins use QtSvg. macdeployqt can't follow their @rpath across Homebrew
    # kegs, but bundles QtSvg when the app links it, and the plugins then find it there.
    find_package(Qt6 REQUIRED COMPONENTS Svg)
    target_link_libraries(kk-sip PRIVATE Qt6::Svg)
else()
    qt_add_executable(kk-sip src/main.cpp)
endif()
target_link_libraries(kk-sip PRIVATE kk-sip-core)

if(Qt6_VERSION VERSION_GREATER_EQUAL 6.7)
    qt_add_translations(kk-sip-core
        TS_FILES translations/kk-sip_ru.ts translations/kk-sip_uk.ts
        SOURCE_TARGETS kk-sip kk-sip-core
        RESOURCE_PREFIX /i18n
    )
else()
    # Qt 6.4-6.6 (Ubuntu 24.04, Debian 12): no SOURCE_TARGETS; lupdate then misses main.cpp only.
    qt_add_translations(kk-sip-core
        TS_FILES translations/kk-sip_ru.ts translations/kk-sip_uk.ts
        RESOURCE_PREFIX /i18n
    )
endif()

option(KKSIP_BUILD_TESTS "Build the smoke test (needs a SIP server, see tests/README.md)" OFF)
if(KKSIP_BUILD_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(kk-sip-smoke tests/smoke_test.cpp)
    target_link_libraries(kk-sip-smoke PRIVATE kk-sip-core Qt6::Test)
endif()

# Windows and macOS packages are put together by packaging/windows/build-windows.sh
# and packaging/macos/build-macos.sh.
if(WIN32 OR APPLE)
    return()
endif()

include(GNUInstallDirs)
install(TARGETS kk-sip RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/kk-sip.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES resources/icons/kk-sip.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
foreach(size 16 22 24 32 48 64 128 256 512)
    install(FILES resources/icons/hicolor/${size}x${size}/kk-sip.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps)
endforeach()
install(FILES packaging/kk-sip.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
install(FILES LICENSE THIRD-PARTY.md DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/kk-sip)

# .deb via `cpack -G DEB` (packaging/linux/build-ubuntu.sh)
set(CPACK_PACKAGE_NAME kk-sip)
set(CPACK_PACKAGE_VENDOR "Konstantin Kochervrin")
set(CPACK_PACKAGE_CONTACT "Konstantin Kochervrin <k.ko4evrin@gmail.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Minimal MicroSIP-style SIP softphone")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/kochevrin/kk-sip")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION comm)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
# Plugins are loaded at runtime, shlibdeps can't see them.
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6svg6, libqt6sql6-sqlite, qt6-qpa-plugins")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "qt6-translations-l10n")
include(CPack)

