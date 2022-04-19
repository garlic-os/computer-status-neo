/**
 * Roadmap
 * - async/await in favor of callbacks
 * - allow to run commands as different user
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
    buttonsEnabled(true) {
        on_inputComputer_textChanged();

        // Unpack psexec.exe
        auto tempDir = new QTemporaryDir();
        if (!tempDir->isValid()) {
            qDebug() << "Temp directory not valid";
        }
        psexec = QDir::toNativeSeparators(tempDir->path() + "/psexec.exe");
        QFile::copy(":/psexec.exe", psexec);

        actions = new t_action[1] {
            &this->action_SystemInfo
        };

        ui->setupUi(this);
}

MainWindow::~MainWindow() {
    delete ui;
    delete tempDir;
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

void MainWindow::on_inputComputer_textChanged() {
    static QRegularExpression pattern("\\w");
    if (ui->inputComputer->text().contains(pattern) && !buttonsEnabled) {
        enableButtons();
    } else {
        disableButtons();
    }
}

void MainWindow::executeCLI(const QString &computerName, const QString &command) {
    auto process = new QProcess(this);

    // Show console window (hidden by default)
    process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->flags |= CREATE_NEW_CONSOLE;
        args->startupInfo->dwFlags &=~ static_cast<unsigned long>(STARTF_USESTDHANDLES);
    });

    // Run the command on the target computer through PsExec
    // Command is wrapped in a cmd window to capture remote stdout/stderr
    process->start("cmd.exe /k \"" + psexec + " /accepteula /s \\\\" + computerName + " " + command + "\"");

    // Delete process object when the CMD window closes
    QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [=]() {
        delete process;
    });
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

// Run the systeminfo command on the target machine; print output to the Result pane
void MainWindow::on_buttonSystemInfo_clicked() {
    ui->textResult->setPlainText("");
    auto process = new QProcess(this);

    // We wrap the process in cmd to properly capture the remote computer's stdout
//    process->start("cmd.exe /c \"" + psexec + " /accepteula \\\\" + ui->inputComputer->text() + " systeminfo\"");
    process->start(psexec + " /accepteula \\\\" + ui->inputComputer->text() + " systeminfo");

    qDebug() << "Running systeminfo...";

    QObject::connect(process, &QProcess::readyReadStandardOutput, this, [=]() {
        qDebug() << process->readAllStandardOutput();
//        ui->textResult->setPlainText(ui->textResult->toPlainText() + QString(process->readAllStandardOutput()));
    });

    QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        [=](int exitCode, QProcess::ExitStatus exitStatus) {
            qDebug() << process->readAllStandardError();
            qDebug() << process->readAllStandardOutput();
            delete process;
    });
}

// Run CMD as SYSTEM on the target machine remotely
void MainWindow::on_buttonReverseShell_clicked() {
    executeCLI(ui->inputComputer->text(), "cmd.exe");
}

//
void MainWindow::on_buttonExecuteAction_clicked() {
    actions[ui->dropdownActions->currentIndex]();
}
