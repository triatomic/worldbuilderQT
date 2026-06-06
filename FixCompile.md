# VS2022 Compile Fixes for WorldBuilderZH

These fixes address VC6-era C++ patterns that don't compile under modern MSVC (VS2022).

## Fix 1: ScriptDialog.h — Illegal qualified names in member declaration (C4596)

**File:** `GeneralsMD/Code/Tools/WorldBuilder/include/ScriptDialog.h` lines 165-166

**Before:**
```cpp
AsciiString ScriptDialog::incrementStringNumber(const AsciiString& input);
void ScriptDialog::applySmartCopyIncrement(Script* pScr);
```

**After:**
```cpp
AsciiString incrementStringNumber(const AsciiString& input);
void applySmartCopyIncrement(Script* pScr);
```

VC6 allowed redundant class qualification inside the class body. Modern MSVC rejects it.

## Fix 2: teamsdialog.cpp — Unqualified `exception` in catch handler (C2061/C2310)

**File:** `GeneralsMD/Code/Tools/WorldBuilder/src/teamsdialog.cpp` line 804

**Before:**
```cpp
} catch(exception)  {
```

**After:**
```cpp
} catch(std::exception&)  {
```

VC6 allowed unqualified `exception` and catch-by-value. Modern MSVC requires `std::` and catch-by-reference is correct practice.

## Build Command

### Primary: Ninja preset (matches `CMakePresets.json`)

The project's intended build uses the CMake presets (Ninja Multi-Config). Use the `win32-internal`
preset. Ninja must be on PATH and the **x86** MSVC environment loaded, so run it through
`vcvarsall.bat x86` (Ninja ships with VS):

```
cmake --preset win32-internal
cmd /c "\"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat\" x86 && cmake --build --preset win32-internal --target z_worldbuilder"
```

Output: `build/win32-internal/GeneralsMD/Release/WorldBuilderZH.exe`

Prefer this — Ninja's dependency tracking is reliable. The VS generator below has produced silent
no-op "up to date" builds (exe not relinked); if you use it, verify the exe mtime/CRC actually changed.

### Alternative: VS2022 generator

```
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32 -DRTS_BUILD_ZEROHOUR=ON -DRTS_BUILD_GENERALS=OFF -DRTS_BUILD_ZEROHOUR_TOOLS=ON -DRTS_BUILD_GENERALS_TOOLS=OFF -DRTS_BUILD_OPTION_INTERNAL=ON
cmake --build build --target z_worldbuilder --config Release
```

Output: `build/GeneralsMD/Release/WorldBuilderZH.exe`
