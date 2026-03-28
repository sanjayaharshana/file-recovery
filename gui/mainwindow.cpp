#include "mainwindow.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpinBox>
#include <QUrl>
#include <QVBoxLayout>
#include <QtGlobal>

#if defined(Q_OS_MACOS)
#include <QEventLoop>
#endif

namespace {

constexpr int kDefaultMaxChunk = 50 * 1024 * 1024;
const QString kAppVersion = QStringLiteral("1.0.6");

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("File Recovery"));
    resize(780, 620);

    auto* central = new QWidget(this);
    central->setObjectName(QStringLiteral("centralRoot"));
    setCentralWidget(central);
    auto* main_layout = new QVBoxLayout(central);
    main_layout->setContentsMargins(28, 24, 28, 24);
    main_layout->setSpacing(8);

    auto* header = new QWidget(central);
    auto* header_outer = new QHBoxLayout(header);
    header_outer->setContentsMargins(0, 0, 0, 12);
    header_outer->setSpacing(16);
    auto* header_layout = new QVBoxLayout();
    header_layout->setSpacing(4);
    auto* title = new QLabel(QStringLiteral("File Recovery"), header);
    title->setObjectName(QStringLiteral("appTitle"));
    auto* subtitle = new QLabel(
        QStringLiteral("Carve files by signature or recover deleted entries on FAT32 images"),
        header);
    subtitle->setObjectName(QStringLiteral("appSubtitle"));
    subtitle->setWordWrap(true);
    header_layout->addWidget(title);
    header_layout->addWidget(subtitle);
    header_outer->addLayout(header_layout, 1);
    auto* guide_btn = new QPushButton(QStringLiteral("Guide / About"), header);
    guide_btn->setObjectName(QStringLiteral("guideBtn"));
    guide_btn->setCursor(Qt::PointingHandCursor);
    guide_btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    guide_btn->setMinimumHeight(36);
    header_outer->addWidget(guide_btn, 0, Qt::AlignTop);
    main_layout->addWidget(header);

    auto* sep = new QFrame(central);
    sep->setFrameShape(QFrame::NoFrame);
    sep->setFixedHeight(1);
    sep->setStyleSheet(QStringLiteral("background-color: #2a3040; border: none;"));
    main_layout->addWidget(sep);

    auto* mode_group = new QGroupBox(QStringLiteral("Mode"), this);
    auto* mode_layout = new QHBoxLayout(mode_group);
    mode_combo_ = new QComboBox(mode_group);
    mode_combo_->addItem(QStringLiteral("Carve (signatures from file or folder)"), QStringLiteral("carve"));
    mode_combo_->addItem(QStringLiteral("FAT32 undelete (disk/partition image)"), QStringLiteral("fat32"));
    mode_layout->addWidget(new QLabel(QStringLiteral("Operation:"), mode_group));
    mode_layout->addWidget(mode_combo_, 1);
    main_layout->addWidget(mode_group);

    auto* paths_group = new QGroupBox(QStringLiteral("Paths"), this);
    auto* paths_form = new QFormLayout(paths_group);
    paths_form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    paths_form->setHorizontalSpacing(12);

    input_is_folder_ = new QCheckBox(QStringLiteral("Input is a folder (carve only)"), paths_group);
    input_edit_ = new QLineEdit(paths_group);
    input_edit_->setPlaceholderText(QStringLiteral("File or folder to scan"));
    input_edit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* input_btn = new QPushButton(QStringLiteral("Browse…"), paths_group);
    input_btn->setObjectName(QStringLiteral("browseBtn"));
    input_btn->setMinimumWidth(108);
    input_btn->setCursor(Qt::PointingHandCursor);
    input_btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto* input_row = new QHBoxLayout();
    input_row->setSpacing(8);
    input_row->addWidget(input_edit_, 1);
    input_row->addWidget(input_btn, 0);
    paths_form->addRow(input_is_folder_, input_row);

    output_edit_ = new QLineEdit(paths_group);
    output_edit_->setPlaceholderText(QStringLiteral("Where to write recovered files"));
    output_edit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* out_btn = new QPushButton(QStringLiteral("Browse…"), paths_group);
    out_btn->setObjectName(QStringLiteral("browseBtn"));
    out_btn->setMinimumWidth(108);
    out_btn->setCursor(Qt::PointingHandCursor);
    out_btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto* out_row = new QHBoxLayout();
    out_row->setSpacing(8);
    out_row->addWidget(output_edit_, 1);
    out_row->addWidget(out_btn, 0);
    paths_form->addRow(QStringLiteral("Output folder:"), out_row);

    main_layout->addWidget(paths_group);

    carve_options_ = new QGroupBox(QStringLiteral("Carve options"), this);
    auto* carve_form = new QFormLayout(carve_options_);
    types_edit_ = new QLineEdit(carve_options_);
    types_edit_->setPlaceholderText(QStringLiteral("jpeg,png,pdf,zip,gif — leave empty for all"));
    carve_form->addRow(QStringLiteral("Types:"), types_edit_);
    max_chunk_spin_ = new QSpinBox(carve_options_);
    max_chunk_spin_->setRange(1, 2'000'000'000);
    max_chunk_spin_->setValue(kDefaultMaxChunk);
    max_chunk_spin_->setSingleStep(1'000'000);
    max_chunk_spin_->setSuffix(QStringLiteral(" bytes"));
    carve_form->addRow(QStringLiteral("Max chunk (unbounded formats):"), max_chunk_spin_);
    main_layout->addWidget(carve_options_);

    run_btn_ = new QPushButton(QStringLiteral("Run recovery"), this);
    run_btn_->setObjectName(QStringLiteral("runBtn"));
    run_btn_->setMinimumHeight(44);
    run_btn_->setCursor(Qt::PointingHandCursor);
    main_layout->addWidget(run_btn_);
    main_layout->addSpacing(4);

    auto* log_label = new QLabel(QStringLiteral("Output log"), this);
    log_label->setObjectName(QStringLiteral("logHeader"));
    main_layout->addWidget(log_label);
    log_ = new QPlainTextEdit(this);
    log_->setObjectName(QStringLiteral("logView"));
    log_->setReadOnly(true);
#ifdef Q_OS_WIN
    log_->setFont(QFont(QStringLiteral("Cascadia Mono"), 11));
    if (!log_->font().exactMatch()) {
        log_->setFont(QFont(QStringLiteral("Consolas"), 10));
    }
#else
    log_->setFont(QFont(QStringLiteral("SF Mono"), 11));
    if (!log_->font().exactMatch()) {
        log_->setFont(QFont(QStringLiteral("Menlo"), 11));
    }
#endif
    log_->setMinimumHeight(220);
    main_layout->addWidget(log_, 1);

    connect(mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MainWindow::onModeChanged);
    connect(input_btn, &QPushButton::clicked, this, &MainWindow::onBrowseInput);
    connect(out_btn, &QPushButton::clicked, this, &MainWindow::onBrowseOutput);
    connect(guide_btn, &QPushButton::clicked, this, &MainWindow::onShowGuide);
    connect(run_btn_, &QPushButton::clicked, this, &MainWindow::onRun);

    onModeChanged(0);
    appendLog(QStringLiteral("Ready. The CLI binary is expected next to this app: ") +
              recoveryExecutablePath());
}

MainWindow::~MainWindow() = default;

QString MainWindow::recoveryExecutablePath() {
    const QDir app_dir(QApplication::applicationDirPath());
#ifdef Q_OS_WIN
    const QString name = QStringLiteral("file_recovery.exe");
#else
    const QString name = QStringLiteral("file_recovery");
#endif
    return app_dir.filePath(name);
}

void MainWindow::appendLog(const QString& text) {
    log_->appendPlainText(text);
}

void MainWindow::updateInputBrowseState() {
    const bool carve = mode_combo_->currentData().toString() == QLatin1String("carve");
    input_is_folder_->setEnabled(carve);
    carve_options_->setEnabled(carve);
    if (!carve) {
        input_is_folder_->setChecked(false);
    }
}

void MainWindow::onModeChanged(int) {
    updateInputBrowseState();
}

void MainWindow::onBrowseInput() {
    raise();
    activateWindow();
#if defined(Q_OS_MACOS)
    QApplication::processEvents(QEventLoop::AllEvents, 50);
#endif

    const bool carve = mode_combo_->currentData().toString() == QLatin1String("carve");
    const bool folder = carve && input_is_folder_->isChecked();
    QString path;
#if defined(Q_OS_MACOS)
    const QFileDialog::Options dlg_opts = QFileDialog::DontUseNativeDialog;
#else
    const QFileDialog::Options dlg_opts = {};
#endif

    if (folder) {
        path = QFileDialog::getExistingDirectory(
            this, QStringLiteral("Input folder"),
            input_edit_->text().isEmpty() ? QDir::homePath() : input_edit_->text(),
            QFileDialog::ShowDirsOnly | dlg_opts);
    } else {
        path = QFileDialog::getOpenFileName(this, QStringLiteral("Input file"), QDir::homePath(),
                                            QString(), nullptr, dlg_opts);
    }
    if (!path.isEmpty()) {
        input_edit_->setText(path);
    }
}

void MainWindow::onBrowseOutput() {
    raise();
    activateWindow();
#if defined(Q_OS_MACOS)
    QApplication::processEvents(QEventLoop::AllEvents, 50);
#endif

#if defined(Q_OS_MACOS)
    const QFileDialog::Options dlg_opts = QFileDialog::DontUseNativeDialog;
#else
    const QFileDialog::Options dlg_opts = {};
#endif

    const QString path = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Output folder"),
        output_edit_->text().isEmpty() ? QDir::homePath() : output_edit_->text(),
        QFileDialog::ShowDirsOnly | dlg_opts);
    if (!path.isEmpty()) {
        output_edit_->setText(path);
    }
}

void MainWindow::onShowGuide() {
    raise();
    activateWindow();

    QMessageBox box(this);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(QStringLiteral("About File Recovery"));
    box.setText(
        QStringLiteral("<p><b>Copyright (c) Sanjaya Senevirathne</b></p>"
                       "<p>Version <b>%1</b> (Update)</p>"
                       "<p><i>Note:</i> Search on YouTube for <b>Project with Sanju</b> "
                       "(Sanju channel) for guides and videos about this project.</p>")
            .arg(kAppVersion));
    box.setTextFormat(Qt::RichText);
    box.setTextInteractionFlags(Qt::TextBrowserInteraction);
    QAbstractButton* const yt_btn =
        box.addButton(QStringLiteral("Open YouTube search"), QMessageBox::ActionRole);
    box.addButton(QMessageBox::Ok);
    box.setDefaultButton(QMessageBox::Ok);
    box.exec();
    if (box.clickedButton() == yt_btn) {
        QDesktopServices::openUrl(QUrl(QStringLiteral(
            "https://www.youtube.com/results?search_query=Project+with+Sanju+file+recovery")));
    }
}

void MainWindow::onRun() {
    if (process_ && process_->state() != QProcess::NotRunning) {
        return;
    }

    const QString exe = recoveryExecutablePath();
    if (!QFileInfo::exists(exe)) {
        QMessageBox::warning(this, QStringLiteral("Missing tool"),
                             QStringLiteral("Could not find file_recovery at:\n%1\n\n"
                                            "Build the CLI target and place it next to this app, "
                                            "or add it to PATH as \"file_recovery\".")
                                 .arg(exe));
        return;
    }

    const QString input = input_edit_->text().trimmed();
    const QString output = output_edit_->text().trimmed();
    if (input.isEmpty() || output.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Input / output"),
                             QStringLiteral("Please set both input and output paths."));
        return;
    }

    const QString mode = mode_combo_->currentData().toString();
    QStringList args;
    args << QStringLiteral("--mode") << mode;
    args << QStringLiteral("--input") << input;
    args << QStringLiteral("--output") << output;

    if (mode == QLatin1String("carve")) {
        const QString types = types_edit_->text().trimmed();
        if (!types.isEmpty()) {
            args << QStringLiteral("--types") << types;
        }
        args << QStringLiteral("--max-chunk") << QString::number(max_chunk_spin_->value());
    }

    log_->clear();
    appendLog(QStringLiteral("$ %1 %2").arg(exe, args.join(QLatin1Char(' '))));

    process_ = std::make_unique<QProcess>(this);
    process_->setProgram(exe);
    process_->setArguments(args);
    process_->setProcessChannelMode(QProcess::MergedChannels);

    connect(process_.get(), &QProcess::readyReadStandardOutput, this, &MainWindow::onProcessOutput);
    connect(process_.get(),
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            &MainWindow::onProcessFinished);

    run_btn_->setEnabled(false);
    process_->start();
    if (!process_->waitForStarted(3000)) {
        appendLog(QStringLiteral("Error: could not start process."));
        run_btn_->setEnabled(true);
        process_.reset();
    }
}

void MainWindow::onProcessOutput() {
    if (!process_) {
        return;
    }
    const QByteArray chunk = process_->readAllStandardOutput();
    appendLog(QString::fromLocal8Bit(chunk));
}

void MainWindow::onProcessFinished(int exitCode, QProcess::ExitStatus status) {
    run_btn_->setEnabled(true);
    if (status == QProcess::CrashExit) {
        appendLog(QStringLiteral("\nProcess crashed."));
    } else {
        appendLog(QStringLiteral("\nExit code: %1").arg(exitCode));
    }
    process_.reset();
}
