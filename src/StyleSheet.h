#pragma once
#include <QString>

// Dark theme stylesheet that mimics the SnapLevel Editor look & feel.
inline const QString kAppStyleSheet = R"(
QWidget {
    background-color: #1c2026;
    color: #d7dadf;
    font-family: "Segoe UI", "Inter", sans-serif;
    font-size: 12px;
}

QMainWindow {
    background-color: #1c2026;
}

QMenuBar {
    background-color: #20242b;
    border-bottom: 1px solid #2c313a;
    padding: 2px;
}
QMenuBar::item {
    background: transparent;
    padding: 4px 10px;
    border-radius: 4px;
}
QMenuBar::item:selected {
    background-color: #2c313a;
}
QMenu {
    background-color: #252a32;
    border: 1px solid #353b45;
}
QMenu::item {
    padding: 6px 24px;
}
QMenu::item:selected {
    background-color: #2f6fdb;
}

QToolBar {
    background-color: #20242b;
    border-bottom: 1px solid #2c313a;
    spacing: 4px;
    padding: 4px;
}
QToolButton {
    background-color: transparent;
    border: 1px solid transparent;
    border-radius: 6px;
    padding: 6px 10px;
    color: #c7cbd1;
}
QToolButton:hover {
    background-color: #2c313a;
    border: 1px solid #3a4150;
}
QToolButton:checked {
    background-color: #2f6fdb;
    color: #ffffff;
    border: 1px solid #2f6fdb;
}

QDockWidget {
    titlebar-close-icon: none;
    color: #d7dadf;
}
QDockWidget::title {
    background-color: #20242b;
    padding: 6px 8px;
    border-bottom: 1px solid #2c313a;
    font-weight: 600;
}

QTreeWidget, QListWidget, QTableWidget {
    background-color: #1c2026;
    border: 1px solid #2c313a;
    border-radius: 4px;
    outline: none;
    selection-background-color: #2f6fdb;
    selection-color: #ffffff;
    alternate-background-color: #20242b;
}
QTreeWidget::item, QListWidget::item {
    padding: 3px 2px;
    border-radius: 3px;
}
QTreeWidget::item:selected, QListWidget::item:selected {
    background-color: #2f6fdb;
    color: #ffffff;
}
QHeaderView::section {
    background-color: #252a32;
    color: #aeb3ba;
    padding: 4px;
    border: none;
    border-bottom: 1px solid #2c313a;
}

QTabWidget::pane {
    border: 1px solid #2c313a;
    background-color: #1c2026;
}
QTabBar::tab {
    background-color: #20242b;
    color: #aeb3ba;
    padding: 7px 16px;
    border: 1px solid #2c313a;
    border-bottom: none;
}
QTabBar::tab:selected {
    background-color: #1c2026;
    color: #ffffff;
    border-bottom: 2px solid #2f6fdb;
}
QTabBar::close-button {
    image: none;
}

QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {
    background-color: #14171c;
    border: 1px solid #353b45;
    border-radius: 4px;
    padding: 4px 6px;
    color: #d7dadf;
    selection-background-color: #2f6fdb;
}
QLineEdit:focus, QComboBox:focus {
    border: 1px solid #2f6fdb;
}
QComboBox::drop-down {
    border: none;
    width: 18px;
}
QComboBox QAbstractItemView {
    background-color: #252a32;
    border: 1px solid #353b45;
    selection-background-color: #2f6fdb;
}

QPushButton {
    background-color: #2a2f38;
    border: 1px solid #3a4150;
    border-radius: 4px;
    padding: 5px 10px;
}
QPushButton:hover {
    background-color: #323844;
}
QPushButton:pressed {
    background-color: #2f6fdb;
}

QCheckBox::indicator {
    width: 14px;
    height: 14px;
    border: 1px solid #4a515e;
    border-radius: 3px;
    background-color: #14171c;
}
QCheckBox::indicator:checked {
    background-color: #2f6fdb;
    border: 1px solid #2f6fdb;
}

QGroupBox {
    border: 1px solid #2c313a;
    border-radius: 6px;
    margin-top: 14px;
    font-weight: 600;
    color: #aeb3ba;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 8px;
    padding: 0 4px;
}

QStatusBar {
    background-color: #20242b;
    border-top: 1px solid #2c313a;
    color: #9aa0a8;
}

QScrollBar:vertical {
    background: #1c2026;
    width: 12px;
    margin: 0;
}
QScrollBar::handle:vertical {
    background: #3a4150;
    border-radius: 5px;
    min-height: 24px;
}
QScrollBar:horizontal {
    background: #1c2026;
    height: 12px;
}
QScrollBar::handle:horizontal {
    background: #3a4150;
    border-radius: 5px;
    min-width: 24px;
}

QSlider::groove:horizontal {
    height: 4px;
    background: #3a4150;
    border-radius: 2px;
}
QSlider::handle:horizontal {
    background: #2f6fdb;
    width: 12px;
    margin: -5px 0;
    border-radius: 6px;
}

QLabel#sectionLabel {
    color: #aeb3ba;
    font-weight: 600;
}
QFrame#separatorLine {
    background-color: #2c313a;
}
)";
