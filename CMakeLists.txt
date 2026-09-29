cmake_minimum_required(VERSION 3.16)

project(openlinkhub-qt VERSION 1.0.0 LANGUAGES CXX)

set(QT_MIN_VERSION "6.6.0")
set(KF_MIN_VERSION "6.0.0")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMInstallIcons)
include(ECMSetupVersion)
include(FeatureSummary)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    Svg
    SvgWidgets
    DBus
)

find_package(KSysGuard REQUIRED)

find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    CoreAddons
    I18n
    XmlGui
    Config
    ConfigWidgets
    WidgetsAddons
    IconThemes
    ColorScheme
    Crash
    GuiAddons
    WindowSystem
    DBusAddons
    GlobalAccel
)

ecm_setup_version(PROJECT
    VARIABLE_PREFIX OPENLINKHUB_QT
    VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/openlinkhub-qt-version.h"
)

add_executable(openlinkhub-qt
    src/main.cpp
    src/app/AppSettings.cpp
    src/app/MainWindow.cpp
    src/app/ConnectionDialog.cpp
    src/api/ApiClient.cpp
    src/api/IconCache.cpp
    src/i18n/HubI18n.cpp
    src/widgets/Sidebar.cpp
    src/widgets/TemperatureBar.cpp
    src/widgets/Card.cpp
    src/widgets/CardGrid.cpp
    src/widgets/FanCurveChart.cpp
    src/pages/DashboardPage.cpp
    src/pages/DevicePage.cpp
    src/pages/SettingsPage.cpp
    src/pages/RgbEditorPage.cpp
    src/pages/TemperaturePage.cpp
    src/pages/MacrosPage.cpp
    src/pages/LcdPage.cpp
    src/pages/ClusterPage.cpp
    src/app/SensorService.cpp
    src/app/DaemonService.cpp
    src/app/TrayController.cpp
    src/app/SpeedHold.cpp
)

target_include_directories(openlinkhub-qt PRIVATE
    src
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(openlinkhub-qt
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Svg
    Qt6::SvgWidgets
    Qt6::DBus
    KF6::CoreAddons
    KF6::I18n
    KF6::XmlGui
    KF6::ConfigCore
    KF6::ConfigWidgets
    KF6::WidgetsAddons
    KF6::IconThemes
    KF6::ColorScheme
    KF6::Crash
    KF6::GuiAddons
    KF6::WindowSystem
    KF6::DBusAddons
    KF6::GlobalAccel
)

target_sources(openlinkhub-qt PRIVATE src/openlinkhub-qt.qrc)

add_executable(openlinkhub-sensorsd
    src/sensors/sensorsd.cpp
    src/sensors/SensorSnapshot.cpp
    src/api/ApiClient.cpp
    src/app/AppSettings.cpp
)

target_include_directories(openlinkhub-sensorsd PRIVATE src)

target_link_libraries(openlinkhub-sensorsd
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    KF6::ConfigCore
)

add_library(ksystemstats_plugin_openlinkhub MODULE
    src/sensors/OpenLinkHubStatsPlugin.cpp
    src/sensors/SensorSnapshot.cpp
)

target_include_directories(ksystemstats_plugin_openlinkhub PRIVATE src)

target_link_libraries(ksystemstats_plugin_openlinkhub
    Qt6::Core
    Qt6::Gui
    KF6::CoreAddons
    KF6::I18n
    KSysGuard::SystemStats
)

configure_file(
    data/openlinkhub-sensors.service.in
    ${CMAKE_CURRENT_BINARY_DIR}/openlinkhub-sensors.service
    @ONLY
)

install(TARGETS openlinkhub-qt ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(TARGETS openlinkhub-sensorsd ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(TARGETS ksystemstats_plugin_openlinkhub DESTINATION ${KDE_INSTALL_PLUGINDIR}/ksystemstats)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/openlinkhub-sensors.service DESTINATION ${KDE_INSTALL_SYSTEMDUSERUNITDIR})
install(PROGRAMS data/pw.freyja.OpenLinkHub.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES data/pw.freyja.OpenLinkHub.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR})

ecm_install_icons(ICONS
    icons/sc-apps-openlinkhub-qt.svg
    icons/sc-apps-openlinkhub-qt-symbolic.svg
    DESTINATION ${KDE_INSTALL_ICONDIR}
    THEME hicolor
)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
