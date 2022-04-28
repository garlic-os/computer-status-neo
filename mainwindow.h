#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QTemporaryDir>
#include <QString>
#include <QMainWindow>

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    virtual ~MainWindow();

private slots:
    void on_inputComputer_textChanged();
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
    bool buttonsEnabled;

    void updateLabelRunningAs();

    // Helper functions
    void executeCLI(const QString &command);
    void executeToResultPane(const QString &command);
    inline const QString compName() const;
    bool confirm(const QString &message, const QString &title) const;

    // To keep any other commands from being run while something is printing to
    // the result pane
    void disableButtons();
    void enableButtons();

    // Functions accessible from the Actions dropdown menu
    typedef void (MainWindow::*t_memberFunction)(void);
    t_memberFunction actions[8];
    void action_systemInfo();
    void action_reactivateWindows();
    void action_getADJoinStatus();
    void action_reinstallOffice365();
    void action_installPrinter();
    void action_shutDown();
    void action_restart();
    void action_sfcDISM();
};

#endif // MAINWINDOW_H
