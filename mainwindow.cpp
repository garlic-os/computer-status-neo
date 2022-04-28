/**
 * Roadmap
 * - finish Switch User button
 * - rename computer?
 * - domain rejoin?
 */

#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QInputDialog>
#include <QList>
#include <QMessageBox>
#include <QObject>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>

// For the "show console window" shenanigans
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "mainwindow.h"
#include "ui_mainwindow.h"


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    tempDir(new QTemporaryDir),
    buttonsEnabled(true) {
        updateLabelRunningAs();

        // Unpack psexec.exe
        if (!tempDir->isValid()) {
            qDebug() << "Temp directory not valid";
        }
        psexec = QDir::toNativeSeparators(tempDir->path() + "/psexec.exe");
        QFile::copy(":/psexec.exe", psexec);

        // Populate the Actions array.
        // NB: The order of functions here and list items in the UI's dropdown
        //     MUST be the same. The app uses the INDEX of the selected action
        //     to decide which function to run.
        // Also if you change/add any action, you must add all of these things:
        // 1. A pointer to its function here in the actions array
        // 1. Its function signature in mainwindow.h
        // 2. Its function implementation here in mainwindow.cpp
        // 3. An entry for it in the UI's Actions dropdown menu (or "Combobox", whatever, QT)
        actions[0] = &MainWindow::action_systemInfo;
        actions[1] = &MainWindow::action_reactivateWindows;
        actions[2] = &MainWindow::action_getADJoinStatus;
        actions[3] = &MainWindow::action_reinstallOffice365;
        actions[4] = &MainWindow::action_installPrinter;
        actions[5] = &MainWindow::action_shutDown;
        actions[6] = &MainWindow::action_restart;
        actions[7] = &MainWindow::action_sfcDISM;

        // Disable unused help button in dialog windows
        QApplication::setAttribute(Qt::AA_DisableWindowContextHelpButton);

        ui->setupUi(this);
        on_inputComputer_textChanged();

        // Set theme
        QString themeName;
        QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", QSettings::NativeFormat);
        if (settings.value("AppsUseLightTheme", 1).toInt() == 0) {
            QFile f(":qdarkstyle/style.qss");
            f.open(QFile::ReadOnly | QFile::Text);
            QTextStream ts(&f);
            qApp->setStyleSheet(ts.readAll());
        }
}

MainWindow::~MainWindow() {
    delete ui;
    delete tempDir;
}

inline const QString MainWindow::compName() const {
    return ui->inputComputer->text();
}

bool MainWindow::confirm(const QString &message, const QString &title) const {
    return QMessageBox::warning(this, title, message,
                         QMessageBox::Ok | QMessageBox::Cancel,
                         QMessageBox::Cancel) == QMessageBox::Ok;
}

void MainWindow::updateLabelRunningAs() {
    auto process = new QProcess(this);
    process->start("whoami");
    QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [=]() {
        QString whoami = process->readLine();
        whoami.chop(2);
        ui->labelRunningAs->setText("Running as: " + whoami);
        delete process;
    });
}

// Run a remote command in a CMD window
void MainWindow::executeCLI(const QString &command) {
    auto process = new QProcess(this);

    // Show console window (hidden by default)
    process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->flags |= CREATE_NEW_CONSOLE;
        args->startupInfo->dwFlags &=~ static_cast<unsigned long>(STARTF_USESTDHANDLES);
    });

    // Run the command on the target computer through PsExec.
    // Command is wrapped in a CMD /k window so the output will stay when the command is done.
    process->start("cmd.exe /k \"" + psexec + " /accepteula /s \\\\" + compName() + " " + command + "\"");

    // Delete process object when the CMD window closes
    QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [=]() {
        delete process;
    });
}

// Run a remote command and print its output to the Result pane
void MainWindow::executeToResultPane(const QString &command) {
    disableButtons();
    ui->textResult->setPlainText("Connecting...");
    auto process = new QProcess(this);

    // PsExec's remote process's output is only readable by wrapping it in CMD and
    // redirecting the output to a file on the remote machine.
    // It's a long story.
    QString computerName = "\\\\" + compName();
    QString logPath = computerName + "\\c$\\it-comp-stat.out";
    process->start(psexec + " /accepteula " + computerName + " cmd.exe /c \"" + command + "\" > C:\\it-comp-stat.out");

    // When process finishes
    QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [=]() {
        QFile logFile(logPath);
        logFile.open(QFile::ReadOnly);
        ui->textResult->setPlainText(logFile.readAll().mid(2));
        enableButtons();
        QFile::remove(logPath);
        delete process;
    });
}

void MainWindow::disableButtons() {
    QList<QPushButton *> buttonsList = ui->tabSingleComputer->findChildren<QPushButton *>();
    for (int i = 0; i < buttonsList.count(); i++) {
        buttonsList.at(i)->setEnabled(false);
    }
    buttonsEnabled = false;
}

void MainWindow::enableButtons() {
    QList<QPushButton *> buttonsList = this->findChildren<QPushButton *>();
    for (int i = 0; i < buttonsList.count(); i++) {
        buttonsList.at(i)->setEnabled(true);
    }
    buttonsEnabled = true;
}

// Ensure that the computer name is valid.
// This really just means it's a string that the commands it's passed to will
// accept as an argument, and not necessarily a valid computer name. It's not
// looking it up in NetDB or anything.
// Honestly, I just don't want you to type "*" here.
void MainWindow::on_inputComputer_textChanged() {
    static QRegularExpression pattern("\\w");
    if (compName().contains(pattern)) {  // TODO: Only run when necessary
        enableButtons();
    } else {
        disableButtons();
    }
}

// Start a remote desktop session on the target machine
void MainWindow::on_buttonRemoteDesktop_clicked() {
    QProcess::startDetached("mstsc.exe /v:" + compName());
}

// Offer to start a remote assistance session on the target machine
void MainWindow::on_buttonRemoteAssistance_clicked() {
    QProcess::startDetached("msra.exe /offerRA" + compName());
}

// Open Computer Management locally, for the target machine
void MainWindow::on_buttonComputerManagement_clicked() {
    QProcess::startDetached("compmgmt.msc /computer=" + compName());
}

// Open the default c$ Admin Share of the target machine in Explorer
void MainWindow::on_buttonCDollarAdminShare_clicked() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(
        "\\\\" + compName() + "\\c$"
    ));
}

// Look the target machine up in NetDB
void MainWindow::on_buttonNetDB_clicked() {
    QDesktopServices::openUrl(
        "https://itweb.mst.edu/auth-cgi-bin/cgiwrap/netdb/view-host.pl?host=" +
        compName() + ".managed.mst.edu"
    );
}

// Run CMD as SYSTEM on the target machine remotely
void MainWindow::on_buttonReverseShell_clicked() {
    executeCLI("cmd");
}

// Run the function corresponding to (the index of) the currently selected action
void MainWindow::on_buttonExecuteAction_clicked() {
    int actionIndex = ui->dropdownActions->currentIndex();
    t_memberFunction action = actions[actionIndex];
    (this->*action)();
}

// Run the `systeminfo` command on the target machine; print output to the Result pane
void MainWindow::action_systemInfo() {
    executeToResultPane("systeminfo.exe");
}

// Good ol' KMS
void MainWindow::action_reactivateWindows() {
    if (confirm("Are you sure you want to reactivate this computer's Windows license?", compName() + ": Reactivate Windows License"))
        executeCLI("slmgr /skms umad-kmsdfs-01.umad.umsystem.edu && slmgr /ato");
}

void MainWindow::action_getADJoinStatus() {
    executeToResultPane("dsregcmd /status");
}

void MainWindow::action_reinstallOffice365() {
    if (confirm("Are you sure you want to reinstall Office 365 on this computer?", compName() + ": Reinstall Microsoft Office 365"))
        executeCLI("R: && cd R:\\software\\appdeploy\\office.365\\x64 && setup.exe /configure configuration-test.xmlcd");
}

void MainWindow::action_installPrinter() {
    bool ok;
    QString printerName = QInputDialog::getText(
        this, compName() + ": Install printer",
        "Enter the name of the printer to install:",
        QLineEdit::Normal, "", &ok, Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);
    if (ok && !printerName.isEmpty())
        executeCLI("rundll32 printui.dll PrintUIEntry /in /n \\winprint.mst.edu\\" + printerName);
}

void MainWindow::action_shutDown() {
    if (confirm("Are you sure you want to shut down this computer?", compName() + ": Send shutdown signal"))
        executeCLI("shutdown -s -t 0");
}

void MainWindow::action_restart() {
    if (confirm("Are you sure you want to restart this computer?", compName() + ": Send restart signal"))
        executeCLI("shutdown -r -t 0");
}

void MainWindow::action_sfcDISM() {
    executeCLI("sfc /scannow && dism /online /cleanup-image /restorehealth");
}
