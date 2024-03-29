#ifndef MAINWINDOW_H
#define MAINWINDOW_H

// For development
//#define QSS_HOT_RELOAD

#include <string>
#include <unordered_map>
#include <QList>
#include <QMainWindow>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QSharedPointer>
#include <QString>
#include <QTemporaryDir>
#include <QTimer>

#ifdef QSS_HOT_RELOAD
    #include <QFileSystemWatcher>
#endif

enum ExecutionType { LOCAL, REMOTE, /*DOUBLE_HOP*/ };

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    virtual ~MainWindow();

private slots:
    void on_inputComputer_textChanged(const QString &text);
    void on_inputComputer_returnPressed();
    void on_buttonPing_clicked();
    void on_buttonRemoteDesktop_clicked();
    void on_buttonRemoteAssistance_clicked();
    void on_buttonComputerManagement_clicked();
    void on_buttonCDollarAdminShare_clicked();
    void on_buttonNetDB_clicked();
    void on_buttonLAPS_clicked();
    void on_buttonReverseShell_clicked();
    void on_buttonExecuteAction_clicked();
    void on_buttonSwitchUser_clicked();
    void on_buttonCopy_clicked();
    void on_buttonClear_clicked();

private:
    QSharedPointer<Ui::MainWindow> ui;
    QSharedPointer<QSettings> settings;
    QSharedPointer<QProcess> runner;
    QSharedPointer<QTimer> runnerTimer;
    QSharedPointer<QProcess> consoleRunner;  // Separate process for commands that show the console
    QList<QPushButton *> buttonsList;

    #ifdef QSS_HOT_RELOAD
        QSharedPointer<QFileSystemWatcher> qssWatcher;
        void enableQSSHotReload();
    #endif

    // Setup functions
    void setupRunner(QProcess *process);
    void setupRunnerTimer(QTimer *timer, QProcess *runner);
    void setupConsoleRunner(QProcess *process);

    const static int DEFAULT_TIMEOUT_MS = 1000 * 10;  // 10 seconds

    // Helper functions
    void executeToNewWindow(const QString &command, ExecutionType executionType);
    void executeToResultPane(const QString &command, ExecutionType executionType,
                             int timeout_ms=DEFAULT_TIMEOUT_MS);
    inline const QString compName() const;
    bool confirm(const QString &message, const QString &title) const;

    // To keep any other commands from being run while something is printing to
    // the result pane
    void setButtonsEnabled(bool enabled=true);

    // Functions accessible from the Actions dropdown menu
    typedef void (MainWindow::*t_memberFunction)(void);
    std::unordered_map<std::string, t_memberFunction> actions;
    void action_systemInfo();
    void action_queryUsers();
    void action_reactivateWindows();
    void action_getADJoinStatus();
//    void action_reinstallOffice365();
    void action_listInstalledPrinters();
//    void action_installPrinter();
    void action_abortShutdown();
    void action_shutDown();
    void action_restart();
    void action_sfcDISM();
    void action_listInstalledSoftware();
//    void action_uninstallAppsAnywhere();
//    void action_installAppsAnywhere();
    void action_listNetworkDrives();
    void action_listPhysicalDrives();
    void action_getSerialNumber();
    void action_getBIOSVersion();
};

#endif // MAINWINDOW_H
