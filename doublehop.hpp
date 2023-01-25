#ifndef DOUBLEHOP_HPP
#define DOUBLEHOP_HPP

#include <QDebug>
#define log qDebug().nospace().noquote()

#include <QFile>
#include <QFileSystemWatcher>
#include <QObject>
#include <QProcess>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>

#include <propvarutil.h>
#include <taskschd.h>
#include "./CComPtr.h"  // MinGW-compatible polyfill for MSVC's COM Object Smart Pointer


typedef enum _TASK_RUN_FLAGS {
    TASK_RUN_NO_FLAGS = 0x00,
    TASK_RUN_AS_SELF = 0x01,
    TASK_RUN_IGNORE_CONSTRAINTS = 0x02,
    TASK_RUN_USE_SESSION_ID = 0x04,
    TASK_RUN_USER_SID = 0x08
} TASK_RUN_FLAGS;


inline wchar_t *wide(const QString &str) {
    return str.toStdWString().data();
}

inline wchar_t *wide(const wchar_t *str) {
    return const_cast<wchar_t *>(str);
}


QString errorMessage(const char *functionName) {
    wchar_t *message = nullptr;
    unsigned long errorCode = GetLastError();
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<wchar_t *>(&message),
        0,
        nullptr
    );
    QString result;
    QTextStream(&result) << "Error in API:  " << functionName << ".\n"
                         << "  Error code:  " << errorCode << ".\n"
                         << "     Message:  " << QString::fromWCharArray(message);
    LocalFree(message);
    return result;
}


VARIANT nullVariant() {
    VARIANT var;
    VariantInit(&var);
    var.vt = VT_NULL;
    return var;
}


VARIANT strVariant(const QString &str) {
    // InitVariantFromString doesn't exist in MinGW 🤷
    VARIANT var;
    InitVariantFromBuffer(
        str.toStdWString().c_str(),
        str.toUtf8().size(),
        &var
    );
    return var;
}


bool touch(const QString &path) {
    QFile file(path);
    if (!file.open(QFile::WriteOnly)) return false;
    file.close();
    return true;
}


// Enable a command to do a "double hop" -- to access a remote resource
// from the remote computer (like running a script from DFS) without
// encountering the Double Hop problem.
// Splits the double hop into two single hops by loading the command into
// a task in Task Scheduler, which changes what Windows thinks is the origin
// of the command from your computer to the remote computer.
QSharedPointer<QThread> doubleHop(const QString &computerName, const QString &command) {
    HRESULT result;
    wchar_t *taskName = wide(L"Computer Status Neo");
    QString outputFile = "comp-stat-neo-test.log";
    QString outputPathLocal = "C:\\" + outputFile;
    QString outputPathRemote = "\\\\" + computerName + "\\c$\\" + outputFile;

    log << "Task Scheduler: Creating new task...";
    CComPtr<ITaskService> service;
    CComPtr<ITaskDefinition> taskDefinition;
    service->NewTask(0, &taskDefinition);

    log << "Task Scheduler: Configuring task with the given command...";
    QProcess whoami;
    whoami.startCommand("whoami");
    CComPtr<IPrincipal> principle;
    taskDefinition->get_Principal(&principle);
    principle->put_RunLevel(TASK_RUNLEVEL_HIGHEST);

    CComPtr<IRegistrationInfo> registrationInfo;
    registrationInfo->put_Description(taskName);
    registrationInfo->put_Author(wide(whoami.readAllStandardOutput()));
    whoami.waitForFinished();

    CComPtr<ITaskSettings> settings;
    settings->put_Enabled(true);
    settings->put_StartWhenAvailable(true);
    settings->put_DisallowStartIfOnBatteries(false);
    settings->put_StopIfGoingOnBatteries(false);
    settings->put_WakeToRun(true);
    settings->put_Hidden(false);

    CComPtr<IActionCollection> actions;
    CComPtr<IExecAction> action;
    actions->Create(TASK_ACTION_EXEC, reinterpret_cast<IAction**>(&action));
    action->put_Path(wide(L"powershell"));
    action->put_Arguments(("-Command \"& {" + command + "}\" *> " + outputPathLocal).toStdWString().data());

    log << "Task Scheduler: Connecting to " + computerName + "...";
    result = service->Connect(
        strVariant(computerName),
        nullVariant(),
        nullVariant(),
        nullVariant()
    );
    if (result != S_OK) {
        throw errorMessage("IService::Connect");
    }

    CComPtr<ITaskFolder> folder;
    CComPtr<IRegisteredTask> task;
    service->GetFolder(wide(L"\\"), &folder);
    folder->RegisterTaskDefinition(
        taskName,
        taskDefinition,
        TASK_CREATE_OR_UPDATE,
        strVariant("SYSTEM"),
        nullVariant(),
        TASK_LOGON_PASSWORD,
        nullVariant(),
        &task
    );

    // Create the output file or clear it on the off-chance that it exists
    touch(outputPathRemote);

    log << "Task Scheduler: Triggering task...";
    result = task->RunEx(
        nullVariant(),
        TASK_RUN_AS_SELF | TASK_RUN_IGNORE_CONSTRAINTS,
        0,
        nullptr,
        nullptr
    );
    if (result != S_OK) {
        throw errorMessage("IRegisteredTask::RunEx");
    }

    return QSharedPointer<QThread>(
        QThread::create([=]() {
            auto file = QSharedPointer<QFile>(new QFile(outputPathRemote));
            file->open(QFile::ReadOnly);
            QFileSystemWatcher outputWatcher;
            outputWatcher.addPath(outputPathRemote);
            QFileSystemWatcher::connect(&outputWatcher, &QFileSystemWatcher::fileChanged, [=]() {
                log << file->readAll();
            });

            TASK_STATE state;
            HRESULT result;
            QTimer timer;
            do {
                result = task->get_State(&state);
                if (result != S_OK) {
                    throw errorMessage("IRegisteredTask::get_State");
                }
            } while (state == TASK_STATE_RUNNING);
            file->close();
        })
    );
}


#endif  // DOUBLEHOP_HPP
