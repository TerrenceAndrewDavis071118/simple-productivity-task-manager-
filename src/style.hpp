#pragma once

namespace tk {

constexpr const char* APP_CSS = R"CSS(
* {
    font-family: "SF Pro Display", "SF Pro Text", "Segoe UI Variable Text", "Segoe UI", -apple-system, BlinkMacSystemFont, "Inter", system-ui, sans-serif;
}

/* =========================================================================
   Liquid Glass & Frosted Glass Design System (iOS 26 Translucency Adaption)
   ========================================================================= */

window.tk-window {
    background-color: #0E0E12;
    background: radial-gradient(circle at 50% 0%, #16161D 0%, #0A0A0C 100%);
    color: #F5F5F7;
}

headerbar.tk-header {
    background-color: transparent;
    border: none;
    box-shadow: none;
    min-height: 44px;
    padding: 4px 12px 0 12px;
}

box.tk-sidebar {
    background-color: rgba(28, 28, 32, 0.72);
    border-radius: 16px;
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-top: 1px solid rgba(255, 255, 255, 0.18);
    box-shadow: 0 8px 32px rgba(0, 0, 0, 0.45);
    margin: 2px 0 12px 12px;
    padding: 14px 10px;
}

button.tk-nav {
    background: transparent;
    border: 1px solid transparent;
    box-shadow: none;
    border-radius: 10px;
    padding: 8px 12px;
    margin: 2px 0;
    color: #A1A1A6;
    font-size: 13px;
    font-weight: 500;
    transition: all 140ms ease;
}

button.tk-nav:hover {
    background-color: rgba(255, 255, 255, 0.08);
    color: #FFFFFF;
    border-color: rgba(255, 255, 255, 0.06);
}

button.tk-nav.active {
    background: linear-gradient(180deg, rgba(255, 255, 255, 0.16) 0%, rgba(255, 255, 255, 0.08) 100%);
    border: 1px solid rgba(255, 255, 255, 0.18);
    color: #FFFFFF;
    font-weight: 600;
    box-shadow: 0 2px 12px rgba(0, 0, 0, 0.25);
}

/* Scrolled window, Viewport & GtkListBox resets to prevent tube/rounded outline clipping and grey box */
scrolledwindow,
scrolledwindow > viewport,
viewport {
    background: transparent;
    background-color: transparent;
    border: none;
    box-shadow: none;
}

box.tk-tasks-panel {
    background-color: transparent;
    border: none;
    box-shadow: none;
    padding: 0;
}

list.tk-list {
    background: transparent;
    background-color: transparent;
    border: none;
    box-shadow: none;
    padding: 0;
}

list.tk-list > row,
list.tk-list > row:not(:first-child) {
    background: transparent;
    background-color: transparent;
    border: none;
    border-top: none;
    box-shadow: none;
    outline: none;
    padding: 5px 2px;
    margin: 0;
    border-radius: 0;
}

list.tk-list > row:hover,
list.tk-list > row:selected,
list.tk-list > row:focus,
list.tk-list > row:active {
    background: transparent;
    background-color: transparent;
    border: none;
    border-top: none;
    box-shadow: none;
    outline: none;
}

box.tk-row {
    background: linear-gradient(180deg, rgba(34, 34, 40, 0.75) 0%, rgba(22, 22, 26, 0.70) 100%);
    border: 1px solid rgba(255, 255, 255, 0.12);
    border-radius: 16px;
    padding: 14px 16px;
    box-shadow: 0 4px 16px rgba(0, 0, 0, 0.35);
    transition: all 140ms ease;
}

box.tk-row:hover {
    background: linear-gradient(180deg, rgba(42, 42, 50, 0.88) 0%, rgba(28, 28, 34, 0.82) 100%);
    border-color: rgba(255, 255, 255, 0.24);
    box-shadow: 0 8px 24px rgba(0, 0, 0, 0.55);
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

popover,
popover.tk-popover {
    background: transparent;
    border: none;
    box-shadow: none;
}

popover > arrow,
popover.tk-popover > arrow {
    background-color: #202023;
    border: 1px solid #36363A;
}

popover.tk-popover > contents {
    background-color: #202023;
    border: 1px solid #36363A;
    border-radius: 16px;
    padding: 14px;
    color: #F5F5F7;
    box-shadow: 0 8px 24px rgba(0, 0, 0, 0.6);
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
    background-color: transparent;
    border: none;
    box-shadow: none;
    border-radius: 0px;
    padding: 2px 0;
}

button.tk-seg-btn {
    background: transparent;
    border: none;
    box-shadow: none;
    border-radius: 0px;
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
    background-color: transparent;
    border: none;
    box-shadow: none;
    border-radius: 0px;
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

/* Merged inline Add Task card below title */
box.tk-add-card {
    background: linear-gradient(180deg, rgba(28, 28, 34, 0.70) 0%, rgba(20, 20, 24, 0.65) 100%);
    border: 1px solid rgba(255, 255, 255, 0.11);
    border-radius: 14px;
    padding: 10px 14px;
    margin: 2px 0 6px 0;
    box-shadow: none;
    transition: background-color 130ms ease, border-color 130ms ease;
}

box.tk-add-card:hover {
    background: linear-gradient(180deg, rgba(34, 34, 42, 0.85) 0%, rgba(24, 24, 30, 0.80) 100%);
    border-color: rgba(255, 255, 255, 0.20);
}

box.tk-add-card.expanded {
    background: rgba(22, 22, 26, 0.95);
    border-color: rgba(255, 255, 255, 0.22);
    border-radius: 16px;
    padding: 12px 14px 14px 14px;
}

entry.tk-add-inline-entry,
entry.tk-add-inline-entry:focus-within,
entry.tk-add-inline-entry text,
entry.tk-add-inline-entry > text,
entry.tk-add-inline-entry text:focus,
entry.tk-add-inline-entry text:focus-within {
    background: transparent;
    background-color: transparent;
    background-image: none;
    border: none;
    outline: none;
    outline-width: 0;
    box-shadow: none;
    color: #F5F5F7;
    padding: 2px 0;
    min-height: 24px;
    font-size: 13.5px;
    font-weight: 500;
    caret-color: #F5F5F7;
}

box.tk-add-details {
    margin-top: 10px;
    padding-top: 12px;
    border-top: 1px solid #1E1E24;
}

textview.tk-detail-entry text {
    background-color: #17171C;
    border: 1px solid #282830;
    border-radius: 8px;
    color: #F5F5F7;
    padding: 8px 10px;
    font-size: 13px;
    caret-color: #F5F5F7;
}

textview.tk-detail-entry text:focus-within {
    border-color: #636366;
}

label.tk-detail-field-title {
    font-size: 11px;
    font-weight: 600;
    color: #8E8E93;
    text-transform: uppercase;
    letter-spacing: 0.5px;
}

box.tk-subtask-row {
    padding: 2px 0;
}

checkbutton.tk-sub-check check {
    min-width: 15px;
    min-height: 15px;
    border-radius: 4px;
    border: 1.5px solid #3A3A40;
    background-color: transparent;
}

checkbutton.tk-sub-check check:checked {
    background-color: #30D158;
    border-color: #30D158;
}

label.tk-sub-title {
    font-size: 12px;
    color: #AEAEB2;
}

label.tk-sub-title.done {
    color: #55555C;
    text-decoration: line-through;
}

label.tk-task-detail {
    font-size: 12px;
    color: #8E8E93;
    margin-top: 2px;
}

picture.tk-detail-img {
    border-radius: 10px;
    border: 1px solid rgba(255, 255, 255, 0.14);
    box-shadow: 0 4px 14px rgba(0, 0, 0, 0.40);
    transition: transform 120ms ease, box-shadow 120ms ease, border-color 120ms ease;
}

picture.tk-detail-img:hover {
    border-color: rgba(255, 255, 255, 0.35);
    box-shadow: 0 6px 18px rgba(0, 0, 0, 0.55);
}

box.tk-add-plus-badge {
    min-width: 18px;
    min-height: 18px;
    border: none;
    background-color: transparent;
    color: #7A7A82;
    padding: 0;
    transition: color 120ms ease;
}

box.tk-add-card:hover box.tk-add-plus-badge,
box.tk-add-card.expanded box.tk-add-plus-badge {
    background-color: transparent;
    color: #F5F5F7;
}

label.tk-add-hint {
    font-size: 13.5px;
    font-weight: 500;
    color: #7A7A82;
    opacity: 1.0;
    transition: color 120ms ease;
}

box.tk-add-card:hover label.tk-add-hint {
    color: #AEAEB2;
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
