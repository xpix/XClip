#pragma once

// Icon
#define IDI_XCLIP               100
#define IDI_XCLIP_SMALL         101

// Tray menu commands
#define IDM_TRAY_FIRST          1000
#define IDM_TRAY_SETTINGS       1001
#define IDM_TRAY_SEARCH         1002
#define IDM_TRAY_MANAGE         1003
#define IDM_TRAY_CLEAR_HISTORY  1004
#define IDM_TRAY_CLEAR_CURRENT  1005
#define IDM_TRAY_AUTOSTART      1006
#define IDM_TRAY_ABOUT          1007
#define IDM_TRAY_EXIT           1008

// History popup item base ID (items are IDM_CLIP_BASE + index)
#define IDM_CLIP_BASE           2000
#define IDM_CLIP_MAX            2999

// Edit/Delete context in popup
#define IDM_CLIP_EDIT           3001
#define IDM_CLIP_DELETE         3002
#define IDM_CLIP_DELETE_ALL     3003

// Config dialog
#define IDD_CONFIG              4000
#define IDD_CONFIG_GENERAL      4001
#define IDD_CONFIG_POPUP        4002
#define IDD_CONFIG_HOTKEYS      4003

// Config controls - General
#define IDC_HISTORY_SIZE        4100
#define IDC_HISTORY_SIZE_SPIN   4101
#define IDC_AUTOSTART           4102
#define IDC_IGNORE_DUPLICATES   4103
#define IDC_SAVE_HISTORY        4104
#define IDC_PURGE_BITMAPS       4105

// Config controls - Popup
#define IDC_POPUP_POSITION      4200
#define IDC_SHOW_PREVIEW        4201
#define IDC_MAX_DISPLAY_LEN     4202
#define IDC_SHOW_ACCELERATORS   4203

// Config controls - Hotkeys
#define IDC_HOTKEY_POPUP        4300

// Search dialog
#define IDD_SEARCH              5000
#define IDC_SEARCH_EDIT         5001
#define IDC_SEARCH_LIST         5002

// About dialog
#define IDD_ABOUT               6000

// Custom messages
#define WM_TRAYICON             (WM_USER + 1)
#define WM_CLIPBOARDUPDATE_CUSTOM (WM_USER + 2)

// Hotkey IDs
#define HOTKEY_POPUP            1
