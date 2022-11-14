/**
 * Computer Status Neo
 * https://github.com/the-garlic-os/computer-status-neo
 */

#include <QDebug>
#define log qDebug().nospace().noquote()

#include <QByteArray>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QInputDialog>
#include <QMessageBox>
#include <QMetaEnum>
#include <QObject>
#include <QRegularExpression>
#include <QTemporaryDir>

// For the "show console window" shenanigans
#include <windows.h>  // CREATE_NEW_CONSOLE, STARTF_USESTDHANDLES

#include "./mainwindow.h"
#include "./ui_mainwindow.h"
#include "./switchuser.hpp"


QString processErrorDump(QProcess *process) {
    QMetaEnum pErrorMeta = QMetaEnum::fromType<QProcess::ProcessError>();
    return "Command line: " + process->program() + ' ' + process->arguments().join(' ') + '\n' +
           "QProcess error code: " + pErrorMeta.valueToKey(process->error()) + '\n' +
           "Return code: " + QString::number(process->exitCode()) + "\n\nstdout:\n" +
           process->readAllStandardOutput() + "\n\nstderr:\n" +
           process->readAllStandardError();
}


QByteArray processOutput(QByteArray text) {
    // Remove leading CRs and LFs
    while (text.startsWith('\r') || text.startsWith('\n')) {
        text = text.mid(1);
    }
    return text;
}


// Load the common QSS. If system is on dark theme, also load the dark theme QSS.
void loadTheme() {
    log << "Loading common styles...";
    QFile themeFile(":/style/common.qss");
    themeFile.open(QFile::ReadOnly | QFile::Text);
    QTextStream themeFileStream(&themeFile);
    QString qss = themeFileStream.readAll();
    QSettings osThemeSettings(
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        QSettings::NativeFormat
    );
    if (osThemeSettings.value("AppsUseLightTheme", 1).toInt() == 0) {
        log << "Loading dark theme overrides...";
        QFile themeFile(":/style/dark.qss");
        themeFile.open(QFile::ReadOnly | QFile::Text);
        QTextStream themeFileStream(&themeFile);
        qss += themeFileStream.readAll();
    }
    log << "Applying styles...";
    qApp->setStyleSheet(qss);
    log << "Stylesheets successfully applied.";
}


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    settings(new QSettings(QSettings::UserScope, "Missouri S&T IT Help Desk", "Computer Status Neo")),
//    qssWatcher(new QFileSystemWatcher),
    runner(new QProcess(this)),
    runnerTimer(new QTimer(this)),
    consoleRunner(new QProcess(this)) {
        // Populate the Actions map.
        // NB: If you change/add any action, you must update all of these things:
        //     1. Its entry in this map
        //     2. Its function signature in mainwindow.h
        //     3. Its function implementation here in mainwindow.cpp
        //     4. Its entry in the UI's Actions dropdown menu (or "Combobox", whatever, QT)
        actions = {
            { "System info", &MainWindow::action_systemInfo },
            { "Query users", &MainWindow::action_queryUsers },
            { "Reactivate Windows license", &MainWindow::action_reactivateWindows },
            { "Get AD join status", &MainWindow::action_getADJoinStatus },
            { "Reinstall Office 365", &MainWindow::action_reinstallOffice365 },
            { "List installed printers", &MainWindow::action_listInstalledPrinters },
            { "Install printer...", &MainWindow::action_installPrinter },
            { "Abort shutdown", &MainWindow::action_abortShutdown },
            { "Shut down...", &MainWindow::action_shutDown },
            { "Restart...", &MainWindow::action_restart },
            { "Run SFC and DISM", &MainWindow::action_sfcDISM },
            { "List installed software", &MainWindow::action_listInstalledSoftware },
//            { "Uninstall AppsAnywhere", &MainWindow::action_uninstallAppsAnywhere },
//            { "Install AppsAnywhere", &MainWindow::action_installAppsAnywhere },
            { "List network drives", &MainWindow::action_listNetworkDrives },
            { "List physical drives", &MainWindow::action_listPhysicalDrives },
            { "Get serial number", &MainWindow::action_getSerialNumber },
            { "Get BIOS version", &MainWindow::action_getBIOSVersion }
        };

        ui->setupUi(this);  // Boilerplate
        ui->inputComputer->setFocus();  // Autofocus query box

        if (settings->contains("run-as-user")) {
            on_buttonSwitchUser_clicked();
        }

        // Set default computer query to self
        runner->startCommand("hostname");
        runner->waitForFinished();
        ui->inputComputer->setText(runner->readAllStandardOutput().chopped(2).toLower());

        // Set the "Running as" label
        runner->startCommand("whoami");
        runner->waitForFinished();
        ui->labelRunningAs->setText("Running as: " + runner->readLine().chopped(2));
        runner->readAllStandardOutput(); // Clear stdout for next process

        // Attach event listeners and set configuration for the global process objects
        setupRunner(runner.data());
        setupRunnerTimer(runnerTimer.data(), runner.data());
        setupConsoleRunner(consoleRunner.data());

        loadTheme();

        // For development: hot reload stylesheets from FS
//        qssWatcher->addPath("S:\\Documents\\Qt\\ComputerStatusNeo\\style\\common.qss");
//        qssWatcher->addPath("S:\\Documents\\Qt\\ComputerStatusNeo\\style\\dark.qss");
//        QFileSystemWatcher::connect(qssWatcher.data(), &QFileSystemWatcher::fileChanged, this, [=, this]() {
//            log << "Loading common styles...";
//            QFile themeFile("S:/Documents/Qt/ComputerStatusNeo/style/common.qss");
//            themeFile.open(QFile::ReadOnly | QFile::Text);
//            QTextStream themeFileStream(&themeFile);
//            QString qss = themeFileStream.readAll();
//            if (true) {
//                log << "Loading dark theme overrides...";
//                QFile themeFile("S:/Documents/Qt/ComputerStatusNeo/style/dark.qss");
//                themeFile.open(QFile::ReadOnly | QFile::Text);
//                QTextStream themeFileStream(&themeFile);
//                qss += themeFileStream.readAll();
//            }
//            log << "Applying styles...";
//            qApp->setStyleSheet(qss);
//            log << "Stylesheets successfully applied.";
//        });

        buttonsList = this->findChildren<QPushButton *>();
}


MainWindow::~MainWindow() {}


void MainWindow::setupRunner(QProcess *process) {
    // When process finishes
    QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [=, this]() {
        setButtonsEnabled(true);
        if (ui->textResult->toPlainText() == "Connecting...") {
            ui->textResult->setPlainText("(No output.)");
        }
    });

    // When process has new content in stdout
    QObject::connect(process, &QProcess::readyReadStandardOutput, this, [=, this]() {
        if (ui->textResult->toPlainText() == "Connecting...") {
            ui->textResult->setPlainText("");
        }
        ui->textResult->setPlainText(
            ui->textResult->toPlainText() +
            processOutput(process->readAllStandardOutput())
        );
    });

    // When process has new content in stderr
    QObject::connect(process, &QProcess::readyReadStandardError, this, [=, this]() {
        if (ui->textResult->toPlainText() == "Connecting...") {
            ui->textResult->setPlainText("");
        }
        ui->textResult->setPlainText(
            ui->textResult->toPlainText() +
            processOutput(process->readAllStandardError())
        );
    });

    // If something goes wrong
    QProcess::connect(process, &QProcess::errorOccurred, this, [=, this]() {
        setButtonsEnabled(true);
        QString output = processErrorDump(process) + "\n---\n\n";
        if (ui->textResult->toPlainText() != "Connecting...") {
            output += ui->textResult->toPlainText();
        }
        ui->textResult->setPlainText(output);
    });
}


// Kill the running process if it spins for too long
void MainWindow::setupRunnerTimer(QTimer *timer, QProcess *runner) {
    QTimer::connect(timer, &QTimer::timeout, this, [=, this]() {
        if (runner->state() != QProcess::NotRunning) {
            ui->textResult->setPlainText("Process timed out.");
            runner->kill();
        }
    });
}


void MainWindow::setupConsoleRunner(QProcess *process) {
    // Show console window (hidden by default)
    process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->flags |= CREATE_NEW_CONSOLE;
        args->startupInfo->dwFlags &=~ static_cast<unsigned long>(STARTF_USESTDHANDLES);
    });

    // If something goes wrong
    QProcess::connect(process, &QProcess::errorOccurred, this, [=, this]() {
        switch (process->exitCode()) {
            case -1073741510:
                // Reverse Shell returns this when everything is fine 🤷
                return;
            default:
                ui->textResult->setPlainText(
                    "Error occurred while running the command\n" +
                    processErrorDump(process)
                );
        }
    });
}


inline const QString MainWindow::compName() const {
    return ui->inputComputer->text();
}


bool MainWindow::confirm(const QString &message, const QString &title) const {
// bool MainWindow::confirm(const QString &message, const QString &title, const QString&okText, const QString &cancelText) const {
    return QMessageBox::warning(nullptr, title, message,
                         QMessageBox::Ok | QMessageBox::Cancel,
                         QMessageBox::Cancel) == QMessageBox::Ok;
}


// Run a remote command and show the output in a local Powershell window.
void MainWindow::executeToNewWindow(const QString &command, bool remote) {
    QString wrappedCommand = remote ?
                "powershell -NoExit Invoke-Command -ComputerName " + compName() + " -ScriptBlock { " + command + " }" :
                "powershell -NoExit " + command;
    log << "Running command: " << wrappedCommand;
    consoleRunner->startCommand(wrappedCommand);
}


// Run a remote command and print its output to the Result pane.
void MainWindow::executeToResultPane(const QString &command, bool remote, int timeout_ms) {
    setButtonsEnabled(false);  // Will be re-enabled when the process finishes
    ui->textResult->setPlainText("Connecting...");

    QString wrappedCommand = remote ?
                "powershell Invoke-Command -ComputerName " + compName() + " -ScriptBlock { " + command + " }" :
                "powershell " + command;
    log << "Running command: " << wrappedCommand;
    runner->startCommand(wrappedCommand);

    if (timeout_ms >= 0) {
        runnerTimer->start(timeout_ms);
    }
}


void MainWindow::setButtonsEnabled(bool enabled) {
    for (const auto button : buttonsList) {
        button->setEnabled(enabled);
    }
}

// Ensure that the computer name is valid.
// This really just means it's a string that the commands it's passed to will
// accept as an argument, and not necessarily a valid computer name. It's not
// looking it up in NetDB or anything.
void MainWindow::on_inputComputer_textChanged(const QString &text) {
    static QRegularExpression pattern("\\w");
    if (text.contains(pattern)) {  // TODO: Only run when necessary
        setButtonsEnabled(true);
    } else {
        setButtonsEnabled(false);
    }
}

void MainWindow::on_inputComputer_returnPressed() {
    on_buttonExecuteAction_clicked();
}

void MainWindow::on_buttonPing_clicked() {
    executeToResultPane("ping /n 1 " + compName());
}

// Start a remote desktop session on the target machine
void MainWindow::on_buttonRemoteDesktop_clicked() {
    executeToResultPane("mstsc /v:" + compName());
}

// Offer to start a remote assistance session on the target machine
void MainWindow::on_buttonRemoteAssistance_clicked() {
    executeToResultPane("msra /offerRA " + compName());
}

// Open Computer Management locally, for the target machine
void MainWindow::on_buttonComputerManagement_clicked() {
    executeToResultPane("compmgmt.msc /computer=" + compName());
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

// Look the target machine up in Local Administrator Password Tools
void MainWindow::on_buttonLAPS_clicked() {
    QDesktopServices::openUrl(
        "https://laps.mst.edu/auth-cgi-bin/cgiwrap/mstlaps/search.pl?query=" +
        compName()
    );
}

// Enter a Powershell session on the target machine
void MainWindow::on_buttonReverseShell_clicked() {
    executeToNewWindow("Enter-PSSession –ComputerName " + compName());
}

// Run the function corresponding to (the index of) the currently selected action
void MainWindow::on_buttonExecuteAction_clicked() {
    QString actionName = ui->dropdownActions->currentText();
    t_memberFunction action = actions[actionName.toStdString()];
    (this->*action)();
}

// Re-run Computer Status as a different user.
void MainWindow::on_buttonSwitchUser_clicked() {
    ui->textResult->setPlainText("");
    UserSwitcher userSwitcher;
    QString savedUsername = settings->value("run-as-user", "").toString();
    auto [ result, newUsername ] = userSwitcher.switchUser(savedUsername);
    if (result == "Success") {
        settings->setValue("run-as-user", newUsername);
        qApp->quit();
    } else {
        settings->remove("run-as-user");
        ui->textResult->setPlainText(result);
    }
}

void MainWindow::on_buttonCopy_clicked() {
    static QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setText(ui->textResult->toPlainText());
}

void MainWindow::on_buttonClear_clicked() {
    ui->textResult->setPlainText("");
}

// Run the `systeminfo` command
void MainWindow::action_systemInfo() {
    executeToResultPane("systeminfo /s " + compName());
}

// See who is logged in
void MainWindow::action_queryUsers() {
    executeToResultPane("query user /server:" + compName());
}

// Good ol' KMS
void MainWindow::action_reactivateWindows() {
    if (!confirm("Are you sure you want to reactivate this computer's Windows license?", compName() + ": Reactivate Windows License")) return;
    executeToNewWindow(
        "slmgr " + compName() + " /skms umad-kmsdfs-01.umad.umsystem.edu; "
        "slmgr " + compName() + " /ato"
    );
}

// See if the computer is on the AD
void MainWindow::action_getADJoinStatus() {
    executeToResultPane("dsregcmd /status", true);
}

// Run the Microsoft Office 365 reinstall script
void MainWindow::action_reinstallOffice365() {
    if (!confirm("Are you sure you want to reinstall Office 365 on this computer?", compName() + ": Reinstall Microsoft Office 365")) return;
    executeToNewWindow(
        "pushd \\\\minerfiles.mst.edu\\dfs\\software\\appdeploy\\office.365\\x64; "
        "setup.exe /configure configuration-test.xmlcd",
        true
    );
}

// List the printers that are installed on the machine
void MainWindow::action_listInstalledPrinters() {
    executeToResultPane("Get-WMIObject Win32_Printer -ComputerName " + compName() + " | Select Name, Location, Comment");
}

// Install a printer by name
void MainWindow::action_installPrinter() {
    bool ok;
    QString printerName = QInputDialog::getText(
        this, compName() + ": Install printer",
        "Enter the name of the printer to install:",
        QLineEdit::Normal, "", &ok, Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);
    if (ok && !printerName.isEmpty()) {
        executeToNewWindow(
            "echo 'Adding printer \"" + printerName + "\" to computer \"" + compName() + "\" ...'; "
            "Add-Printer"
            " -ComputerName " + compName() +
            " -IppURL '" + printerName + ".prn.mst.edu'; "
            "echo Done.; "
            "Get-WMIObject Win32_Printer -ComputerName " + compName() + " | Select Name, Location, Comment"
        );
    }
}

void MainWindow::action_abortShutdown() {
    executeToResultPane("shutdown /a /m \\\\" + compName());
}

void MainWindow::action_shutDown() {
    bool ok;
    int timeoutSeconds = QInputDialog::getInt(
        this, compName() + ": Send shutdown signal",
        "Enter the timeout period (in seconds) before shutdown:",
        0, 0, 2147483647, 1, &ok);
    if (ok) {
        executeToResultPane("shutdown /s /t " + QString::number(timeoutSeconds) + " /m \\\\" + compName());
    }
}

void MainWindow::action_restart() {
    bool ok;
    int timeoutSeconds = QInputDialog::getInt(
        this, compName() + ": Send restart signal",
        "Enter the timeout period (in seconds) before restart:",
        0, 0, 2147483647, 1, &ok);
    if (ok) {
        executeToResultPane("shutdown /r /t " + QString::number(timeoutSeconds) + " /m \\\\" + compName());
    }
}

void MainWindow::action_sfcDISM() {
    executeToNewWindow("sfc /scannow; dism /online /cleanup-image /restorehealth", true);
}

void MainWindow::action_listInstalledSoftware() {
    // this command takes a while
    executeToResultPane("Get-WmiObject Win32_Product -ComputerName " + compName() + " | Select Name", false, DEFAULT_TIMEOUT_MS * 2);
}

// Run the AppsAnywhere uninstall script
//void MainWindow::action_uninstallAppsAnywhere() {
//    if (!confirm("Are you sure to want to uninstall AppsAnywhere?", compName() + ": Uninstall AppsAnywhere")) return;
//    executeToNewWindow("\\\\minerfiles.mst.edu\\dfs\\software\\itwindist\\sccm\\Packages\\AppsAnywhere.1_6_0\\Uninstall.cmd", true, "/i");
//}

// Run the AppsAnywhere install script
//void MainWindow::action_installAppsAnywhere() {
//    if (!confirm("Are you sure to want to install AppsAnywhere?", compName() + ": Install AppsAnywhere")) return;
//    QProcess::startDetached("powershell -c \"Enable-WSManCredSSP -Role Client -DelegateComputer" + compName() + "\"");  // TODO: Race condition. Wait for this to finish before continuing
//    executeToNewWindow("\\\\minerfiles.mst.edu\\dfs\\software\\itwindist\\sccm\\Packages\\AppsAnywhere.1_6_0\\Install.cmd", true, "/c");
//    // https://community.idera.com/database-tools/powershell/powertips/b/tips/posts/solving-double-hop-remoting-with-credssp
//}

void MainWindow::action_listNetworkDrives() {
    executeToResultPane(
        "Get-WmiObject Win32_MappedLogicalDisk"
        " -ComputerName " + compName() +
        " | Select PSComputerName, Name, ProviderName"
    );
}

void MainWindow::action_listPhysicalDrives() {
    executeToResultPane(
        "Get-WmiObject -Class MSFT_PhysicalDisk"
        " -ComputerName " + compName() +
        " -Namespace root\\Microsoft\\Windows\\Storage"
        " | Select FriendlyName"
    );
}

void MainWindow::action_getSerialNumber() {
    executeToResultPane("Get-WmiObject win32_bios | Select SerialNumber");
}

void MainWindow::action_getBIOSVersion() {
    executeToResultPane("Get-WmiObject win32_bios | Select Name");
}
