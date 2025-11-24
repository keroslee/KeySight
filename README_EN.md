# KeySight - Keyboard & Mouse Heatmap Statistics Tool

[![Windows](https://img.shields.io/badge/Windows-10%2B-blue?logo=windows)](https://www.microsoft.com/windows)
[![Visual Studio](https://img.shields.io/badge/Visual%20Studio-2022-purple?logo=visual-studio)](https://visualstudio.microsoft.com/)
[![License](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[中文](README.md)

KeySight is a lightweight Windows desktop application for recording and analyzing keyboard and mouse usage, presenting key press frequency through beautifully visualized heatmaps.

## ✨ Key Features

- **📊 Real-time Statistics**: Automatically tracks and counts keyboard and mouse key presses.
- **🎨 Heatmap Visualization**: Interactive HTML-based heatmap that intuitively displays key press frequency.
- **🔔 Real-time Display**: Shows floating hints when keys are pressed, supports special keys like the Windows logo key. <mark style="background: #ff0; color: #dc3545; border: 4px solid #dc3545; padding: 2px 6px; border-radius: 15px;">⚠️ Be mindful of password security</mark>
- **⚙️ Highly Configurable**: Supports startup with boot (requires Administrator privileges), hotkey controls, statistics toggle, and more.
- **🔒 Privacy Protection**: All data is stored locally (only key press counts), no personal information is uploaded.
- **🌐 Cross-platform Viewing**: Supports viewing statistics locally or online.

## 🖼️ Preview

![Configuration Menu](menu.png)
*Click the system tray icon to open the configuration menu*
![Keyboard Heatmap](heatmap.png)
*Interactive keyboard heatmap, redder colors indicate higher usage frequency*

## 🚀 Quick Start

### System Requirements

- Windows 10 or later

### Installation

#### Method 1: Download Pre-compiled Version
1. Go to the [Releases page](https://github.com/keroslee/KeySight/releases)
2. Download the latest version of `KeySight.zip`
3. Extract to any directory
4. Run `KeySight.exe`

#### Method 2: Compile from Source
```bash
# Clone the repository
git clone https://github.com/keroslee/KeySight.git
cd KeySight
```
Open KeySight.sln with Visual Studio 2022.

### Usage

1. **First Run**: The application icon will appear in the system tray after launch.
2. **View Statistics**: Left/Right-click the tray icon → "View Statistics"
3. **Configuration Options**:
   - **Enable Statistics**: Start/Stop recording key press data.
   - **Show Keys**: Display real-time hints when keys are pressed.
   - **Start on Boot**: Set the program to launch with the system.
   - **Set Hotkey**: Configure show/hide hotkeys.

## ⚙️ Configuration Files
The program generates the following files in its directory:

- `KeySight.ini` - Program configuration. Delete this file to restore default settings.
- `KeySight.txt` - Statistical data (cumulative counts). Delete this file to reset statistics.
- `KeySight.log` - Runtime log.

### Configuration Example
```ini
[Config]
boot=0
stats=1
show=1
hotkey=0
lang=en-US
view_url=https://keros.im/kb.htm
[en-US]
lan_app=Key Statistics
lan_enable=Enable
lan_disable=Disable
lan_onboot=Start on Boot
lan_stat=Enable Statistics
lan_show=Show Keys
lan_hotkey=Set Hotkey
lan_exit=Exit
lan_total=Total Keystrokes:
lan_hotkey_tip=Press hotkey to set / Del to remove
lan_hotkey_title=Set Hotkey
lan_suc=Success
lan_fai=Failed
lan_dup=Application is already running, check system tray
lan_view=View Statistics
lan_viewing=Opening webpage to view key heatmap
lan_view_failed=Failed to open webpage
lan_view_success=Webpage opened successfully
```

## 🌐 Language Configuration
KeySight supports a multi-language interface. It can automatically switch based on the user's system language and also supports manual language configuration.

### 支持的语言
- **Simplified Chinese** (zh-CN) - Chinese
- **English** (en-US) - English
- **System Auto-detection** - Automatically selects based on system language settings
- **Adding Other Languages** - Run the program once to initialize the INI file, modify lang, add the corresponding configuration block, then restart the program.
- **Manually Specifying Language** - Modify lang in the INI file and restart the program.

## 🛠️ Development Guide

### Build Environment
- Visual Studio 2022
- Windows SDK
- C++17 Standard

### Dependencies
- Windows API
- GDI+ (Graphics Drawing)
- Raw Input API (Input Capture)

### Compilation Steps
1. Open KeySight.sln with Visual Studio 2022.
2. Set the build configuration to Release.
3. Build Solution (Ctrl+Shift+B).

## 🎯 Feature Details

### Keyboard Statistics
- Records all standard keyboard keys.
- Distinguishes left/right modifier keys (Shift, Ctrl, Alt, Win).
- Supports function keys and the numeric keypad.

### Mouse Statistics
- Left/Right button click statistics.
- Scroll wheel usage statistics.
- Side button (X1/X2) statistics.

### Visualization Features
- Gradient heatmap colors.
- Hover to display specific counts.
- Responsive design adapts to different screens.
- Dynamically hides unused numeric keypad areas.

## 🔗 Listary5 Integration

KeySight extends the trigger mechanism for the Listary5 search bar.

### Integration Features
- **⚡ Seamless Switching**: Press the left ❖ key to directly trigger the Listary5 search bar. The right ❖ key functionality remains unchanged.
- **🔄 Automatic Hotkey Detection**: Automatically reads Listary5's configuration file to obtain its hotkey settings.

### How It Works
KeySight obtains Listary5's hotkey settings by parsing its configuration file (Preferences.json):

```json
{
  "hotkey": {
    "main": 12345
  }
}
```

## 🔧 Troubleshooting

### Common Issues

**Q: The program fails to start or exits immediately.**
A: Check if another instance is already running. The program supports only a single instance.

**Q: Unable to start on boot.**
A: Enabling this feature requires Administrator privileges because it needs to create a KeySight task in the Windows Task Scheduler. Also, check the program file path for spaces or special characters.

**Q: The heatmap page fails to open.**
A: Check your default browser settings and internet connection. Alternatively, try pressing a few keys/mouse buttons and retry. On first use, the KeySight.txt file might not be generated yet (it starts writing only after recording more than 10 key presses).

**Q: False positive by antivirus software.**
A: Add the program to your antivirus software's whitelist.

### Log File
The program runtime log is saved in KeySight.log. Check this file if you encounter problems.

## 📄 License
This project is licensed under the **GNU General Public License v3.0** - see the [LICENSE](https://www.gnu.org/licenses/gpl-3.0.html) for details.

## 🙏 Acknowledgments
- Thanks to [LC044/TraceBoard](https://github.com/LC044/TraceBoard/blob/main/server/static/index.html)
- Thanks to AI and Myself👻

## 📞 Contact
- Project Homepage: [https://github.com/keroslee/KeySight](https://github.com/keroslee/KeySight)
- Issue Tracker: [GitHub Issues](https://github.com/keroslee/KeySight/issues)
- Email: keros@keros.im

---
**Note**: This tool is intended solely for personal productivity analysis and habit improvement. Do not use it to monitor others' devices. By using this tool, you agree to assume all related responsibilities.