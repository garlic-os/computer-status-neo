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

    typedef void (*t_action)();  // Pointer to a void input and output function
    t_action actions = {
        &this->action_SystemInfo
    };

    void executeCLI(const QString &computerName, const QString &command);
    void disableButtons();
    void enableButtons();

    void action_SystemInfo();
};

#endif // MAINWINDOW_H
