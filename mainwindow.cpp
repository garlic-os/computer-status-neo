#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QInputDialog>
#include <QList>
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>

// For the "show console window" shenanigans
#define WIN32_LEAN_AND_MEAN
#include <windows.h>  // CREATE_NEW_CONSOLE, STARTF_USESTDHANDLES

#include "mainwindow.h"
#include "ui_mainwindow.h"

// I hate seeing this long ugly thing everywhere so it's going in its own variable.
// "Process Finished Overload"
const static auto pfo = QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished);


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    tempDir(new QTemporaryDir) {
        updateLabelRunningAs();

        // Unpack the bundled executables and scripts
        if (!tempDir->isValid()) {
            QMessageBox::critical(nullptr, "Failed to start", "Temp directory not valid",
                                 QMessageBox::Ok, QMessageBox::Ok);
            delete tempDir;
            qApp->exit();
        }
        psexec = QDir::toNativeSeparators(tempDir->path() + "/psexec64.exe");
        QFile::copy(":/psexec64.exe", psexec);
        psinfo = QDir::toNativeSeparators(tempDir->path() + "/psinfo64.exe");
        QFile::copy(":/psinfo64.exe", psinfo);

        // Populate the Actions map.
        // NB: If you change/add any action, you must update all of these things:
        //     1. Its entry to this map
        //     2. Its function signature in mainwindow.h
        //     3. Its function implementation here in mainwindow.cpp
        //     4. Its entry in the UI's Actions dropdown menu (or "Combobox", whatever, QT)
        actions = {
            { "System Info", &MainWindow::action_systemInfo },
            { "Query Users", &MainWindow::action_queryUsers },
            { "Reactivate Windows License", &MainWindow::action_reactivateWindows },
            { "Get AD Join Status", &MainWindow::action_getADJoinStatus },
            { "Reinstall Office 365", &MainWindow::action_reinstallOffice365 },
            { "List Installed Printers", &MainWindow::action_listInstalledPrinters },
            { "Install printer...", &MainWindow::action_installPrinter },
            { "Abort Shutdown", &MainWindow::action_abortShutdown },
            { "Shut Down...", &MainWindow::action_shutDown },
            { "Restart...", &MainWindow::action_restart },
            { "Run SFC and DISM", &MainWindow::action_sfcDISM },
            { "List Installed Software", &MainWindow::action_listInstalledSoftware },
            { "Uninstall AppsAnywhere", &MainWindow::action_uninstallAppsAnywhere },
            { "Install AppsAnywhere", &MainWindow::action_installAppsAnywhere }
        };

        // Disable unused help button in dialog windows
        QApplication::setAttribute(Qt::AA_DisableWindowContextHelpButton);

        ui->setupUi(this);
        ui->inputComputer->setFocus();

        // Set default computer query to self
        QProcess process;
        process.start("hostname");
        process.waitForFinished();
        ui->inputComputer->setText(process.readAllStandardOutput().chopped(2).toLower());

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
    return QMessageBox::warning(nullptr, title, message,
                         QMessageBox::Ok | QMessageBox::Cancel,
                         QMessageBox::Cancel) == QMessageBox::Ok;
}

void MainWindow::updateLabelRunningAs() {
    auto process = new QProcess(this);
    process->start("whoami");
    QObject::connect(process, pfo, [=]() {
        ui->labelRunningAs->setText("Running as: " + process->readLine().chopped(2));
        delete process;
    });
}

// Run a remote command in a CMD window
void MainWindow::executeToCMD(const QString &command, bool remote) {
    auto process = new QProcess(this);

    // Show console window (hidden by default)
    process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->flags |= CREATE_NEW_CONSOLE;
        args->startupInfo->dwFlags &=~ static_cast<unsigned long>(STARTF_USESTDHANDLES);
    });

    if (remote) {
        // Run the command on the target computer through PsExec.
        // Command is wrapped in a CMD /k window so the output will stay when the command is done.
        process->start("cmd.exe /k \"" + psexec + " /accepteula /s /n 10 \\\\" + compName() + " " + command + "\"");
    } else {
        process->start(command);
    }

    // Delete process object when the CMD window closes
    QObject::connect(process, pfo, [=]() {
        delete process;
    });
}

// Run a remote command and print its output to the Result pane
void MainWindow::executeToResultPane(const QString &command,
                                     bool remote, bool streamStderr,
                                     const t_callback& callback) {
    setButtonsEnabled(false);
    ui->textResult->setPlainText("Connecting...");
    auto process = new QProcess(this);

    if (remote) {
        // PsExec's remote process's output is only readable by wrapping it in CMD and
        // redirecting the output to a file on the remote machine.
        // It's a long story.
        QString logPath = "\\\\" + compName() + "\\c$\\it-comp-stat.out";
        process->start(psexec + " /accepteula /n 10 \\\\" + compName() + " cmd.exe /c \"" + command + "\" > C:\\it-comp-stat.out");

        // When process finishes
        QObject::connect(process, pfo, [=](int returnCode) {
            switch (returnCode) {
                case 0: {
                    QFile logFile(logPath);
                    logFile.open(QFile::ReadOnly);
                    ui->textResult->setPlainText(logFile.readAll().mid(2));
                    QFile::remove(logPath);
                    break;
                } case 1460: {
                    ui->textResult->setPlainText("Connection timed out.");
                    break;
                }
            }
            setButtonsEnabled(true);
            callback(process);
            delete process;
        });
    } else {
        process->start(command);
        QObject::connect(process, pfo, [=]() {
            ui->textResult->setPlainText(process->readAllStandardOutput());
            qDebug() << process->readAllStandardError();
            setButtonsEnabled(true);
            callback(process);
            delete process;
        });
    }
    if (streamStderr) {
        QObject::connect(process, &QProcess::readyReadStandardError, this, [=]() {
            qDebug() << "Ready Read Standard Error";
            ui->textResult->setPlainText(process->readAllStandardOutput().mid(2));
        });
    }
}

void MainWindow::setButtonsEnabled(bool enabled) {
    static QList<QPushButton *> buttonsList = this->findChildren<QPushButton *>();
    for (int i = 0; i < buttonsList.count(); i++) {
        buttonsList.at(i)->setEnabled(enabled);
    }
}

// Ensure that the computer name is valid.
// This really just means it's a string that the commands it's passed to will
// accept as an argument, and not necessarily a valid computer name. It's not
// looking it up in NetDB or anything.
// Honestly, I just don't want you to type "*" here.
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

void MainWindow::on_dropdownActions_currentTextChanged(const QString &text) {
    bool actionIsForceable = text == "Uninstall AppsAnywhere" || text == "Install AppsAnywhere";
    ui->checkboxForceAction->setCheckable(actionIsForceable);
}

void MainWindow::on_buttonPing_clicked() {
    executeToResultPane("ping /n 1 " + compName(), false);
}

// Start a remote desktop session on the target machine
void MainWindow::on_buttonRemoteDesktop_clicked() {
    QProcess::startDetached("mstsc /v:" + compName());
}

// Offer to start a remote assistance session on the target machine
void MainWindow::on_buttonRemoteAssistance_clicked() {
    QProcess::startDetached("msra /offerRA" + compName());
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

// Look the target machine up in Local Administrator Password Tools
void MainWindow::on_buttonLAPS_clicked() {
    QDesktopServices::openUrl(
        "https://laps.mst.edu/auth-cgi-bin/cgiwrap/mstlaps/search.pl?query=" +
        compName()
    );
}

// Open a CMD window as SYSTEM on the target machine
void MainWindow::on_buttonReverseShell_clicked() {
    executeToCMD("cmd");
}

// Run the function corresponding to (the index of) the currently selected action
void MainWindow::on_buttonExecuteAction_clicked() {
    QString actionName = ui->dropdownActions->currentText();
    t_memberFunction action = actions[actionName.toStdString()];
    (this->*action)();
}

void MainWindow::on_buttonSwitchUser_clicked() {
    ui->textResult->setPlainText("Switch user: not implemented. As a workaround, shift-right-click the executable and click \"Run as different user\".");
}

// Run the `systeminfo` command
void MainWindow::action_systemInfo() {
    executeToResultPane("systeminfo /s " + compName(), false, true);
}

// See who is logged in
void MainWindow::action_queryUsers() {
    executeToResultPane("query user /server:" + compName(), false);
}

// Good ol' KMS
void MainWindow::action_reactivateWindows() {
    if (!confirm("Are you sure you want to reactivate this computer's Windows license?", compName() + ": Reactivate Windows License")) return;
    executeToCMD("slmgr /skms umad-kmsdfs-01.umad.umsystem.edu && slmgr /ato");
}

// See if the computer is on the AD
void MainWindow::action_getADJoinStatus() {
    executeToResultPane("dsregcmd /status");
}

// Run the Microsoft Office 365 reinstall script
void MainWindow::action_reinstallOffice365() {
    if (!confirm("Are you sure you want to reinstall Office 365 on this computer?", compName() + ": Reinstall Microsoft Office 365")) return;
    executeToCMD("R: && cd R:\\software\\appdeploy\\office.365\\x64 && setup.exe /configure configuration-test.xmlcd");
}

// List the printers that are installed on the machine
void MainWindow::action_listInstalledPrinters() {
    executeToResultPane("powershell -c \"Get-WMIObject Win32_Printer\"");
}

// Install a printer by name
void MainWindow::action_installPrinter() {
    bool ok;
    QString printerName = QInputDialog::getText(
        this, compName() + ": Install printer",
        "Enter the name of the printer to install:",
        QLineEdit::Normal, "", &ok, Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);
    if (ok && !printerName.isEmpty()) {
        executeToCMD("rundll32 printui.dll PrintUIEntry /c \\\\" + compName() + " /in /n \\\\winprint.mst.edu\\" + printerName, false);
    }
}

void MainWindow::action_abortShutdown() {
    executeToResultPane("shutdown /a /m \\\\" + compName(), false);
}

void MainWindow::action_shutDown() {
    bool ok;
    int timeoutSeconds = QInputDialog::getInt(
        this, compName() + ": Send shutdown signal",
        "Enter the timeout period (in seconds) before shutdown:",
        0, 0, 2147483647, 1, &ok);
    if (ok) {
        executeToResultPane("shutdown /s /t " + QString::number(timeoutSeconds) + " /m \\\\" + compName(), false);
    }
}

void MainWindow::action_restart() {
    bool ok;
    int timeoutSeconds = QInputDialog::getInt(
        this, compName() + ": Send restart signal",
        "Enter the timeout period (in seconds) before restart:",
        0, 0, 2147483647, 1, &ok);
    if (ok) {
        executeToResultPane("shutdown /r /t " + QString::number(timeoutSeconds) + " /m \\\\" + compName(), false);
    }
}

void MainWindow::action_sfcDISM() {
    executeToCMD("cmd /c \"sfc /scannow && dism /online /cleanup-image /restorehealth\"");
}

void MainWindow::action_listInstalledSoftware() {
    executeToResultPane(psinfo + " /accepteula /nobanner /s applications \\\\" + compName(), false);
}

// Run the AppsAnywhere uninstall script, or the "force" version if
// the Force checkbox is checked
void MainWindow::action_uninstallAppsAnywhere() {
    if (!confirm("Are you sure to want to uninstall AppsAnywhere?", compName() + ": Uninstall AppsAnywhere")) return;
    if (ui->checkboxForceAction->checkState() == Qt::Checked) {
        // Force
        executeToCMD("R:\\software\\itwindist\\appsanywheredeploy.1_5_0\\force_uninstall.bat");
    } else {
        // Not force
        executeToCMD("/c perl R:\\software\\itwindist\\applications\\SCCM\\appsanywheredeploy.1_5_0\\remove.pl");
    }
void MainWindow::action_listNetworkDrives() {
    executeToResultPane("Get-WmiObject Win32_MappedLogicalDisk -ComputerName " + compName() + " | Select PSComputerName, Name, ProviderName");
}

// Run the AppsAnywhere install script, and don't check if it might already
// be installed if the Force checkbox is checked
void MainWindow::action_installAppsAnywhere() {
    if (!confirm("Are you sure to want to install AppsAnywhere?", compName() + ": Install AppsAnywhere")) return;
    if (ui->checkboxForceAction->checkState() == Qt::Checked) {
        // Force
        executeToCMD("/c perl R:\\software\\itwindist\\applications\\SCCM\\appsanywheredeploy.1_5_0\\update.pl --no-install-check");
    } else {
        // Not force
        executeToCMD("/c perl R:\\software\\itwindist\\applications\\SCCM\\appsanywheredeploy.1_5_0\\update.pl");
    }
void MainWindow::action_listPhysicalDrives() {
    executeToResultPane("Get-WmiObject -Class MSFT_PhysicalDisk -ComputerName " + compName() + " -Namespace root\\Microsoft\\Windows\\Storage | Select FriendlyName");
}

void MainWindow::action_getSerialNumber() {
    executeToResultPane("Get-WmiObject win32_bios | Select SerialNumber");
}

void MainWindow::action_getBIOSVersion() {
    executeToResultPane("Get-WmiObject win32_bios | Select Name");
}
