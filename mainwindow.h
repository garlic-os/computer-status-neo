#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <functional>
#include <string>
#include <unordered_map>
#include <QMainWindow>
#include <QProcess>
#include <QString>
#include <QTemporaryDir>

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
    void on_dropdownActions_currentTextChanged(const QString &text);
    void on_buttonPing_clicked();
    void on_buttonRemoteDesktop_clicked();
    void on_buttonRemoteAssistance_clicked();
    void on_buttonComputerManagement_clicked();
    void on_buttonCDollarAdminShare_clicked();
    void on_buttonNetDB_clicked();
    void on_buttonReverseShell_clicked();
    void on_buttonExecuteAction_clicked();

private:
    Ui::MainWindow *ui;
    QTemporaryDir *tempDir;  // Temporary directory for storing psexec.exe
    QString psexec;  // Path to unpacked psexec.exe

    void updateLabelRunningAs();

    // Helper functions
    using t_callback = std::function<void (const QProcess*)>;
    const static inline t_callback dummy = [](const QProcess*){};

    void executeCLI(const QString &command, bool remote=true);
    void executeToResultPane(const QString &command, bool remote=true, bool streamStderr=false, const t_callback& callback=dummy);
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
    void action_reinstallOffice365();
    void action_getInstalledPrinters();
    void action_installPrinter();
    void action_shutDown();
    void action_restart();
    void action_sfcDISM();
    void action_uninstallAppsAnywhere();
    void action_installAppsAnywhere();
};

#endif // MAINWINDOW_H
