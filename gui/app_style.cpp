#include "app_style.hpp"

#include <QApplication>
#include <QString>

void applyModernAppStyle(QApplication* app) {
    app->setStyle(QStringLiteral("Fusion"));

    const QString qss = QStringLiteral(
        R"(
/* File Recovery — dark theme */
QMainWindow, QWidget#centralRoot {
    background-color: #12141a;
}
QLabel#appTitle {
    color: #f0f2f7;
    font-size: 22px;
    font-weight: 700;
    letter-spacing: -0.5px;
}
QLabel#appSubtitle {
    color: #8b93a7;
    font-size: 13px;
    margin-top: 2px;
}
QGroupBox {
    font-size: 12px;
    font-weight: 600;
    color: #c8cdd8;
    border: 1px solid #2a3040;
    border-radius: 12px;
    margin-top: 16px;
    padding: 20px 16px 14px 16px;
    background-color: #1a1e2a;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 14px;
    padding: 0 10px;
    color: #7dd3c0;
}
QLabel {
    color: #d1d5e0;
    font-size: 13px;
}
QLineEdit, QSpinBox, QComboBox {
    background-color: #0f1117;
    border: 1px solid #343a4d;
    border-radius: 8px;
    padding: 8px 12px;
    min-height: 20px;
    color: #eef0f5;
    font-size: 13px;
    selection-background-color: #2a6f5c;
}
QLineEdit:focus, QSpinBox:focus, QComboBox:focus {
    border-color: #3ecf9a;
}
QLineEdit:disabled, QSpinBox:disabled, QComboBox:disabled {
    color: #5c6478;
    background-color: #161922;
}
QComboBox::drop-down {
    border: none;
    width: 28px;
}
QComboBox::down-arrow {
    width: 0;
    height: 0;
    border-left: 5px solid transparent;
    border-right: 5px solid transparent;
    border-top: 6px solid #8b93a7;
    margin-right: 8px;
}
QComboBox QAbstractItemView {
    background-color: #1a1e2a;
    color: #eef0f5;
    border: 1px solid #343a4d;
    selection-background-color: #2a6f5c;
    outline: none;
    padding: 4px;
}
QSpinBox::up-button, QSpinBox::down-button {
    width: 20px;
    background-color: #252a38;
    border: none;
    border-radius: 4px;
    margin: 2px;
}
QSpinBox::up-button:hover, QSpinBox::down-button:hover {
    background-color: #343a4d;
}
QCheckBox {
    color: #d1d5e0;
    spacing: 10px;
    font-size: 13px;
}
QCheckBox::indicator {
    width: 18px;
    height: 18px;
    border-radius: 5px;
    border: 1px solid #454d62;
    background-color: #0f1117;
}
QCheckBox::indicator:checked {
    background-color: #2a9d78;
    border-color: #3ecf9a;
}
QPushButton#browseBtn {
    background-color: #252a38;
    color: #e4e7ef;
    border: 1px solid #3d4458;
    border-radius: 8px;
    padding: 8px 16px;
    font-size: 13px;
    font-weight: 500;
    min-height: 20px;
}
QPushButton#browseBtn:hover {
    background-color: #2f3548;
    border-color: #5a6278;
}
QPushButton#browseBtn:pressed {
    background-color: #1e222e;
}
QPushButton#guideBtn {
    background-color: #252a38;
    color: #e4e7ef;
    border: 1px solid #3d4458;
    border-radius: 8px;
    padding: 8px 16px;
    font-size: 13px;
    font-weight: 500;
    min-height: 20px;
}
QPushButton#guideBtn:hover {
    background-color: #2f3548;
    border-color: #5a6278;
}
QPushButton#guideBtn:pressed {
    background-color: #1e222e;
}
QPushButton#runBtn {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
        stop:0 #2a9d78, stop:1 #1d7a5c);
    color: #ffffff;
    border: none;
    border-radius: 10px;
    padding: 12px 24px;
    font-size: 14px;
    font-weight: 600;
    min-height: 22px;
}
QPushButton#runBtn:hover {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
        stop:0 #34b88a, stop:1 #249068);
}
QPushButton#runBtn:pressed {
    background: #1a6b52;
}
QPushButton#runBtn:disabled {
    background: #2a3040;
    color: #6b7288;
}
QPlainTextEdit#logView {
    background-color: #0a0c10;
    color: #a8f0c8;
    border: 1px solid #252a38;
    border-radius: 10px;
    padding: 12px 14px;
    font-size: 12px;
    selection-background-color: #2a6f5c;
}
QLabel#logHeader {
    color: #8b93a7;
    font-size: 11px;
    font-weight: 600;
    text-transform: uppercase;
    letter-spacing: 1.2px;
    margin-top: 4px;
}
QScrollBar:vertical {
    background: #14161c;
    width: 10px;
    margin: 0;
    border-radius: 5px;
}
QScrollBar::handle:vertical {
    background: #3d4458;
    min-height: 40px;
    border-radius: 5px;
}
QScrollBar::handle:vertical:hover {
    background: #5a6278;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0;
}
QMessageBox {
    background-color: #1a1e2a;
}
QMessageBox QLabel {
    color: #e8eaef;
}
QMessageBox QPushButton {
    background-color: #252a38;
    color: #e4e7ef;
    border: 1px solid #3d4458;
    border-radius: 8px;
    padding: 8px 18px;
    min-width: 72px;
}
QMessageBox QPushButton:hover {
    background-color: #2f3548;
}
)");

    app->setStyleSheet(qss);
}
