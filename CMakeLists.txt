cmake_minimum_required(VERSION 3.16)

project(openlinkhub-qt VERSION 0.1.0 LANGUAGES CXX)

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
)

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
    src/widgets/CardGrid.cpp
    src/pages/DashboardPage.cpp
    src/pages/DevicePage.cpp
    src/pages/SettingsPage.cpp
    src/pages/RgbEditorPage.cpp
    src/pages/TemperaturePage.cpp
    src/pages/MacrosPage.cpp
    src/pages/LcdPage.cpp
    src/pages/ClusterPage.cpp
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
)

target_sources(openlinkhub-qt PRIVATE src/openlinkhub-qt.qrc)

install(TARGETS openlinkhub-qt ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(PROGRAMS data/pw.freyja.OpenLinkHub.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES data/pw.freyja.OpenLinkHub.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR})

ecm_install_icons(ICONS
    icons/sc-apps-openlinkhub-qt.svg
    DESTINATION ${KDE_INSTALL_ICONDIR}
    THEME hicolor
)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
