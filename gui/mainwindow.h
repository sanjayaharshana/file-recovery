#pragma once

#include <QMainWindow>
#include <QProcess>
#include <memory>

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QCheckBox;
class QSpinBox;
class QGroupBox;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onBrowseInput();
    void onBrowseOutput();
    void onModeChanged(int index);
    void onRun();
    void onShowGuide();
    void onProcessOutput();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    void appendLog(const QString& text);
    static QString recoveryExecutablePath();
    void updateInputBrowseState();

    QComboBox* mode_combo_{};
    QCheckBox* input_is_folder_{};
    QLineEdit* input_edit_{};
    QLineEdit* output_edit_{};
    QLineEdit* types_edit_{};
    QSpinBox* max_chunk_spin_{};
    QPushButton* run_btn_{};
    QPlainTextEdit* log_{};
    QGroupBox* carve_options_{};
    std::unique_ptr<QProcess> process_;
};
