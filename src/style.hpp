#pragma once

namespace tk {

constexpr const char* APP_CSS = R"CSS(
* {
    font-family: "SF Pro Display", "SF Pro Text", "Inter", system-ui, sans-serif;
}

window.tk-window {
    background-color: #0B0B0D;
    color: #F5F5F7;
}

headerbar.tk-header {
    background-color: #0B0B0D;
    border: none;
    box-shadow: none;
    min-height: 44px;
    padding: 4px 12px 0 12px;
}

headerbar.tk-header windowcontrols button {
    min-width: 24px;
    min-height: 24px;
    border-radius: 999px;
    color: #8E8E93;
    background: transparent;
}

headerbar.tk-header windowcontrols button:hover {
    background-color: #2C2C2E;
    color: #F5F5F7;
}

box.tk-sidebar {
    background-color: #1C1C1E;
    border-radius: 14px;
    border: 1px solid #2A2A2D;
    margin: 2px 0 12px 12px;
    padding: 12px 8px;
}

button.tk-nav {
    background: transparent;
    border: none;
    box-shadow: none;
    border-radius: 9px;
    padding: 8px 10px;
    margin: 1px 0;
    color: #AEAEB2;
    font-size: 13px;
    font-weight: 500;
}

button.tk-nav:hover {
    background-color: #262629;
    color: #F5F5F7;
}

button.tk-nav.active {
    background-color: #323236;
    color: #F5F5F7;
    font-weight: 600;
}

label.tk-nav-count {
    font-size: 12px;
    font-weight: 500;
    color: #8E8E93;
    font-variant-numeric: tabular-nums;
}

button.tk-nav.active label.tk-nav-count {
    color: #D1D1D6;
}

label.tk-view-title {
    font-size: 24px;
    font-weight: 700;
    letter-spacing: -0.02em;
    color: #F5F5F7;
}

menubutton.tk-add-btn > button {
    background-color: #2C2C2E;
    color: #F5F5F7;
    border: 1px solid #3A3A3D;
    border-radius: 999px;
    min-width: 30px;
    min-height: 30px;
    padding: 0 10px;
    font-size: 17px;
    font-weight: 400;
    box-shadow: none;
}

menubutton.tk-add-btn > button:hover {
    background-color: #3A3A3D;
    border-color: #48484C;
}

list.tk-list {
    background: transparent;
}

list.tk-list > row {
    background: transparent;
    padding: 3px 0;
}

list.tk-list > row:hover {
    background: transparent;
}

box.tk-row {
    background-color: #121215;
    border: 1px solid #1C1C20;
    border-radius: 12px;
    padding: 12px 15px;
    box-shadow: 0 2px 8px rgba(0, 0, 0, 0.35);
    transition: background-color 120ms ease, border-color 120ms ease, box-shadow 120ms ease;
}

box.tk-row:hover {
    background-color: #17171C;
    border-color: #26262C;
    box-shadow: 0 4px 14px rgba(0, 0, 0, 0.55);
}

checkbutton.tk-check check {
    min-width: 18px;
    min-height: 18px;
    border-radius: 999px;
    border: 1.5px solid #636366;
    background-color: transparent;
    color: #141416;
    box-shadow: none;
    transition: all 120ms ease;
}

checkbutton.tk-check check:hover {
    border-color: #D1D1D6;
    background-color: rgba(255, 255, 255, 0.06);
}

checkbutton.tk-check check:checked {
    background-color: #E5E5EA;
    border-color: #E5E5EA;
    color: #141416;
}

label.tk-task-title {
    font-size: 14px;
    font-weight: 500;
    color: #F5F5F7;
}

label.tk-task-time {
    font-size: 13px;
    font-weight: 500;
    color: #8E8E93;
    font-variant-numeric: tabular-nums;
}

button.tk-del-btn {
    background: transparent;
    border: none;
    box-shadow: none;
    border-radius: 999px;
    min-width: 24px;
    min-height: 24px;
    padding: 0;
    color: #636366;
    font-size: 14px;
    opacity: 0.28;
    transition: opacity 120ms ease, background-color 120ms ease, color 120ms ease;
}

box.tk-row:hover button.tk-del-btn,
button.tk-del-btn:focus {
    opacity: 1;
}

button.tk-del-btn:hover {
    background-color: #2C2C2E;
    color: #F5F5F7;
}

label.tk-empty {
    font-size: 14px;
    font-weight: 500;
    color: #636366;
    margin-top: 48px;
}

popover.tk-popover > contents {
    background-color: #202023;
    border: 1px solid #36363A;
    border-radius: 16px;
    padding: 14px;
    color: #F5F5F7;
}

entry.tk-entry {
    background-color: #161618;
    border: 1px solid #303034;
    border-radius: 10px;
    color: #F5F5F7;
    padding: 8px 12px;
    font-size: 13px;
    min-height: 32px;
    caret-color: #F5F5F7;
}

entry.tk-entry:focus-within {
    border-color: #8E8E93;
}

box.tk-seg {
    background-color: #161618;
    border: 1px solid #2C2C30;
    border-radius: 10px;
    padding: 3px;
}

button.tk-seg-btn {
    background: transparent;
    border: none;
    box-shadow: none;
    border-radius: 7px;
    padding: 5px 8px;
    color: #8E8E93;
}

button.tk-seg-btn:hover {
    color: #D1D1D6;
}

button.tk-seg-btn.active {
    background-color: #2E2E32;
    color: #F5F5F7;
}

label.tk-seg-name {
    font-size: 11px;
    font-weight: 600;
}

label.tk-seg-val {
    font-size: 11px;
    font-weight: 500;
    color: #8E8E93;
    font-variant-numeric: tabular-nums;
}

button.tk-seg-btn.active label.tk-seg-val {
    color: #D1D1D6;
}

calendar.tk-cal {
    background-color: #161618;
    color: #F5F5F7;
    border: 1px solid #2C2C30;
    border-radius: 12px;
    padding: 6px;
    font-size: 12px;
}

calendar.tk-cal > header {
    color: #F5F5F7;
    font-weight: 600;
    padding-bottom: 4px;
}

calendar.tk-cal > header button {
    background: transparent;
    border: none;
    box-shadow: none;
    border-radius: 999px;
    color: #AEAEB2;
    min-width: 24px;
    min-height: 24px;
}

calendar.tk-cal > header button:hover {
    background-color: #2C2C2E;
    color: #F5F5F7;
}

calendar.tk-cal > grid > label.day-name {
    color: #636366;
    font-size: 10px;
    font-weight: 600;
}

calendar.tk-cal > grid > label.day-number {
    border-radius: 999px;
    padding: 3px;
    font-size: 12px;
    font-variant-numeric: tabular-nums;
}

calendar.tk-cal > grid > label.day-number:hover {
    background-color: #262629;
}

calendar.tk-cal > grid > label.day-number:selected {
    background-color: #E5E5EA;
    color: #141416;
    font-weight: 700;
}

calendar.tk-cal > grid > label.day-number.other-month {
    color: #48484C;
}

calendar.tk-cal > grid > label.day-number.today {
    border: 1px solid #48484C;
}

box.tk-clock-box {
    background-color: #161618;
    border: 1px solid #2C2C30;
    border-radius: 12px;
    padding: 8px 10px;
}

button.tk-clock-num {
    background-color: #232326;
    border: 1px solid transparent;
    border-radius: 8px;
    padding: 2px 8px;
    min-width: 38px;
    color: #AEAEB2;
    font-size: 18px;
    font-weight: 600;
    font-variant-numeric: tabular-nums;
    box-shadow: none;
}

button.tk-clock-num.active {
    background-color: #323236;
    border-color: #525256;
    color: #F5F5F7;
}

label.tk-clock-colon {
    font-size: 18px;
    font-weight: 600;
    color: #636366;
}

button.tk-ampm {
    background-color: #232326;
    border: 1px solid #303034;
    border-radius: 7px;
    padding: 2px 7px;
    font-size: 10px;
    font-weight: 600;
    color: #AEAEB2;
    box-shadow: none;
}

button.tk-ampm:hover {
    background-color: #2C2C30;
    color: #F5F5F7;
}

label.tk-form-err {
    font-size: 11px;
    color: #AEAEB2;
}

button.tk-btn-cancel {
    background: transparent;
    border: 1px solid #3A3A3D;
    border-radius: 8px;
    color: #AEAEB2;
    padding: 6px 14px;
    font-size: 12px;
    font-weight: 500;
    box-shadow: none;
}

button.tk-btn-cancel:hover {
    background-color: #2C2C2E;
    color: #F5F5F7;
}

button.tk-btn-primary {
    background-color: #E5E5EA;
    border: none;
    border-radius: 8px;
    color: #141416;
    padding: 6px 16px;
    font-size: 12px;
    font-weight: 600;
    box-shadow: none;
}

button.tk-btn-primary:hover {
    background-color: #F5F5F7;
}

label.tk-app-title {
    font-size: 13px;
    font-weight: 600;
    letter-spacing: -0.01em;
    color: #F5F5F7;
}

box.tk-stat-card {
    background-color: #121215;
    border: 1px solid #1C1C20;
    border-radius: 14px;
    padding: 14px 16px;
}

button.tk-period-arrow {
    background-color: #1C1C1E;
    border: 1px solid #2A2A2D;
    border-radius: 8px;
    color: #AEAEB2;
    min-width: 28px;
    min-height: 28px;
    padding: 0 6px;
    box-shadow: none;
}

button.tk-period-arrow:hover {
    background-color: #262629;
    color: #F5F5F7;
}

menubutton.tk-period-date > button {
    background-color: #1C1C1E;
    border: 1px solid #2A2A2D;
    border-radius: 8px;
    color: #F5F5F7;
    padding: 4px 12px;
    font-size: 12px;
    font-weight: 600;
    font-variant-numeric: tabular-nums;
    box-shadow: none;
}

menubutton.tk-period-date > button:hover {
    background-color: #262629;
}

box.tk-dot {
    min-width: 8px;
    min-height: 8px;
    border-radius: 999px;
}

box.tk-dot.green {
    background-color: #30D158;
}

box.tk-dot.orange {
    background-color: #FF9F0A;
}

box.tk-dot.red {
    background-color: #FF453A;
}

label.tk-legend-name {
    font-size: 13px;
    font-weight: 500;
    color: #D1D1D6;
}

label.tk-stat-total {
    font-size: 13px;
    font-weight: 600;
    color: #E5E5EA;
    font-variant-numeric: tabular-nums;
    margin-top: 2px;
}

label.tk-section-title {
    font-size: 13px;
    font-weight: 600;
    color: #AEAEB2;
    margin-top: 2px;
}

box.tk-victim-row {
    background-color: #1C1C1E;
    border: 1px solid #262629;
    border-radius: 10px;
    padding: 9px 12px;
}

label.tk-delta {
    font-size: 12px;
    font-weight: 500;
    font-variant-numeric: tabular-nums;
}

label.tk-delta.green {
    color: #30D158;
}

label.tk-delta.orange {
    color: #FF9F0A;
}

label.tk-delta.red {
    color: #FF453A;
}

label.tk-range-sep {
    font-size: 12px;
    font-weight: 600;
    color: #636366;
}

label.tk-view-count {
    font-size: 14px;
    font-weight: 500;
    color: #8E8E93;
    padding-bottom: 2px;
}

/* Hamburger 3-line menu button in top-left */
menubutton.tk-nav-menu-btn > button {
    background-color: transparent;
    color: #AEAEB2;
    border: none;
    border-radius: 8px;
    min-width: 32px;
    min-height: 32px;
    padding: 4px;
    box-shadow: none;
    transition: all 120ms ease;
}

menubutton.tk-nav-menu-btn > button:hover,
menubutton.tk-nav-menu-btn.active > button {
    background-color: #262629;
    color: #F5F5F7;
}

/* Menu popover items */
button.tk-menu-item-btn {
    background: transparent;
    border: none;
    box-shadow: none;
    border-radius: 8px;
    padding: 8px 12px;
    color: #AEAEB2;
    font-size: 13px;
    font-weight: 500;
    min-width: 170px;
}

button.tk-menu-item-btn:hover {
    background-color: #262629;
    color: #F5F5F7;
}

/* Add task banner below title, matching running task style without shadow */
menubutton.tk-add-banner-btn {
    background: transparent;
    border: none;
    padding: 0;
    margin: 2px 0 6px 0;
}

menubutton.tk-add-banner-btn > button {
    background-color: #131317;
    border: 1px solid #222228;
    border-radius: 12px;
    padding: 11px 15px;
    box-shadow: none;
    color: #8E8E93;
    transition: background-color 130ms ease, border-color 130ms ease;
}

menubutton.tk-add-banner-btn > button:hover {
    background-color: #19191F;
    border-color: #31313A;
    color: #F5F5F7;
    box-shadow: none;
}

box.tk-add-plus-badge {
    min-width: 20px;
    min-height: 20px;
    border-radius: 999px;
    border: 1.5px solid #48484E;
    background-color: #18181E;
    color: #AEAEB2;
    padding: 2px;
    transition: all 120ms ease;
}

menubutton.tk-add-banner-btn > button:hover box.tk-add-plus-badge {
    border-color: #8E8E93;
    background-color: #22222B;
    color: #F5F5F7;
}

label.tk-add-placeholder {
    font-size: 13.5px;
    font-weight: 500;
    color: #7A7A82;
}

menubutton.tk-add-banner-btn > button:hover label.tk-add-placeholder {
    color: #AEAEB2;
}

label.tk-add-shortcut {
    font-size: 11px;
    font-weight: 500;
    color: #636366;
    background-color: #18181E;
    border: 1px solid #24242B;
    border-radius: 6px;
    padding: 2px 7px;
    margin-left: 4px;
}

/* Collapsible side drawer on the left */
box.tk-side-panel {
    background-color: #101013;
    border-right: 1px solid #1C1C20;
    padding: 14px 14px;
    transition: all 150ms ease;
}

box.tk-side-hdr {
    padding-bottom: 8px;
    border-bottom: 1px solid #1C1C20;
}

label.tk-side-title {
    font-size: 16px;
    font-weight: 700;
    color: #F5F5F7;
}

button.tk-side-close {
    background: transparent;
    border: none;
    box-shadow: none;
    border-radius: 999px;
    color: #8E8E93;
    min-width: 24px;
    min-height: 24px;
    padding: 2px;
}

button.tk-side-close:hover {
    background-color: #2C2C2E;
    color: #F5F5F7;
}
)CSS";

} // namespace tk
