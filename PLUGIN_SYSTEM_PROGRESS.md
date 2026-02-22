# Telegram Desktop Plugin System - Progress Documentation

## Overview
This document tracks the progress of implementing a Lua-based plugin system for Telegram Desktop, allowing users to load custom Lua scripts that can hook into various Telegram events.

## Completed Tasks

### 1. Compile Commands Generation ✅
- **Status**: Complete
- **Details**: 
  - `compile_commands.json` is generated in the project root via `do_cmake_config.ps1`
  - The script configures CMake with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`
  - File is automatically copied from `out-clangd/compile_commands.json` to project root
  - clangd should now be able to provide proper code completion and navigation

### 2. Lua Integration ✅
- **Status**: Complete
- **Details**:
  - Lua 5.4.7 source code downloaded from GitHub (https://github.com/lua/lua)
  - Located at: `Telegram/ThirdParty/lua/`
  - CMake integration in `cmake/external/lua/CMakeLists.txt`
  - Linked to Telegram target in `Telegram/CMakeLists.txt` as `desktop-app::external_lua`
  - All Lua source files are compiled as a static library

### 3. Plugin System Architecture ✅
- **Status**: Complete
- **Files Created/Modified**:
  - `Telegram/SourceFiles/plugins/plugin_manager.h` - Plugin manager interface
  - `Telegram/SourceFiles/plugins/plugin_manager.cpp` - Plugin manager implementation
  - `Telegram/SourceFiles/boxes/plugin_box.h` - UI box for plugin management
  - `Telegram/SourceFiles/boxes/plugin_box.cpp` - Plugin UI implementation
  - `Telegram/SourceFiles/window/window_main_menu.cpp` - Menu integration (line 709-713)

### 4. Plugin Manager Features ✅
- **Status**: Complete
- **Features**:
  - Singleton `Plugins::Manager` instance
  - Load/unload Lua scripts from file paths
  - Per-plugin enable/disable toggle
  - Plugin state management (loaded/unloaded)
  - Event hook system for Telegram events
  - Logging system per plugin
  - Session integration (hooks into `Main::Session`)

### 5. UI Implementation ✅
- **Status**: Complete
- **Features**:
  - "Plugins" menu item added between "Saved Messages" and "Settings"
  - Plugin management dialog with:
    - List of loaded plugins
    - Enable/disable toggle per plugin
    - "View Logs" button per plugin
    - "Remove" button per plugin
    - "Load Script..." button to add new plugins
  - Log viewer dialog showing:
    - Timestamped log entries
    - "Clear" button to clear logs
    - Real-time log updates

### 6. Event Hooks ✅
- **Status**: Complete
- **Implemented Hooks**:
  - `onLoad()` - Called when plugin is loaded
  - `onUnload()` - Called when plugin is unloaded
  - `onNewMessage(chatName, senderName, text, isOutgoing)` - Called on new messages
- **Integration**:
  - Hooks into `Main::Session::changes().messageUpdates()`
  - Filters for `Data::MessageUpdate::Flag::NewAdded`
  - Extracts chat name, sender name, message text, and direction

### 7. Demo Plugin ✅
- **Status**: Complete
- **File**: `demo_message_logger.lua`
- **Features**:
  - Logs all new messages with direction (IN/OUT)
  - Shows chat name, sender name, and message text
  - Includes onLoad/onUnload handlers

## Build Status

### Compilation
- **Plugin Code**: ✅ Compiles successfully
- **Lua Integration**: ✅ Compiles successfully
- **Full Build**: ✅ **BUILD SUCCESSFUL**
  - Fixed compiler heap space issues by:
    - Reducing parallel jobs to half of CPU cores (minimum 1)
    - Switching to Release build configuration
    - Adding `/Zm2000` compiler option to increase precompiled header memory limit
  - Fixed missing `menuIconBots` by using `menuIconManage` instead
  - Fixed missing `WINRT_IMPL_RoOriginateLanguageException` by adding alias in `base_windows_winrt.cpp`
  - Binary location: `out-clangd/Release/Telegram.exe`

### Build Commands
```powershell
# Configure CMake and generate compile_commands.json
powershell -ExecutionPolicy Bypass -File do_cmake_config.ps1

# Build the project
powershell -ExecutionPolicy Bypass -File do_build.ps1
```

### Run project
```powershell
powershell -ExecutionPolicy Bypass -File do_run.ps1
```

## File Structure

```
tdesktop/
├── Telegram/
│   ├── SourceFiles/
│   │   ├── plugins/
│   │   │   ├── plugin_manager.h
│   │   │   └── plugin_manager.cpp
│   │   ├── boxes/
│   │   │   ├── plugin_box.h
│   │   │   └── plugin_box.cpp
│   │   └── window/
│   │       └── window_main_menu.cpp (modified)
│   └── ThirdParty/
│       └── lua/ (Lua 5.4.7 source)
├── cmake/
│   └── external/
│       └── lua/
│           └── CMakeLists.txt
├── demo_message_logger.lua
└── compile_commands.json
```

## Usage

### Loading a Plugin
1. Open Telegram Desktop
2. Click the menu (hamburger icon)
3. Click "Plugins" (between "Saved Messages" and "Settings")
4. Click "Load Script..."
5. Select a `.lua` file
6. Plugin will be loaded and enabled by default

### Viewing Plugin Logs
1. Open Plugins dialog
2. Click "View Logs" under any plugin
3. Logs show timestamped entries
4. Click "Clear" to clear logs

### Enabling/Disabling Plugins
- Toggle the switch next to each plugin name to enable/disable

### Removing Plugins
- Click "Remove" under any plugin to unload it

## Lua Plugin API

### Available Functions
- `log(message)` - Log a message to the plugin's log
- `print(...)` - Same as log (prints to plugin log)

### Hook Functions (Optional)
Plugins can define these functions to receive events:

```lua
function onLoad()
    -- Called when plugin is loaded
end

function onUnload()
    -- Called when plugin is unloaded
end

function onNewMessage(chatName, senderName, text, isOutgoing)
    -- Called when a new message arrives
    -- chatName: Name of the chat/channel
    -- senderName: Name of the message sender
    -- text: Message text content
    -- isOutgoing: true if message is outgoing, false if incoming
end
```

## Build Fixes Applied

1. **Compiler Heap Space Issues** ✅ Fixed
   - Reduced parallel jobs to `[Environment]::ProcessorCount / 2` (minimum 1)
   - Switched build configuration from Debug to Release
   - Added `/Zm2000` compiler option to increase precompiled header memory limit

2. **Missing Menu Icon** ✅ Fixed
   - Changed `st::menuIconBots` to `st::menuIconManage` (icon doesn't exist)

3. **WinRT Linker Error** ✅ Fixed
   - Added alias `WINRT_IMPL_RoOriginateLanguageException` in `base_windows_winrt.cpp`
   - Maps to existing `WINRT_RoOriginateLanguageException` function

## Known Issues

1. **Plugin Persistence**: Plugins are not saved between sessions
   - **Future Enhancement**: Save plugin list to settings

3. **Error Handling**: Limited error recovery if Lua script has syntax errors
   - **Current**: Errors are logged to plugin logs
   - **Future Enhancement**: Better error display in UI

## Future Enhancements

1. **Additional Event Hooks**:
   - `onMessageEdited`
   - `onMessageDeleted`
   - `onChatOpened`
   - `onUserStatusChanged`
   - `onCallStarted`
   - etc.

2. **Plugin Configuration**:
   - Per-plugin settings/configuration UI
   - Persistent plugin state

3. **Plugin Marketplace**:
   - Browse and install plugins from a repository
   - Plugin versioning

4. **Sandboxing**:
   - Security restrictions for plugins
   - Permission system

5. **Plugin Persistence**:
   - Save loaded plugins between sessions
   - Auto-load plugins on startup

## Testing

### Build Status
- ✅ **Build completed successfully**
- Build time: ~6-7 minutes
- Binary: `out-clangd/Release/Telegram.exe`

### Manual Testing Steps
1. ✅ Build the application - **COMPLETE**
2. Run Telegram Desktop from `out-clangd/Release/Telegram.exe`
3. Open Plugins menu (between "Saved Messages" and "Settings")
4. Load `demo_message_logger.lua` (or any `.lua` file)
5. Send/receive messages
6. Check plugin logs to verify message logging works

## Notes for LLM Continuation

If continuing this work:
- All plugin system code is in `Telegram/SourceFiles/plugins/` and `Telegram/SourceFiles/boxes/plugin_box.*`
- Lua source is in `Telegram/ThirdParty/lua/`
- Menu integration is in `Telegram/SourceFiles/window/window_main_menu.cpp` around line 709
- Plugin manager is initialized in `Telegram/SourceFiles/main/main_session.cpp` (line 132)
- The build system uses CMake with Ninja generator
- Build directory is `out-clangd/`
- Use `do_cmake_config.ps1` to regenerate compile_commands.json
- Use `do_build.ps1` to build (may need to reduce parallel jobs for MSVC heap issues)
