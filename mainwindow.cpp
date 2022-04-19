/**
 * Roadmap
 * - confirm potentially destructive actions
 * - finish Install Printer action
 * - allow to run commands as different user
 * - rename computer?
 * - domain rejoin?
 */

#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QList>
#include <QObject>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>

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
        on_inputComputer_textChanged();

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
        // If you change/add any action, you must add all of these things:
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

        ui->setupUi(this);
}

MainWindow::~MainWindow() {
    delete ui;
    delete tempDir;
}

// Run a remote command in a CMD window
void MainWindow::executeCLI(const QString &command) {
    auto process = new QProcess(this);

    // Show console window (hidden by default)
    process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->flags |= CREATE_NEW_CONSOLE;
        args->startupInfo->dwFlags &=~ static_cast<unsigned long>(STARTF_USESTDHANDLES);
    });

    // Run the command on the target computer through PsExec
    // Command is wrapped in a cmd window to capture remote stdout/stderr
    process->start("cmd.exe /k \"" + psexec + " /accepteula /s \\\\" + ui->inputComputer->text() + " " + command + "\"");

    // Delete process object when the CMD window closes
    QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [=]() {
        delete process;
    });
}

// Run a remote command and print its output to the Result pane
void MainWindow::executeToResultPane(const QString &command) {
    ui->textResult->setPlainText("");
    auto process = new QProcess(this);

    // We wrap the process in cmd to properly capture the remote computer's stdout
//    process->start("cmd.exe /c \"" + psexec + " /accepteula \\\\" + ui->inputComputer->text() + " systeminfo\"");
    process->start(psexec + " /accepteula \\\\" + ui->inputComputer->text() + " " + command);

    qDebug() << "Running result-pane command...";

    QObject::connect(process, &QProcess::readyReadStandardOutput, this, [=]() {
        qDebug() << process->readAllStandardOutput();
//        ui->textResult->setPlainText(ui->textResult->toPlainText() + QString(process->readAllStandardOutput()));
    });

    QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        [=](/*int exitCode, QProcess::ExitStatus exitStatus*/) {
            qDebug() << process->readAllStandardError();
            qDebug() << process->readAllStandardOutput();
            delete process;
    });
}

void MainWindow::disableButtons() {
    QList<QPushButton *> buttonsList = this->findChildren<QPushButton *>();
    for (int i = 0; i < buttonsList.count(); i++){
        buttonsList.at(i)->setEnabled(false);
    }
    buttonsEnabled = false;
}

void MainWindow::enableButtons() {
    QList<QPushButton *> buttonsList = this->findChildren<QPushButton *>();
    for (int i = 0; i < buttonsList.count(); i++){
        buttonsList.at(i)->setEnabled(true);
    }
    buttonsEnabled = true;
}

// Ensure that the computer name is valid
// This really just means it's a string that the commands it's passed to will
// accept as an argument, and not necessarily a valid computer name. It's not
// looking it up in NetDB or anything.
// Honestly, I just don't want you to type "*" here
void MainWindow::on_inputComputer_textChanged() {
    static QRegularExpression pattern("\\w");
    if (ui->inputComputer->text().contains(pattern) && !buttonsEnabled) {
        enableButtons();
    } else {
        disableButtons();
    }
}

// Start a remote desktop session on the target machine
void MainWindow::on_buttonRemoteDesktop_clicked() {
    QProcess::startDetached("mstsc.exe /v:" + ui->inputComputer->text());
}

// Offer to start a remote assistance session on the target machine
void MainWindow::on_buttonRemoteAssistance_clicked() {
    QProcess::startDetached("msra.exe /offerRA" + ui->inputComputer->text());
}

// Open Computer Management locally, for the target machine
void MainWindow::on_buttonComputerManagement_clicked() {
    QProcess::startDetached("compmgmt.msc /computer=" + ui->inputComputer->text());
}

// Open the default c$ Admin Share of the target machine in Explorer
void MainWindow::on_buttonCDollarAdminShare_clicked() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(
        "\\\\" + ui->inputComputer->text() + "\\c$"
    ));
}

// Look the target machine up in NetDB
void MainWindow::on_buttonNetDB_clicked() {
    QDesktopServices::openUrl(
        "https://itweb.mst.edu/auth-cgi-bin/cgiwrap/netdb/view-host.pl?host=" +
        ui->inputComputer->text() + ".managed.mst.edu"
    );
}

// Run CMD as SYSTEM on the target machine remotely
void MainWindow::on_buttonReverseShell_clicked() {
    executeCLI("cmd");
}

//
void MainWindow::on_buttonExecuteAction_clicked() {
    actions[ui->dropdownActions->currentIndex()]();
}

// Run the `systeminfo` command on the target machine; print output to the Result pane
void MainWindow::action_systemInfo() {
    executeToResultPane("systeminfo");
}

// Good ol' KMS
void MainWindow::action_reactivateWindows() {
    executeCLI("slmgr /skms umad-kmsdfs-01.umad.umsystem.edu && slmgr /ato");
}

void MainWindow::action_getADJoinStatus() {
    executeToResultPane("dsregcmd /status");
}

void MainWindow::action_reinstallOffice365() {
    executeCLI("R: && cd R:\\software\\appdeploy\\office.365\\x64 && setup.exe /configure configuration-test.xmlcd");
}

void MainWindow::action_installPrinter() {
    // QString printerName = getPrinterName();
    // executeCLI("rundll32 printui.dll PrintUIEntry /in /n \\winprint.mst.edu\\" + printerName);
}

void MainWindow::action_shutDown() {
    executeCLI("shutdown -s -t 0");
}

void MainWindow::action_restart() {
    executeCLI("shutdown -r -t 0");
}

void MainWindow::action_sfcDISM() {
    executeCLI("sfc /scannow && dism /online /cleanup-image /restorehealth");
}
