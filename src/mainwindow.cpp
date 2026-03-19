#include "mainwindow.h"
#include <QMessageBox>
#include <QDateTime>
#include <QScrollBar>
#include <QTextEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QTableWidget>
#include <QCheckBox>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>

// CRC-16 计算（Modbus RTU）
QByteArray MainWindow::calculateCRC(const QByteArray& data)
{
    quint16 crc = 0xFFFF;
    for (char c : data) {
        crc ^= (quint16)(unsigned char)c;
        for (int i = 0; i < 8; i++) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    QByteArray crcBytes;
    crcBytes.append((char)(crc & 0xFF));
    crcBytes.append((char)((crc >> 8) & 0xFF));
    return crcBytes;
}

bool MainWindow::verifyCRC(const QByteArray& data)
{
    if (data.size() < 2) return false;
    QByteArray payload = data.left(data.size() - 2);
    quint16 received = ((quint16)(unsigned char)data[data.size()-1] << 8) |
                        (quint16)(unsigned char)data[data.size()-2];
    
    quint16 crc = 0xFFFF;
    for (char c : payload) {
        crc ^= (quint16)(unsigned char)c;
        for (int i = 0; i < 8; i++) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc == received;
}

QString MainWindow::toHex(const QByteArray& data)
{
    QStringList hexList;
    for (unsigned char c : data) {
        hexList << QString("%1").arg(c, 2, 16, QChar('0')).toUpper();
    }
    return hexList.join(" ");
}

QByteArray MainWindow::fromHex(const QString& hex)
{
    QByteArray result;
    QString cleanHex = hex.trimmed().remove(QRegularExpression("\\s+"));
    for (int i = 0; i + 1 < cleanHex.size(); i += 2) {
        bool ok;
        quint8 byte = cleanHex.mid(i, 2).toUInt(&ok, 16);
        if (ok) result.append((char)byte);
    }
    return result;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), isConnected(false), nextTaskId(1), lastByteTime(0)
{
    sendStats = {0, 0};
    receiveStats = {0, 0};
    combinedStats = {0, 0};
    
    setupUI();
    
    serial = new QSerialPort(this);
    connect(serial, &QSerialPort::readyRead, this, &MainWindow::onSerialDataReceived);
    
    frameTimer = new QTimer(this);
    frameTimer->setSingleShot(true);
    connect(frameTimer, &QTimer::timeout, this, [this]() {
        if (!receiveBuffer.isEmpty()) {
            appendReceiveLog(receiveBuffer);
            receiveBuffer.clear();
        }
    });
}

MainWindow::~MainWindow()
{
    if (serial->isOpen()) {
        serial->close();
    }
    
    for (auto& task : sendTasks) {
        if (task.timer) {
            task.timer->stop();
            delete task.timer;
        }
    }
}

void MainWindow::setupUI()
{
    setWindowTitle("Qt Modbus 调试工具 v1.1 - RS485 串口版 🦞");
    resize(1000, 800);
    
    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout* mainLayout = new QVBoxLayout(central);
    
    // 串口配置
    QGroupBox* connGroup = new QGroupBox("🔌 串口配置");
    QHBoxLayout* connLayout = new QHBoxLayout();
    
    connLayout->addWidget(new QLabel("端口:"));
    QComboBox* portCombo = new QComboBox();
    portCombo->setMinimumWidth(100);
    connLayout->addWidget(portCombo);
    
    connLayout->addWidget(new QLabel("波特率:"));
    QComboBox* baudCombo = new QComboBox();
    baudCombo->addItems({"1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200"});
    baudCombo->setCurrentText("9600");
    connLayout->addWidget(baudCombo);
    
    connLayout->addWidget(new QLabel("数据位:"));
    QComboBox* dataBitsCombo = new QComboBox();
    dataBitsCombo->addItems({"8", "7", "6", "5"});
    dataBitsCombo->setCurrentText("8");
    connLayout->addWidget(dataBitsCombo);
    
    connLayout->addWidget(new QLabel("停止位:"));
    QComboBox* stopBitsCombo = new QComboBox();
    stopBitsCombo->addItems({"1", "1.5", "2"});
    stopBitsCombo->setCurrentText("1");
    connLayout->addWidget(stopBitsCombo);
    
    connLayout->addWidget(new QLabel("校验:"));
    QComboBox* parityCombo = new QComboBox();
    parityCombo->addItems({"无", "奇", "偶"});
    parityCombo->setCurrentText("无");
    connLayout->addWidget(parityCombo);
    
    connLayout->addWidget(new QLabel("帧超时:"));
    QSpinBox* timeoutSpin = new QSpinBox();
    timeoutSpin->setRange(10, 1000);
    timeoutSpin->setValue(50);
    timeoutSpin->setSuffix(" ms");
    connLayout->addWidget(timeoutSpin);
    
    QPushButton* connectBtn = new QPushButton("🔌 打开串口");
    connectBtn->setMaximumWidth(100);
    connLayout->addWidget(connectBtn);
    
    QLabel* statusLabel = new QLabel("状态：○ 未连接");
    statusLabel->setStyleSheet("color: gray;");
    connLayout->addWidget(statusLabel);
    
    connLayout->addStretch();
    connGroup->setLayout(connLayout);
    mainLayout->addWidget(connGroup);
    
    // 发送区域
    QGroupBox* sendGroup = new QGroupBox("📤 发送");
    QHBoxLayout* sendLayout = new QHBoxLayout();
    
    sendLayout->addWidget(new QLabel("数据:"));
    QLineEdit* dataEdit = new QLineEdit();
    dataEdit->setPlaceholderText("01 03 00 00 00 0A");
    dataEdit->setMinimumWidth(300);
    sendLayout->addWidget(dataEdit);
    
    QCheckBox* autoCrcCheck = new QCheckBox("☑ 自动 CRC");
    autoCrcCheck->setChecked(true);
    sendLayout->addWidget(autoCrcCheck);
    
    QPushButton* sendBtn = new QPushButton("📤 发送");
    sendLayout->addWidget(sendBtn);
    
    sendLayout->addStretch();
    sendGroup->setLayout(sendLayout);
    mainLayout->addWidget(sendGroup);
    
    // 循环发送
    QGroupBox* taskGroup = new QGroupBox("🔄 循环发送");
    QVBoxLayout* taskLayout = new QVBoxLayout();
    
    QTableWidget* taskTable = new QTableWidget();
    taskTable->setColumnCount(5);
    taskTable->setHorizontalHeaderLabels({"启用", "发送数据", "间隔 (ms)", "次数", "操作"});
    taskTable->setMaximumHeight(150);
    taskLayout->addWidget(taskTable);
    
    QHBoxLayout* taskBtnLayout = new QHBoxLayout();
    QPushButton* addTaskBtn = new QPushButton("+ 添加任务");
    taskBtnLayout->addWidget(addTaskBtn);
    
    QPushButton* startTasksBtn = new QPushButton("▶ 启动");
    taskBtnLayout->addWidget(startTasksBtn);
    
    QPushButton* stopTasksBtn = new QPushButton("⏹ 停止");
    stopTasksBtn->setEnabled(false);
    taskBtnLayout->addWidget(stopTasksBtn);
    
    taskBtnLayout->addStretch();
    taskLayout->addLayout(taskBtnLayout);
    taskGroup->setLayout(taskLayout);
    mainLayout->addWidget(taskGroup);
    
    // 日志区域 - 三个独立的日志框
    QGroupBox* logGroup = new QGroupBox("📝 日志");
    QVBoxLayout* logLayout = new QVBoxLayout();
    
    // 发送日志
    QHBoxLayout* sendLogLayout = new QHBoxLayout();
    sendLogLayout->addWidget(new QLabel("📤 发送日志"));
    sendStatsLabel = new QLabel("(0 次，0B)");
    sendStatsLabel->setStyleSheet("color: blue; font-weight: bold;");
    sendLogLayout->addWidget(sendStatsLabel);
    sendLogLayout->addStretch();
    QPushButton* exportSendBtn = new QPushButton("💾 导出");
    exportSendBtn->setMaximumWidth(80);
    sendLogLayout->addWidget(exportSendBtn);
    QPushButton* clearSendBtn = new QPushButton("🗑 清空");
    clearSendBtn->setMaximumWidth(80);
    sendLogLayout->addWidget(clearSendBtn);
    logLayout->addLayout(sendLogLayout);
    
    sendLogEdit = new QTextEdit();
    sendLogEdit->setReadOnly(true);
    sendLogEdit->setMaximumHeight(120);
    sendLogEdit->setFont(QFont("Consolas", 9));
    logLayout->addWidget(sendLogEdit);
    
    // 接收日志
    QHBoxLayout* receiveLogLayout = new QHBoxLayout();
    receiveLogLayout->addWidget(new QLabel("📥 接收日志"));
    receiveStatsLabel = new QLabel("(0 次，0B)");
    receiveStatsLabel->setStyleSheet("color: green; font-weight: bold;");
    receiveLogLayout->addWidget(receiveStatsLabel);
    receiveLogLayout->addStretch();
    QPushButton* exportReceiveBtn = new QPushButton("💾 导出");
    exportReceiveBtn->setMaximumWidth(80);
    receiveLogLayout->addWidget(exportReceiveBtn);
    QPushButton* clearReceiveBtn = new QPushButton("🗑 清空");
    clearReceiveBtn->setMaximumWidth(80);
    receiveLogLayout->addWidget(clearReceiveBtn);
    logLayout->addLayout(receiveLogLayout);
    
    receiveLogEdit = new QTextEdit();
    receiveLogEdit->setReadOnly(true);
    receiveLogEdit->setMaximumHeight(120);
    receiveLogEdit->setFont(QFont("Consolas", 9));
    logLayout->addWidget(receiveLogEdit);
    
    // 合并日志
    QHBoxLayout* combinedLogLayout = new QHBoxLayout();
    combinedLogLayout->addWidget(new QLabel("📊 合并日志"));
    combinedStatsLabel = new QLabel("(0 次，0B)");
    combinedStatsLabel->setStyleSheet("color: purple; font-weight: bold;");
    combinedLogLayout->addWidget(combinedStatsLabel);
    combinedLogLayout->addStretch();
    QPushButton* exportCombinedBtn = new QPushButton("💾 导出");
    exportCombinedBtn->setMaximumWidth(80);
    combinedLogLayout->addWidget(exportCombinedBtn);
    QPushButton* clearCombinedBtn = new QPushButton("🗑 清空");
    clearCombinedBtn->setMaximumWidth(80);
    combinedLogLayout->addWidget(clearCombinedBtn);
    logLayout->addLayout(combinedLogLayout);
    
    combinedLogEdit = new QTextEdit();
    combinedLogEdit->setReadOnly(true);
    combinedLogEdit->setMinimumHeight(150);
    combinedLogEdit->setFont(QFont("Consolas", 9));
    logLayout->addWidget(combinedLogEdit);
    
    logGroup->setLayout(logLayout);
    mainLayout->addWidget(logGroup);
    
    // 连接信号
    connect(connectBtn, &QPushButton::clicked, this, &MainWindow::onConnectButtonClicked);
    connect(sendBtn, &QPushButton::clicked, this, &MainWindow::onSendButtonClicked);
    connect(clearSendBtn, &QPushButton::clicked, this, [this]() { sendLogEdit->clear(); sendStats = {0, 0}; updateLogStats(); });
    connect(clearReceiveBtn, &QPushButton::clicked, this, [this]() { receiveLogEdit->clear(); receiveStats = {0, 0}; updateLogStats(); });
    connect(clearCombinedBtn, &QPushButton::clicked, this, [this]() { combinedLogEdit->clear(); combinedStats = {0, 0}; updateLogStats(); });
    connect(exportSendBtn, &QPushButton::clicked, this, &MainWindow::onExportSendLog);
    connect(exportReceiveBtn, &QPushButton::clicked, this, &MainWindow::onExportReceiveLog);
    connect(exportCombinedBtn, &QPushButton::clicked, this, &MainWindow::onExportCombinedLog);
    connect(addTaskBtn, &QPushButton::clicked, this, &MainWindow::onAddTaskButtonClicked);
    connect(startTasksBtn, &QPushButton::clicked, this, &MainWindow::onStartTasksButtonClicked);
    connect(stopTasksBtn, &QPushButton::clicked, this, &MainWindow::onStopTasksButtonClicked);
    
    // 刷新端口列表
    refreshPortList();
}

void MainWindow::refreshPortList()
{
    QComboBox* portCombo = findChild<QComboBox*>();
    if (portCombo) {
        portCombo->clear();
        QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
        for (const QSerialPortInfo& port : ports) {
            portCombo->addItem(port.portName());
        }
        if (ports.isEmpty()) {
            portCombo->addItem("无可用串口");
        }
    }
}

void MainWindow::onConnectButtonClicked()
{
    QComboBox* portCombo = findChild<QComboBox*>();
    QComboBox* baudCombo = findChild<QComboBox*>();
    QComboBox* dataBitsCombo = findChild<QComboBox*>();
    QComboBox* stopBitsCombo = findChild<QComboBox*>();
    QComboBox* parityCombo = findChild<QComboBox*>();
    QSpinBox* timeoutSpin = findChild<QSpinBox*>();
    QPushButton* connectBtn = findChild<QPushButton*>();
    QLabel* statusLabel = findChild<QLabel*>();
    
    if (!portCombo || !baudCombo || !connectBtn || !statusLabel) return;
    
    if (!isConnected) {
        QString portName = portCombo->currentText();
        if (portName == "无可用串口") {
            QMessageBox::warning(this, "警告", "未检测到可用串口");
            return;
        }
        
        serial->setPortName(portName);
        serial->setBaudRate(baudCombo->currentText().toInt());
        
        // 数据位
        int dataBits = dataBitsCombo->currentText().toInt();
        QSerialPort::DataBits db;
        switch (dataBits) {
            case 5: db = QSerialPort::Data5; break;
            case 6: db = QSerialPort::Data6; break;
            case 7: db = QSerialPort::Data7; break;
            default: db = QSerialPort::Data8;
        }
        serial->setDataBits(db);
        
        // 停止位
        QString stopBits = stopBitsCombo->currentText();
        QSerialPort::StopBits sb;
        if (stopBits == "1.5") sb = QSerialPort::OneAndHalfStop;
        else if (stopBits == "2") sb = QSerialPort::TwoStop;
        else sb = QSerialPort::OneStop;
        serial->setStopBits(sb);
        
        // 校验位
        QString parity = parityCombo->currentText();
        QSerialPort::Parity p;
        if (parity == "奇") p = QSerialPort::OddParity;
        else if (parity == "偶") p = QSerialPort::EvenParity;
        else p = QSerialPort::NoParity;
        serial->setParity(p);
        
        if (serial->open(QIODevice::ReadWrite)) {
            isConnected = true;
            connectBtn->setText("❌ 关闭串口");
            statusLabel->setText("状态：● 已连接");
            statusLabel->setStyleSheet("color: green;");
            
            if (timeoutSpin) {
                frameTimer->setInterval(timeoutSpin->value());
            }
        } else {
            QMessageBox::critical(this, "错误", "无法打开串口：" + serial->errorString());
        }
    } else {
        serial->close();
        isConnected = false;
        connectBtn->setText("🔌 打开串口");
        statusLabel->setText("状态：○ 未连接");
        statusLabel->setStyleSheet("color: gray;");
    }
}

void MainWindow::onSendButtonClicked()
{
    if (!isConnected) {
        QMessageBox::warning(this, "警告", "请先打开串口");
        return;
    }
    
    QLineEdit* dataEdit = findChild<QLineEdit*>();
    QCheckBox* autoCrcCheck = findChild<QCheckBox*>();
    
    if (!dataEdit || !autoCrcCheck) return;
    
    QString hexStr = dataEdit->text().trimmed();
    if (hexStr.isEmpty()) {
        QMessageBox::warning(this, "警告", "请输入发送数据");
        return;
    }
    
    QByteArray data = fromHex(hexStr);
    if (data.isEmpty()) {
        QMessageBox::warning(this, "警告", "数据格式错误");
        return;
    }
    
    if (autoCrcCheck->isChecked()) {
        data.append(calculateCRC(data));
    }
    
    serial->write(data);
    serial->flush();
    
    appendSendLog(data);
    appendCombinedLog(data, true);
}

void MainWindow::onSerialDataReceived()
{
    while (serial->canReadLine()) {
        QByteArray data = serial->readLine();
        receiveBuffer.append(data);
        lastByteTime = QDateTime::currentMSecsSinceEpoch();
        frameTimer->start();
    }
}

void MainWindow::appendSendLog(const QByteArray& data)
{
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    QString hexStr = toHex(data);
    sendLogEdit->append(QString("[%1] >> %2").arg(timestamp).arg(hexStr));
    sendLogEdit->verticalScrollBar()->setValue(sendLogEdit->verticalScrollBar()->maximum());
    
    sendStats.count++;
    sendStats.bytes += data.size();
    updateLogStats();
}

void MainWindow::appendReceiveLog(const QByteArray& data)
{
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    QString hexStr = toHex(data);
    bool crcOk = verifyCRC(data);
    QString crcMark = crcOk ? " ✓" : " ✗";
    receiveLogEdit->append(QString("[%1] << %2%3").arg(timestamp).arg(hexStr).arg(crcMark));
    receiveLogEdit->verticalScrollBar()->setValue(receiveLogEdit->verticalScrollBar()->maximum());
    
    receiveStats.count++;
    receiveStats.bytes += data.size();
    updateLogStats();
}

void MainWindow::appendCombinedLog(const QByteArray& data, bool isSend)
{
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    QString hexStr = toHex(data);
    QString line;
    
    if (isSend) {
        line = QString("[%1] >> %2").arg(timestamp).arg(hexStr);
        combinedStats.bytes += data.size();
    } else {
        bool crcOk = verifyCRC(data);
        QString crcMark = crcOk ? " ✓" : " ✗";
        line = QString("[%1] << %2%3").arg(timestamp).arg(hexStr).arg(crcMark);
        combinedStats.bytes += data.size();
    }
    
    combinedLogEdit->append(line);
    combinedLogEdit->verticalScrollBar()->setValue(combinedLogEdit->verticalScrollBar()->maximum());
    
    combinedStats.count++;
    updateLogStats();
}

void MainWindow::updateLogStats()
{
    sendStatsLabel->setText(QString("(%1 次，%2B)").arg(sendStats.count).arg(sendStats.bytes));
    receiveStatsLabel->setText(QString("(%1 次，%2B)").arg(receiveStats.count).arg(receiveStats.bytes));
    combinedStatsLabel->setText(QString("(%1 次，%2B)").arg(combinedStats.count).arg(combinedStats.bytes));
}

void MainWindow::onExportSendLog()
{
    if (sendStats.count == 0) {
        QMessageBox::information(this, "提示", "暂无发送日志可导出");
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(this, "导出发送日志", 
        QString("SendLog_%1.txt").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")),
        "文本文件 (*.txt)");
    
    if (fileName.isEmpty()) return;
    
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << sendLogEdit->toPlainText();
        file.close();
        QMessageBox::information(this, "成功", QString("发送日志已导出到:\n%1").arg(fileName));
    } else {
        QMessageBox::critical(this, "错误", QString("无法写入文件:\n%1").arg(fileName));
    }
}

void MainWindow::onExportReceiveLog()
{
    if (receiveStats.count == 0) {
        QMessageBox::information(this, "提示", "暂无接收日志可导出");
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(this, "导出接收日志", 
        QString("ReceiveLog_%1.txt").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")),
        "文本文件 (*.txt)");
    
    if (fileName.isEmpty()) return;
    
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << receiveLogEdit->toPlainText();
        file.close();
        QMessageBox::information(this, "成功", QString("接收日志已导出到:\n%1").arg(fileName));
    } else {
        QMessageBox::critical(this, "错误", QString("无法写入文件:\n%1").arg(fileName));
    }
}

void MainWindow::onExportCombinedLog()
{
    if (combinedStats.count == 0) {
        QMessageBox::information(this, "提示", "暂无合并日志可导出");
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(this, "导出合并日志", 
        QString("CombinedLog_%1.txt").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")),
        "文本文件 (*.txt)");
    
    if (fileName.isEmpty()) return;
    
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << combinedLogEdit->toPlainText();
        file.close();
        QMessageBox::information(this, "成功", QString("合并日志已导出到:\n%1").arg(fileName));
    } else {
        QMessageBox::critical(this, "错误", QString("无法写入文件:\n%1").arg(fileName));
    }
}

void MainWindow::onClearLogButtonClicked()
{
    sendLogEdit->clear();
    receiveLogEdit->clear();
    combinedLogEdit->clear();
    sendStats = {0, 0};
    receiveStats = {0, 0};
    combinedStats = {0, 0};
    updateLogStats();
}

void MainWindow::onAddTaskButtonClicked()
{
    QLineEdit* dataEdit = findChild<QLineEdit*>();
    QTableWidget* taskTable = findChild<QTableWidget*>();
    
    if (!dataEdit || !taskTable) return;
    
    QString hexStr = dataEdit->text().trimmed();
    if (hexStr.isEmpty()) {
        QMessageBox::warning(this, "警告", "请先输入发送数据");
        return;
    }
    
    int row = taskTable->rowCount();
    taskTable->insertRow(row);
    
    QCheckBox* check = new QCheckBox();
    taskTable->setCellWidget(row, 0, check);
    
    QTableWidgetItem* dataItem = new QTableWidgetItem(hexStr);
    taskTable->setItem(row, 1, dataItem);
    
    QSpinBox* intervalSpin = new QSpinBox();
    intervalSpin->setRange(100, 60000);
    intervalSpin->setValue(1000);
    intervalSpin->setSuffix(" ms");
    taskTable->setCellWidget(row, 2, intervalSpin);
    
    QSpinBox* countSpin = new QSpinBox();
    countSpin->setRange(-1, 9999);
    countSpin->setValue(-1);
    countSpin->setSpecialValueText("∞");
    taskTable->setCellWidget(row, 3, countSpin);
    
    QPushButton* delBtn = new QPushButton("🗑 删除");
    connect(delBtn, &QPushButton::clicked, this, [this, row]() {
        QTableWidget* table = findChild<QTableWidget*>();
        if (table) table->removeRow(row);
    });
    taskTable->setCellWidget(row, 4, delBtn);
}

void MainWindow::onStartTasksButtonClicked()
{
    QTableWidget* taskTable = findChild<QTableWidget*>();
    QPushButton* startBtn = findChild<QPushButton*>();
    QPushButton* stopBtn = findChild<QPushButton*>();
    
    if (!taskTable || !startBtn || !stopBtn) return;
    
    for (auto& task : sendTasks) {
        if (task.timer) {
            task.timer->stop();
            delete task.timer;
        }
    }
    sendTasks.clear();
    
    for (int row = 0; row < taskTable->rowCount(); row++) {
        QCheckBox* check = qobject_cast<QCheckBox*>(taskTable->cellWidget(row, 0));
        QTableWidgetItem* dataItem = taskTable->item(row, 1);
        QSpinBox* intervalSpin = qobject_cast<QSpinBox*>(taskTable->cellWidget(row, 2));
        QSpinBox* countSpin = qobject_cast<QSpinBox*>(taskTable->cellWidget(row, 3));
        
        if (!check || !dataItem || !intervalSpin || !countSpin) continue;
        
        SendTask task;
        task.id = nextTaskId++;
        task.enabled = check->isChecked();
        task.data = fromHex(dataItem->text());
        task.intervalMs = intervalSpin->value();
        task.maxCount = countSpin->value();
        task.currentCount = 0;
        
        if (task.enabled) {
            task.timer = new QTimer(this);
            connect(task.timer, &QTimer::timeout, this, [this, taskId=task.id]() {
                onTaskSend(taskId);
            });
            task.timer->start(task.intervalMs);
        } else {
            task.timer = nullptr;
        }
        
        sendTasks.append(task);
    }
    
    startBtn->setEnabled(false);
    stopBtn->setEnabled(true);
}

void MainWindow::onStopTasksButtonClicked()
{
    QPushButton* startBtn = findChild<QPushButton*>();
    QPushButton* stopBtn = findChild<QPushButton*>();
    
    for (auto& task : sendTasks) {
        if (task.timer) {
            task.timer->stop();
            delete task.timer;
        }
        task.currentCount = 0;
    }
    sendTasks.clear();
    
    if (startBtn) startBtn->setEnabled(true);
    if (stopBtn) stopBtn->setEnabled(false);
}

void MainWindow::onTaskSend(int taskId)
{
    for (auto& task : sendTasks) {
        if (task.id == taskId) {
            if (task.maxCount >= 0 && task.currentCount >= task.maxCount) {
                task.timer->stop();
                return;
            }
            
            if (!isConnected) return;
            
            QByteArray data = task.data;
            QCheckBox* autoCrcCheck = findChild<QCheckBox*>();
            if (autoCrcCheck && autoCrcCheck->isChecked()) {
                data.append(calculateCRC(data));
            }
            
            serial->write(data);
            serial->flush();
            
            appendSendLog(data);
            appendCombinedLog(data, true);
            
            task.currentCount++;
            return;
        }
    }
}
