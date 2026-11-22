#pragma once

namespace tk_installer {

inline constexpr const char* WIN7_INSTALLER_CSS = R"CSS(
/* Classic Windows 7 / Aero Wizard Styling */
window.win7-installer {
    background-color: #F0F0F0;
    color: #000000;
    font-family: "Segoe UI", "Tahoma", "Inter", sans-serif;
    font-size: 9pt;
}

/* Sidebar / Welcome Banner (Deep Classic Blue with Branding) */
box.win7-sidebar {
    background: linear-gradient(to bottom, #002868 0%, #004B97 45%, #002050 100%);
    border-right: 1px solid #001840;
    padding: 24px 16px;
}

label.win7-sidebar-title {
    color: #FFFFFF;
    font-size: 15pt;
    font-weight: bold;
    text-shadow: 1px 1px 2px rgba(0, 0, 0, 0.6);
}

label.win7-sidebar-corp {
    color: #A9D1FF;
    font-size: 8.5pt;
    font-weight: bold;
    letter-spacing: 0.5px;
}

/* Top Banner Header for Step Pages */
box.win7-header-banner {
    background-color: #FFFFFF;
    border-bottom: 1px solid #D0D0D0;
    padding: 10px 18px;
}

label.win7-header-title {
    font-size: 9.5pt;
    font-weight: bold;
    color: #000000;
}

label.win7-header-subtitle {
    font-size: 8pt;
    color: #404040;
    margin-left: 14px;
}

/* Content Area */
box.win7-content {
    background-color: #F0F0F0;
    padding: 16px 20px;
}

/* Bottom Button Control Bar */
box.win7-bottom-bar {
    background-color: #F0F0F0;
    border-top: 1px solid #DFDFDF;
    padding: 12px 16px;
}

/* Classic Windows 7 Wizard Action Buttons */
button.win7-btn {
    background: linear-gradient(to bottom, #F2F2F2 0%, #EBEBEB 50%, #DDDDDD 51%, #CFCFCF 100%);
    border: 1px solid #707070;
    border-radius: 3px;
    padding: 3px 18px;
    min-width: 75px;
    min-height: 23px;
    font-size: 9pt;
    color: #000000;
    box-shadow: inset 0 1px 0 #FFFFFF, 0 1px 1px rgba(0,0,0,0.1);
}

button.win7-btn:hover {
    background: linear-gradient(to bottom, #EAF6FD 0%, #D9F0FC 50%, #BEE6FD 51%, #A7D9F5 100%);
    border-color: #3C7FB1;
    box-shadow: inset 0 1px 0 #FFFFFF, 0 0 3px rgba(60, 127, 177, 0.5);
}

button.win7-btn:active {
    background: linear-gradient(to bottom, #CFE4F1 0%, #BCDBEF 50%, #9BC6E6 51%, #85B5DC 100%);
    border-color: #2C628B;
    box-shadow: inset 0 1px 2px rgba(0,0,0,0.25);
}

button.win7-btn:disabled {
    background: #F4F4F4;
    border-color: #ADB2B5;
    color: #A0A0A0;
    box-shadow: none;
}

button.win7-btn.primary {
    border-color: #0078D7;
}

/* Agreement Text Area (Classic Beveled Inset) */
textview.win7-eula text {
    background-color: #FFFFFF;
    color: #000000;
    border: 1px solid #7F9DB9;
    border-radius: 1px;
    padding: 8px 10px;
    font-family: "Consolas", "Courier New", "Monospace", monospace;
    font-size: 8.5pt;
}

/* Classic Radio and Check Buttons */
checkbutton.win7-check {
    font-size: 9pt;
    color: #000000;
}

checkbutton.win7-check check {
    min-width: 13px;
    min-height: 13px;
    border-radius: 2px;
    border: 1px solid #707070;
    background-color: #FFFFFF;
}

checkbutton.win7-check check:checked {
    background-color: #0078D7;
    border-color: #005499;
}

/* Progress Bar */
progressbar.win7-progress trough {
    background: #E6E6E6;
    border: 1px solid #BCBCBC;
    border-radius: 2px;
    min-height: 16px;
    box-shadow: inset 0 1px 2px rgba(0,0,0,0.2);
}

progressbar.win7-progress progress {
    background: linear-gradient(to bottom, #A1E685 0%, #70C749 50%, #51A62C 51%, #3E8B1F 100%);
    border-radius: 1px;
    border: 1px solid #2B6F12;
}

/* System Group / Path Box */
entry.win7-entry {
    background-color: #FFFFFF;
    border: 1px solid #7F9DB9;
    border-radius: 1px;
    padding: 4px 6px;
    font-size: 9pt;
    color: #000000;
}

/* Corporate Branding Labels */
label.win7-bold {
    font-weight: bold;
    color: #000000;
}

label.win7-dim {
    color: #555555;
    font-size: 8.5pt;
}
)CSS";

inline constexpr const char* EULA_TEXT = R"TXT(TASK KILLER END USER LICENSE AGREEMENT (EULA)
DEVELOPED AND DISTRIBUTED BY POWERSTACK CORP.

IMPORTANT: PLEASE READ THIS SOFTWARE LICENSE AGREEMENT CAREFULLY BEFORE INSTALLING OR USING THE "TASK KILLER" PRODUCTIVITY SUITE.

1. GRANT OF LICENSE
PowerStack Corp. grants you a personal, worldwide, non-exclusive, and perpetual license to install and run Task Killer on your computer systems for personal, educational, or commercial organizational productivity purposes.

2. OWNERSHIP & INTELLECTUAL PROPERTY
Task Killer, including all source code, algorithms, visual styles, sound effects, and documentation, is the proprietary property of PowerStack Corp. and its contributors. All rights not expressly granted herein are reserved.

3. ACCEPTABLE USE & TIME EXECUTION
Task Killer is designed to maintain high productivity by enforcing deadlines and task completion. By installing this software, you acknowledge that automated sound notifications and alerts are part of the intended functional design.

4. PRIVACY & LOCAL DATA STORAGE
PowerStack Corp. prioritizes customer privacy:
- All task schedules, notes, checklists, and statistical records remain 100% stored locally on your device database.
- No personal user content or telemetry is transmitted to remote cloud servers without your explicit action.

5. DISCLAIMER OF WARRANTIES
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT. IN NO EVENT SHALL POWERSTACK CORP. BE LIABLE FOR ANY CLAIM, DAMAGES, OR OTHER LIABILITY ARISING FROM THE USE OF THIS SOFTWARE.

6. TERMINATION
This agreement is effective until terminated. You may terminate it at any time by uninstalling the application and removing its associated packages.

Copyright (C) 2026 PowerStack Corp. All rights reserved.
)TXT";

} // namespace tk_installer
