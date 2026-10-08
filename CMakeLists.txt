cmake_minimum_required(VERSION 3.21)
project(kk-sip VERSION 0.1.0 LANGUAGES CXX)

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
    set(ENV{PKG_CONFIG_PATH} "${KKSIP_PJSIP_PREFIX}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
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
    resources/resources.qrc
)
target_include_directories(kk-sip-core PUBLIC src)
target_compile_definitions(kk-sip-core PUBLIC KKSIP_VERSION="${PROJECT_VERSION}")
target_link_libraries(kk-sip-core PUBLIC Qt6::Widgets Qt6::Sql Qt6::Network PkgConfig::PJSIP)
# The bundled PJSIP is static, so its private deps (opus, ssl, asound...) must be linked too.
target_link_libraries(kk-sip-core PUBLIC ${PJSIP_STATIC_LDFLAGS})

qt_add_executable(kk-sip src/main.cpp)
target_link_libraries(kk-sip PRIVATE kk-sip-core)

qt_add_translations(kk-sip-core
    TS_FILES translations/kk-sip_ru.ts
    SOURCE_TARGETS kk-sip kk-sip-core
    RESOURCE_PREFIX /i18n
)

option(KKSIP_BUILD_TESTS "Build the smoke test (needs a SIP server, see tests/README.md)" OFF)
if(KKSIP_BUILD_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(kk-sip-smoke tests/smoke_test.cpp)
    target_link_libraries(kk-sip-smoke PRIVATE kk-sip-core Qt6::Test)
endif()

include(GNUInstallDirs)
install(TARGETS kk-sip RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/kk-sip.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES resources/icons/kk-sip.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
