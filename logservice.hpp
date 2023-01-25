#ifndef LOGSERVICE_HPP
#define LOGSERVICE_HPP

#include <QObject>
#include <QThread>


// the worker that will be moved to a thread
class LogWorker: public QObject {
    Q_OBJECT

  public:
    explicit LogWorker(QObject *parent = nullptr);

  public slots:
    // this slot will be executed by event loop (one call at a time)
    void logEvent(const QString &event);
};


class LogService : public QObject {
    Q_OBJECT

  private:
    QThread *thread;
    LogWorker *worker;

  public:
    explicit LogService(QObject *parent = nullptr) : QObject(parent) {
        thread = new QThread(this);
        worker = new LogWorker;
        worker->moveToThread(thread);
        connect(this, &LogService::logEvent, worker, &LogWorker::logEvent);
        connect(thread, &QThread::finished, worker, &QObject::deleteLater);
        thread->start();
    }
    ~LogService() {
        thread->quit();
        thread->wait();
    }

  signals:
    // to use the service, just call this signal to send a request:
    // logService->logEvent("event");
    void logEvent(const QString &event);
};


#endif  // LOGSERVICE_HPP
