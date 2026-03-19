#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QModbusRtuSerialMaster>
#include <QModbusDataUnit>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include <QByteArray>
#include <QVector>
#include <QString>
#include <QList>

#include "logpanel.h"
#include "cyclicsender.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

/**
 * @brief 循环发送任务配置
 */
struct SendTaskConfig
{
    int id;
    bool enabled;
    QString data;        // 十六进制字符串
    int intervalMs;      // 间隔时间
    int maxCount;        // 最大次数 (-1 无限)
    int currentCount;    // 当前次数
    QTimer *timer;       // 定时器
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // 串口操作
    void onConnectButtonClicked();
    void onPortChanged();
    
    // 手动发送
    void onReadButtonClicked();
    void onWriteButtonClicked();
    void onSendButtonClicked();
    void onScanButtonClicked();
    
    // 循环发送
    void onAddTaskClicked();
    void onStartCycleClicked();
    void onPauseCycleClicked();
    void onStopCycleClicked();
    void onTaskSend(int taskId);
    
    // 状态更新
    void updateConnectionStatus(QModbusDevice::State state);
    void onErrorOccurred(QModbusDevice::Error error);
    void onDataReceived(const QByteArray &data);

private:
    void setupUI();
    void setupModbus();
    void refreshPortList();
    
    // CRC 处理
    QByteArray appendCRC(const QByteArray &data);
    bool verifyCRC(const QByteArray &data);
    
    // 帧处理
    void processReceivedData(const QByteArray &data);
    void resetFrameTimeout();
    void onFrameTimeout();
    
    // 日志
    void appendSendLog(const QByteArray &data);
    void appendReceiveLog(const QByteArray &data, bool crcOk);
    
    // UI 组件
    QWidget *centralWidget;
    QVBoxLayout *mainLayout;
    
    // 串口配置
    QComboBox *portCombo;
    QComboBox *baudCombo;
    QComboBox *dataBitsCombo;
    QComboBox *stopBitsCombo;
    QComboBox *parityCombo;
    QSpinBox *timeoutSpinBox;      // 帧超时时间
    QCheckBox *autoCRCCheck;       // 自动 CRC 复选框
    QPushButton *connectButton;
    QLabel *statusLabel;
    
    // 手动发送
    QComboBox *functionCombo;
    QSpinBox *slaveIdSpinBox;
    QSpinBox *startAddressSpinBox;
    QSpinBox *quantitySpinBox;
    QLineEdit *writeValueEdit;
    QLineEdit *customDataEdit;     // 自定义发送数据
    QPushButton *readButton;
    QPushButton *writeButton;
    QPushButton *sendButton;
    QPushButton *scanButton;
    
    // 循环发送
    QTableWidget *taskTable;
    QPushButton *addTaskButton;
    QPushButton *startCycleButton;
    QPushButton *pauseCycleButton;
    QPushButton *stopCycleButton;
    QLabel *cycleStatusLabel;
    
    // 日志面板
    LogPanel *sendLogPanel;
    LogPanel *receiveLogPanel;
    LogPanel *combinedLogPanel;
    
    // Modbus 客户端
    QModbusRtuSerialMaster *modbusClient;
    bool isConnected;
    
    // 帧超时处理
    QTimer *frameTimeoutTimer;
    QByteArray currentFrame;
    qint64 lastByteTime;
    
    // 循环发送任务
    QVector<SendTaskConfig> sendTasks;
    int nextTaskId;
};

#endif // MAINWINDOW_H
