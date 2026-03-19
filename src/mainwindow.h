#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QList>
#include <QDateTime>
#include <QRegularExpression>

#include "logpanel.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// 循环发送任务
struct SendTask {
    int id;
    bool enabled;
    QByteArray data;
    int intervalMs;
    int maxCount;
    int currentCount;
    QTimer* timer;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onConnectButtonClicked();
    void onSendButtonClicked();
    void onClearLogButtonClicked();
    void onSerialDataReceived();
    void onTaskSend(int taskId);
    void onAddTaskButtonClicked();
    void onStartTasksButtonClicked();
    void onStopTasksButtonClicked();

private:
    void setupUI();
    void refreshPortList();
    QByteArray calculateCRC(const QByteArray& data);
    bool verifyCRC(const QByteArray& data);
    QString toHex(const QByteArray& data);
    QByteArray fromHex(const QString& hex);
    void appendSendLog(const QByteArray& data);
    void appendReceiveLog(const QByteArray& data);
    
    Ui::MainWindow* ui;
    QSerialPort* serial;
    bool isConnected;
    QList<SendTask> sendTasks;
    int nextTaskId;
    qint64 lastByteTime;
    QByteArray receiveBuffer;
    QTimer* frameTimer;
};

#endif // MAINWINDOW_H
