#include <windows.h>
#include <wincred.h>
#include <QDir>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTextStream>
#include <tuple>

#include "./ui_mainwindow.h"  // qApp

#define CRED_PACK_PROTECTED_CREDENTIALS 0x1


// Nightmare
class UserSwitcher {
  private:
    void *inAuthBuf = 0;
    void *outAuthBuf = 0;
    unsigned long inAuthBufSize = 0;
    unsigned long outAuthBufSize = 0;
    wchar_t *username = nullptr;
    // Domain field is unused; packed into the username as "domain\username" instead 🤷‍
    wchar_t *password = nullptr;
    unsigned long usernameSize = 0;
    unsigned long domainSize = 0;
    unsigned long passwordSize = 0;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;

    static QString errorMessage(const char *functionName) {
        wchar_t *message = nullptr;
        unsigned long errorCode = GetLastError();
        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            GetLastError(),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<wchar_t*>(&message),
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

    // $ cp -r
    static void copyDir(const QString &src, const QString &dst) {
        QDir dir(src);
        if (!dir.exists()) {
            return;
        }
        for (QString d : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QString dst_path = dst + '/' + d;
            dir.mkpath(dst_path);
            copyDir(src + '/' + d, dst_path);
        }
        for (QString f : dir.entryList(QDir::Files)) {
            QFile::copy(src + '/' + f, dst + '/' + f);
        }
    }

    static QString getCurrentDomain() {
        QProcess process;
        process.startCommand("whoami");
        process.waitForFinished();
        QString currentDomain = process.readLine().chopped(2);
        return currentDomain.first(currentDomain.indexOf('\\') + 1);
    }


    QString promptForCredentials(const QString &autofillUsername, HWND parentHWND) {
        unsigned long result;
        unsigned long authPackage = 0;
        CREDUI_INFOW uiInfo = {
            .cbSize = sizeof(uiInfo),
            .hwndParent = parentHWND,
            .pszMessageText = L"Please enter credentials to use for Computer Status Neo.",
            .pszCaptionText = L"Switch user",
            .hbmBanner = nullptr
        };

        result = CredPackAuthenticationBufferW(
            CRED_PACK_PROTECTED_CREDENTIALS,
            autofillUsername.toStdWString().data(),
            const_cast<wchar_t *>(L""),
            nullptr,
            &inAuthBufSize
        );
        if (!result && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            return errorMessage("CredPackAuthenticationBufferW Size Check");
        }

        inAuthBuf = malloc(inAuthBufSize);
        if (inAuthBuf == nullptr) {
            return errorMessage("malloc");
        }

        result = CredPackAuthenticationBufferW(
            CRED_PACK_PROTECTED_CREDENTIALS,
            autofillUsername.toStdWString().data(),
            const_cast<wchar_t *>(L""),
            static_cast<unsigned char *>(inAuthBuf),
            &inAuthBufSize
        );
        if (!result) {
            return errorMessage("CredPackAuthenticationBufferW");
        }

        result = CredUIPromptForWindowsCredentialsW(
            &uiInfo,
            0,
            &authPackage,
            inAuthBuf,
            inAuthBufSize,
            &outAuthBuf,
            &outAuthBufSize,
            nullptr,
            CREDUIWIN_AUTHPACKAGE_ONLY
        );
        switch (result) {
            case ERROR_SUCCESS:
                break;
            case ERROR_CANCELLED:
                return "Cancelled";
            default:
                return errorMessage("CredUIPromptForWindowsCredentialsW");
        }

        result = CredUnPackAuthenticationBufferW(
            0x1,
            outAuthBuf,
            outAuthBufSize,
            nullptr,
            &usernameSize,
            nullptr,
            &domainSize,
            nullptr,
            &passwordSize
        );
        if (!result && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            return errorMessage("CredUnPackAuthenticationBufferW Size Check");
        }

        username = new wchar_t[usernameSize];
        password = new wchar_t[passwordSize];

        result = CredUnPackAuthenticationBufferW(
            0x1,
            outAuthBuf,
            outAuthBufSize,
            username,
            &usernameSize,
            nullptr,
            &domainSize,
            password,
            &passwordSize
        );
        if (!result) {
            return errorMessage("CredUnPackAuthenticationBufferW");
        }

        return "Success";
    }


    QString runAs(const QString command) {
        QString realUsername = QString::fromStdWString(username);
        QStringList parts = realUsername.split('\\');
        QString domain = parts[0];
        realUsername = parts[1];

        unsigned long result = CreateProcessWithLogonW(
            realUsername.toStdWString().data(),
            domain.toStdWString().data(),
            password,
            0,
            nullptr,
            command.toStdWString().data(),
            CREATE_UNICODE_ENVIRONMENT,
            nullptr,
            nullptr,
            &si,
            &pi
        );
        if (!result && GetLastError() != ERROR_SUCCESS) {
            return errorMessage("CreateProcessWithLogonW");
        }
        return "Success";
    }


  public:
    explicit UserSwitcher() {
        ZeroMemory(&si, sizeof(si));
        ZeroMemory(&pi, sizeof(pi));
        si.cb = sizeof(si);
    }


    virtual ~UserSwitcher() {
        if (outAuthBuf != nullptr) {
            SecureZeroMemory(outAuthBuf, outAuthBufSize);
            CoTaskMemFree(outAuthBuf);
        }
        if (inAuthBuf != nullptr) {
            free(inAuthBuf);
        }
        if (username != nullptr) {
            delete[] username;
        }
        if (password != nullptr) {
            SecureZeroMemory(password, sizeof(password));
            delete[] password;
        }
        if (pi.hProcess   != nullptr) CloseHandle(pi.hProcess);
        if (pi.hThread    != nullptr) CloseHandle(pi.hThread);
        if (si.hStdInput  != nullptr) CloseHandle(si.hStdInput);
        if (si.hStdOutput != nullptr) CloseHandle(si.hStdOutput);
        if (si.hStdError  != nullptr) CloseHandle(si.hStdError);
    }


    std::tuple<QString, QString> switchUser(QString autofillUsername) {
        // Get Computer Status's window handle to place the credential prompt
        // over the window (default is to put it in the middle of the screen)
        HWND hwnd = nullptr;
        for (const auto widget : qApp->allWidgets()) {
            if (widget->objectName() == "MainWindow") {
                hwnd = reinterpret_cast<HWND>(widget->effectiveWinId());
                break;
            }
        }

        autofillUsername = autofillUsername.size() > 0 ? autofillUsername : getCurrentDomain();
        QString credResult = promptForCredentials(autofillUsername, hwnd);
        QString usernameToSave = username == nullptr ? "" : QString::fromStdWString(username);
        if (credResult != "Success") {
            return { credResult, usernameToSave };
        }

        // Copy Computer Status Neo to your temp folder and grant the new user execute access.
        // Usually, Computer Status Tool will be on a network drive and the new user isn't
        // going to be able to execute it from there.
        QDir dest(QDir::tempPath() + "\\Computer Status Neo");
        if (!dest.exists()) {
            dest.mkpath(".");
        }
        copyDir(qApp->applicationDirPath(), dest.path());
        QProcess process;
        process.startCommand(
            "icacls \"" + QDir::toNativeSeparators(dest.path()) +
            "\" /grant " + usernameToSave +
            ":(OI)(CI)RX /T"
        );
        process.waitForFinished();

        // The Computer Status Neo.exe within the temporary directory
        QString command = dest.filePath(qApp->applicationFilePath().split('/').last());
        return { runAs(command), usernameToSave };
    }
};
