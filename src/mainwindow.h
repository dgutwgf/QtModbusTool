#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QModbusTcpClient>
#include <QModbusDataUnit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QTableWidget>
#include <QLabel>
#include <QGroupBox>
#include <QTextEdit>
#include <QStatusBar>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onConnectButtonClicked();
    void onReadButtonClicked();
    void onWriteButtonClicked();
    void onScanButtonClicked();
    void updateConnectionStatus(QModbusDevice::State state);
    void appendLog(const QString &message);

private:
    void setupUI();
    void setupModbus();
    
    // UI 组件
    QWidget *centralWidget;
    QVBoxLayout *mainLayout;
    
    // 连接设置
    QGroupBox *connectionGroup;
    QLineEdit *serverAddressEdit;
    QSpinBox *portSpinBox;
    QLineEdit *slaveIdSpinBox;
    QPushButton *connectButton;
    QLabel *statusLabel;
    
    // 读写操作
    QGroupBox *operationGroup;
    QComboBox *functionCombo;
    QSpinBox *startAddressSpinBox;
    QSpinBox *quantitySpinBox;
    QLineEdit *valueEdit;
    QPushButton *readButton;
    QPushButton *writeButton;
    QPushButton *scanButton;
    
    // 数据显示
    QGroupBox *dataGroup;
    QTableWidget *dataTable;
    
    // 日志
    QGroupBox *logGroup;
    QTextEdit *logEdit;
    
    // Modbus 客户端
    QModbusTcpClient *modbusClient;
    bool isConnected;
};

#endif // MAINWINDOW_H
