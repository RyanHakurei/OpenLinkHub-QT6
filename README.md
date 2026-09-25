# OpenLinkHub Qt

A native KDE / Qt 6 frontend for [OpenLinkHub](https://github.com/jurkovic-nikola/OpenLinkHub). It talks to the
OpenLinkHub HTTP API (default `http://127.0.0.1:27003`) and mirrors the web UI: dashboard, devices, RGB, fan curves,
LCD, macros, cluster, and settings.

The app is a Qt Widgets + KDE Frameworks 6 program. It does **not** force a widget style or color scheme, so it follows
your Plasma theme and Kvantum style.

## Requirements

- OpenLinkHub running locally (or reachable over HTTP)
- Qt 6.6+ (Widgets, Network, Svg)
- KDE Frameworks 6: CoreAddons, I18n, XmlGui, Config, ConfigWidgets, WidgetsAddons, IconThemes, ColorScheme, Crash, GuiAddons
- extra-cmake-modules

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
./build/bin/openlinkhub-qt
```

Install (optional):

```bash
cmake --install build
```

## Usage

1. Start the OpenLinkHub service.
2. Launch `openlinkhub-qt`.
3. If the daemon is not on `127.0.0.1:27003`, use **Settings → Configure OpenLinkHub…** (or the Preferences action) to set host and port.

Device control still happens in the OpenLinkHub daemon. This app is a frontend only (`frontend: false` in OpenLinkHub’s
`config.json` can hide the web UI if you only want this client).

## Theme

No application stylesheet or `QApplication::setStyle()` is used. Kvantum, Breeze, and other Qt styles apply through the
normal Qt platform theme.

## License

GPL-3.0-or-later
