# XClip

A lightweight Windows clipboard manager that lives in the system tray and keeps a history of your clipboard entries.

## Features

- **Clipboard History** – Stores text, images, and file lists (configurable max entries)
- **Hotkey Popup** – Press `Ctrl+Shift+V` to open a popup menu at the caret or mouse position
- **Search** – Quickly search through clipboard history
- **Duplicate Detection** – Optionally ignores duplicate entries
- **Persistent History** – Saves history to disk between sessions
- **Autostart** – Optional Windows startup integration
- **Single Instance** – Prevents multiple copies from running
- **System Tray** – Minimal footprint with tray icon and context menu
- **Configuration Dialog** – Adjust all settings via a GUI

## Requirements

- Windows 10 or later
- Visual Studio 2019+ (or any C++17 capable MSVC toolchain)
- CMake 3.16+

## Build

```powershell
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

The compiled binary will be in `build/Release/XClip.exe`.

## Usage

1. Run `XClip.exe` – it appears as a tray icon.
2. Copy anything to the clipboard as usual.
3. Press **Ctrl+Shift+V** to open the clipboard history popup and select a previous entry.
4. Right-click the tray icon for settings, search, and more options.

## Configuration

Settings are stored in an INI file next to the executable. You can change:

| Setting | Default | Description |
|---------|---------|-------------|
| Max History Size | 25 | Number of entries to keep |
| Ignore Duplicates | Yes | Skip duplicate clipboard content |
| Save History | Yes | Persist history across restarts |
| Purge Bitmaps on Exit | No | Free bitmap memory on close |
| Popup Position | Caret | Show popup at caret or mouse |
| Show Preview Tooltips | Yes | Tooltip previews in popup menu |
| Hotkey | Ctrl+Shift+V | Keyboard shortcut for popup |

## Project Structure

```
src/          – Application source code
resources/    – Icons, manifest, resource scripts
build/        – CMake build output (not committed)
```

## License

MIT
