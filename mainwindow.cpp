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
    ui(new Ui::MainWindow) {
        updateLabelRunningAs();

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

// Run a remote command and show the output in a local Powershell window.
void MainWindow::executeToNewWindow(const QString &command, bool remote,
                                    const t_callback& callback) {
    auto process = new QProcess(this);

    // Show console window (hidden by default)
    process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->flags |= CREATE_NEW_CONSOLE;
        args->startupInfo->dwFlags &=~ static_cast<unsigned long>(STARTF_USESTDHANDLES);
    });

    auto wrapped_command = remote ?
                "powershell -NoExit Invoke-Command -ComputerName " + compName() + " -ScriptBlock { " + command + " }" :
                "powershell -NoExit " + command;
    qDebug() << "Running command: " << wrapped_command;
    process->start(wrapped_command);

    // Delete process object when the PS window closes
    QObject::connect(process, pfo, [=]() {
        qDebug() << processErrorDump(process);
        callback(process);
        delete process;
    });

    QProcess::connect(process, &QProcess::errorOccurred, [=]() {
        ui->textResult->setPlainText(
            "Error occurred while running the command\n" +
            processErrorDump(process)
        );
    });
}


// Run a remote command and print its output to the Result pane.
void MainWindow::executeToResultPane(const QString &command, bool remote,
                                     const t_callback& callback) {
    setButtonsEnabled(false);
    ui->textResult->setPlainText("Connecting...");
    auto process = new QProcess(this);
    auto wrapped_command = remote ?
                "powershell Invoke-Command -ComputerName " + compName() + " -ScriptBlock { " + command + " }" :
                "powershell " + command;
    qDebug() << "Running command: " << wrapped_command;
    process->start(wrapped_command);

    // When process finishes
    QObject::connect(process, pfo, [=]() {
        QByteArray text = process->readAllStandardOutput();
        if (text.length() == 0) {
            text = process->readAllStandardError();
        }
//        qDebug() << text;
        while (text.startsWith("\r") || text.startsWith("\n")) {  // Remove leading CRs and LFs
//            qDebug() << "Leading CR/LF detected";
            text = text.mid(1);
        }
        if (text.length() == 0) {
            text = "(No output.)";
        }
        ui->textResult->setPlainText(text);
        setButtonsEnabled(true);
        callback(process);
        delete process;
    });

    QProcess::connect(process, &QProcess::errorOccurred, [=]() {
        ui->textResult->setPlainText(processErrorDump(process));
    });
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
    executeToResultPane("ping /n 1 " + compName());
}

// Start a remote desktop session on the target machine
void MainWindow::on_buttonRemoteDesktop_clicked() {
    QProcess::startDetached("mstsc /v:" + compName());
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

void MainWindow::on_buttonSwitchUser_clicked() {
    ui->textResult->setPlainText("Switch user: not implemented. As a workaround, shift-right-click the executable and click \"Run as different user\".");
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
    executeToNewWindow("slmgr " + compName() + " /skms umad-kmsdfs-01.umad.umsystem.edu; slmgr " + compName() + " /ato");
}

// See if the computer is on the AD
void MainWindow::action_getADJoinStatus() {
    executeToResultPane("dsregcmd /status", true);
}

// Run the Microsoft Office 365 reinstall script
void MainWindow::action_reinstallOffice365() {
    if (!confirm("Are you sure you want to reinstall Office 365 on this computer?", compName() + ": Reinstall Microsoft Office 365")) return;
    executeToNewWindow("pushd \\\\minerfiles.mst.edu\\dfs\\software\\appdeploy\\office.365\\x64; setup.exe /configure configuration-test.xmlcd", true);
}

// List the printers that are installed on the machine
void MainWindow::action_listInstalledPrinters() {
    executeToResultPane("Get-WMIObject Win32_Printer -ComputerName " + compName() + " | Select Name, Location");
}

// Install a printer by name
void MainWindow::action_installPrinter() {
    bool ok;
    QString printerName = QInputDialog::getText(
        this, compName() + ": Install printer",
        "Enter the name of the printer to install:",
        QLineEdit::Normal, "", &ok, Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);
    if (ok && !printerName.isEmpty()) {
        executeToNewWindow("rundll32 printui.dll,PrintUIEntry /c \\\\" + compName() + " /in /n \\\\winprint.mst.edu\\" + printerName);
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
    executeToResultPane("Get-WmiObject Win32_MappedLogicalDisk -ComputerName " + compName() + " | Select PSComputerName, Name, ProviderName");
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
