# Qt WorldBuilder (Zero Hour)

A modern **Qt** port of the *Command & Conquer: Generals — Zero Hour* **WorldBuilder** map editor:
a full MFC→Qt inversion of the original tool, keeping the classic workflow while replacing the
aging MFC UI with a native Qt front end.

### ⬇️ [**Download the latest release**](https://github.com/triatomic/worldbuilderQT/releases/latest)

Grab `WorldBuilderZH-Qt-*.zip` from the [releases page](https://github.com/triatomic/worldbuilderQT/releases/latest),
unzip it, and run **`WorldBuilderZH.exe`** from inside a Zero Hour install. The Qt runtime is
bundled — no separate Qt install is required to run it.

## Running

WorldBuilder loads game data from its own folder, so it must sit inside a Zero Hour install (it
`SetCurrentDirectory`s to its own exe folder at startup). Unzip the release into your Zero Hour
directory (or copy the exe + the bundled Qt DLLs there) and launch **`WorldBuilderZH.exe`**.

You must own the game. The C&C Ultimate Collection is available on
[EA App](https://www.ea.com/en-gb/games/command-and-conquer/command-and-conquer-the-ultimate-collection/buy/pc)
or [Steam](https://store.steampowered.com/bundle/39394/Command__Conquer_The_Ultimate_Collection/).

## Building

WorldBuilder is a **32-bit** app built with **Visual Studio 2022 (MSVC v14x x86)**, **Qt 5.15.2
32-bit (`msvc2019`)**, **Ninja**, and **CMake 3.25+**. The Qt build is **off by default** — a plain
configure gives you the classic MFC WorldBuilder; enable it with
`-DRTS_ENABLE_WORLDBUILDER_QT=ON`.

See **[QT-BUILD.md](QT-BUILD.md)** for the full step-by-step setup, the Qt install options, the
CMake preset commands, runtime deployment (`windeployqt`), and the optional keyboard/focus debug
facility.

Quick version (inside an x86 MSVC environment, `vcvarsall.bat x86`):

```
cmake --preset win32-internal ^
  -DRTS_ENABLE_WORLDBUILDER_QT=ON ^
  -DCMAKE_PREFIX_PATH="C:/Qt/5.15.2/msvc2019"

cmake --build --preset win32-internal --target z_worldbuilder
```

Output: `build/win32-internal/GeneralsMD/Release/WorldBuilderZH.exe`

## About this repository

This is a fork of the open-sourced *Command & Conquer: Generals + Zero Hour* source. The work here
is focused on the **WorldBuilder Qt inversion** under `Tools/WorldBuilder`; the rest of the game
engine and tools sources are carried along unchanged as the WorldBuilder's build dependencies.

Building the full game/tools from source requires several third-party SDKs (DirectX, STLport,
Miles, Bink, GameSpy, ZLib, and others) that are not included in this repository. Those are only
needed for a from-scratch source build — the [release download](https://github.com/triatomic/worldbuilderQT/releases/latest)
is a ready-to-run binary.

## License

This repository and its contents are licensed under the GPL v3 license, with additional terms
applied. Please see [LICENSE.md](LICENSE.md) for details.
