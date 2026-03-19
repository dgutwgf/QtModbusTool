#include "mainwindow.h"
#include <QMessageBox>
#include <QDateTime>
#include <QHeaderView>
#include <QModbusReply>
#include <QSerialPortInfo>
#include <QScrollBar>
#include <QRegularExpression>

// CRC-16 计算（Modbus RTU 标准）
static quint16 calculateCRC16(const QByteArray &data)
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
    return crc;
}

// 附加 CRC
static QByteArray appendCRC16(const QByteArray &data)
{
    quint16 crc = calculateCRC16(data);
    QByteArray result = data;
    result.append((char)(crc & 0xFF));          // 低字节
    result.append((char)((crc >> 8) & 0xFF));   // 高字节
    return result;
}

// 验证 CRC
static bool verifyCRC16(const QByteArray &data)
{
    if (data.size() < 2) return false;
    QByteArray payload = data.left(data.size() - 2);
    quint16 received = ((quint16)(unsigned char)data[data.size()-1] << 8) |
                        (quint16)(unsigned char)data[data.size()-2];
    return calculateCRC16(payload) == received;
}

// 字节数组转十六进制字符串
static QString toHex(const QByteArray &data)
{
    QStringList hexList;
    for (unsigned char c : data) {
        hexList << QString("%1").arg(c, 2, 16, QChar('0')).toUpper();
    }
    return hexList.join(" ");
}

// 十六进制字符串转字节数组
static QByteArray fromHex(const QString &hex)
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
    : QMainWindow(parent), isConnected(false), nextTaskId(1)
{
    setupUI();
    setupModbus();
}

MainWindow::~MainWindow()
{
    if (modbusClient) {
        if (modbusClient->state() != QModbusDevice::UnconnectedState)
            modbusClient->disconnectDevice();
        modbusClient->deleteLater();
    }
    
    // 清理定时器
    for (auto &task : sendTasks) {
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
    
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    mainLayout = new QVBoxLayout(centralWidget);
    
    // === 串口配置区域 ===
    auto *connGroup = new QGroupBox("🔌 串口配置");
    auto *connLayout = new QHBoxLayout();
    
    connLayout->addWidget(new QLabel("端口:"));
    portCombo = new QComboBox();
    portCombo->setMinimumWidth(100);
    refreshPortList();
    connLayout->addWidget(portCombo);
    
    connLayout->addWidget(new QLabel("波特率:"));
    baudCombo = new QComboBox();
    baudCombo->addItems({"1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200"});
    baudCombo->setCurrentText("9600");
    baudCombo->setMinimumWidth(80);
    connLayout->addWidget(baudCombo);
    
    connLayout->addWidget(new QLabel("数据位:"));
    dataBitsCombo = new QComboBox();
    dataBitsCombo->addItems({"8", "7", "6", "5"});
    dataBitsCombo->setCurrentText("8");
    dataBitsCombo->setMinimumWidth(60);
    connLayout->addWidget(dataBitsCombo);
    
    connLayout->addWidget(new QLabel("停止位:"));
    stopBitsCombo = new QComboBox();
    stopBitsCombo->addItems({"1", "1.5", "2"});
    stopBitsCombo->setCurrentText("1");
    stopBitsCombo->setMinimumWidth(60);
    connLayout->addWidget(stopBitsCombo);
    
    connLayout->addWidget(new QLabel("校验:"));
    parityCombo = new QComboBox();
    parityCombo->addItems({"无", "奇", "偶"});
    parityCombo->setCurrentText("无");
    parityCombo->setMinimumWidth(60);
    connLayout->addWidget(parityCombo);
    
    connLayout->addWidget(new QLabel("帧超时:"));
    timeoutSpinBox = new QSpinBox();
    timeoutSpinBox->setRange(1, 1000);
    timeoutSpinBox->setValue(50);
    timeoutSpinBox->setSuffix(" ms");
    timeoutSpinBox->setMinimumWidth(80);
    connLayout->addWidget(timeoutSpinBox);
    
    autoCRCCheck = new QCheckBox("☑ 自动 CRC");
    autoCRCCheck->setChecked(true);
    connLayout->addWidget(autoCRCCheck);
    
    connectButton = new QPushButton("🔌 打开串口");
    connectButton->setMaximumWidth(100);
    connLayout->addWidget(connectButton);
    
    statusLabel = new QLabel("状态：○ 未连接");
    statusLabel->setStyleSheet("color: gray;");
    connLayout->addWidget(statusLabel);
    
    connLayout->addStretch();
    connGroup->setLayout(connLayout);
    mainLayout->addWidget(connGroup);
    
    // === 手动发送区域 ===
    auto *manualGroup = new QGroupBox("🔧 手动发送");
    auto *manualLayout = new QHBoxLayout();
    
    manualLayout->addWidget(new QLabel("功能码:"));
    functionCombo = new QComboBox();
    functionCombo->addItem("03 - 读保持寄存器", 3);
    functionCombo->addItem("04 - 读输入寄存器", 4);
    functionCombo->addItem("06 - 写单个寄存器", 6);
    functionCombo->addItem("16 - 写多个寄存器", 16);
    functionCombo->setMinimumWidth(180);
    manualLayout->addWidget(functionCombo);
    
    manualLayout->addWidget(new QLabel("从站 ID:"));
    slaveIdSpinBox = new QSpinBox();
    slaveIdSpinBox->setRange(1, 247);
    slaveIdSpinBox->setValue(1);
    slaveIdSpinBox->setMinimumWidth(60);
    manualLayout->addWidget(slaveIdSpinBox);
    
    manualLayout->addWidget(new QLabel("起始地址:"));
    startAddressSpinBox = new QSpinBox();
    startAddressSpinBox->setRange(0, 65535);
    startAddressSpinBox->setValue(0);
    startAddressSpinBox->setMinimumWidth(70);
    manualLayout->addWidget(startAddressSpinBox);
    
    manualLayout->addWidget(new QLabel("数量:"));
    quantitySpinBox = new QSpinBox();
    quantitySpinBox->setRange(1, 125);
    quantitySpinBox->setValue(10);
    quantitySpinBox->setMinimumWidth(60);
    manualLayout->addWidget(quantitySpinBox);
    
    manualLayout->addWidget(new QLabel("写入值:"));
    writeValueEdit = new QLineEdit("0");
    writeValueEdit->setPlaceholderText("逗号分隔");
    writeValueEdit->setMinimumWidth(150);
    manualLayout->addWidget(writeValueEdit);
    
    manualLayout->addWidget(new QLabel("自定义:"));
    customDataEdit = new QLineEdit();
    customDataEdit->setPlaceholderText("01 03 00 00 00 0A");
    customDataEdit->setMinimumWidth(200);
    manualLayout->addWidget(customDataEdit);
    
    readButton = new QPushButton("📖 读取");
    manualLayout->addWidget(readButton);
    
    writeButton = new QPushButton("✏️ 写入");
    manualLayout->addWidget(writeButton);
    
    sendButton = new QPushButton("📤 发送");
    manualLayout->addWidget(sendButton);
    
    scanButton = new QPushButton("🔍 扫描");
    manualLayout->addWidget(scanButton);
    
    manualLayout->addStretch();
    manualGroup->setLayout(manualLayout);
    mainLayout->addWidget(manualGroup);
    
    // === 循环发送区域 ===
    auto *cycleGroup = new QGroupBox("🔄 循环发送");
    auto *cycleLayout = new QVBoxLayout();
    
    taskTable = new QTableWidget();
    taskTable->setColumnCount(5);
    taskTable->setHorizontalHeaderLabels({"启用", "发送数据", "间隔 (ms)", "次数", "操作"});
    taskTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    taskTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    taskTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    taskTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    taskTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    taskTable->setMaximumHeight(150);
    cycleLayout->addWidget(taskTable);
    
    auto *btnLayout = new QHBoxLayout();
    addTaskButton = new QPushButton("+ 添加任务");
    btnLayout->addWidget(addTaskButton);
    
    startCycleButton = new QPushButton("▶ 启动循环");
    btnLayout->addWidget(startCycleButton);
    
    pauseCycleButton = new QPushButton("⏸ 暂停");
    pauseCycleButton->setEnabled(false);
    btnLayout->addWidget(pauseCycleButton);
    
    stopCycleButton = new QPushButton("⏹ 停止");
    stopCycleButton->setEnabled(false);
    btnLayout->addWidget(stopCycleButton);
    
    cycleStatusLabel = new QLabel("共 0 项，已发送 0 次");
    cycleStatusLabel->setStyleSheet("color: blue;");
    btnLayout->addWidget(cycleStatusLabel);
    
    btnLayout->addStretch();
    cycleLayout->addLayout(btnLayout);
    cycleGroup->setLayout(cycleLayout);
    mainLayout->addWidget(cycleGroup);
    
    // === 日志区域 ===
    sendLogPanel = new LogPanel("📤 发送日志");
    mainLayout->addWidget(sendLogPanel);
    
    receiveLogPanel = new LogPanel("📥 接收日志");
    mainLayout->addWidget(receiveLogPanel);
    
    combinedLogPanel = new LogPanel("📊 合并日志");
    mainLayout->addWidget(combinedLogPanel);
    
    // 状态栏
    statusBar()->showMessage("就绪");
    
    // 连接信号槽
    connect(connectButton, &QPushButton::clicked, this, &MainWindow::onConnectButtonClicked);
    connect(portCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onPortChanged);
    
    connect(readButton, &QPushButton::clicked, this, &MainWindow::onReadButtonClicked);
    connect(writeButton, &QPushButton::clicked, this, &MainWindow::onWriteButtonClicked);
    connect(sendButton, &QPushButton::clicked, this, &MainWindow::onSendButtonClicked);
    connect(scanButton, &QPushButton::clicked, this, &MainWindow::onScanButtonClicked);
    
    connect(addTaskButton, &QPushButton::clicked, this, &MainWindow::onAddTaskClicked);
    connect(startCycleButton, &QPushButton::clicked, this, &MainWindow::onStartCycleClicked);
    connect(pauseCycleButton, &QPushButton::clicked, this, &MainWindow::onPauseCycleClicked);
    connect(stopCycleButton, &QPushButton::clicked, this, &MainWindow::onStopCycleClicked);
    
    // 帧超时定时器
    frameTimeoutTimer = new QTimer(this);
    frameTimeoutTimer->setSingleShot(true);
    connect(frameTimeoutTimer, &QTimer::timeout, this, &MainWindow::onFrameTimeout);
}

void MainWindow::setupModbus()
{
    modbusClient = new QModbusRtuSerialMaster(this);
    
    connect(modbusClient, &QModbusRtuSerialMaster::stateChanged,
            this, &MainWindow::updateConnectionStatus);
    
    connect(modbusClient, &QModbusRtuSerialMaster::errorOccurred,
            this, &MainWindow::onErrorOccurred);
}

void MainWindow::refreshPortList()
{
    portCombo->clear();
    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &port : ports) {
        portCombo->addItem(port.portName());
    }
    if (ports.isEmpty()) {
        portCombo->addItem("无可用串口");
    }
}

void MainWindow::onPortChanged()
{
    // 端口变化时，如果是已连接状态，提示用户
    if (isConnected) {
        statusLabel->setText("状态：⚠ 端口已变更，请重连");
        statusLabel->setStyleSheet("color: orange;");
    }
}

void MainWindow::onConnectButtonClicked()
{
    if (!isConnected) {
        // 打开串口
        QString portName = portCombo->currentText();
        if (portName == "无可用串口") {
            QMessageBox::warning(this, "警告", "未检测到可用串口");
            return;
        }
        
        modbusClient->setConnectionParameter(QModbusSerialPort::SerialPortNameParameter, portName);
        modbusClient->setConnectionParameter(QModbusSerialPort::BaudRateParameter, baudCombo->currentText().toInt());
        
        // 数据位
        int dataBits = dataBitsCombo->currentText().toInt();
        QSerialPort::DataBits db;
        switch (dataBits) {
            case 5: db = QSerialPort::Data5; break;
            case 6: db = QSerialPort::Data6; break;
            case 7: db = QSerialPort::Data7; break;
            default: db = QSerialPort::Data8;
        }
        modbusClient->setConnectionParameter(QModbusSerialPort::DataBitsParameter, db);
        
        // 停止位
        QString stopBits = stopBitsCombo->currentText();
        QSerialPort::StopBits sb;
        if (stopBits == "1.5") sb = QSerialPort::OneAndHalfStop;
        else if (stopBits == "2") sb = QSerialPort::TwoStop;
        else sb = QSerialPort::OneStop;
        modbusClient->setConnectionParameter(QModbusSerialPort::StopBitsParameter, sb);
        
        // 校验位
        QString parity = parityCombo->currentText();
        QSerialPort::Parity p;
        if (parity == "奇") p = QSerialPort::OddParity;
        else if (parity == "偶") p = QSerialPort::EvenParity;
        else p = QSerialPort::NoParity;
        modbusClient->setConnectionParameter(QModbusSerialPort::ParityParameter, p);
        
        if (modbusClient->connectDevice()) {
            appendLog(QString("正在打开串口 %1...").arg(portName));
            connectButton->setText("❌ 关闭串口");
        } else {
            QMessageBox::critical(this, "错误", modbusClient->errorString());
            appendLog(QString("打开串口失败：%1").arg(modbusClient->errorString()));
        }
    } else {
        // 关闭串口
        if (modbusClient->state() != QModbusDevice::UnconnectedState)
            modbusClient->disconnectDevice();
        appendLog("已关闭串口");
        connectButton->setText("🔌 打开串口");
        isConnected = false;
        statusLabel->setText("状态：○ 未连接");
        statusLabel->setStyleSheet("color: gray;");
    }
}

void MainWindow::updateConnectionStatus(QModbusDevice::State state)
{
    switch (state) {
    case QModbusDevice::UnconnectedState:
        statusLabel->setText("状态：○ 未连接");
        statusLabel->setStyleSheet("color: gray;");
        connectButton->setText("🔌 打开串口");
        isConnected = false;
        break;
    case QModbusDevice::ConnectingState:
        statusLabel->setText("状态：⏳ 打开中...");
        statusLabel->setStyleSheet("color: blue;");
        break;
    case QModbusDevice::ConnectedState:
        statusLabel->setText("状态：● 已连接");
        statusLabel->setStyleSheet("color: green;");
        connectButton->setText("❌ 关闭串口");
        isConnected = true;
        appendLog("串口打开成功！");
        break;
    case QModbusDevice::ClosingState:
        statusLabel->setText("状态：⏳ 关闭中...");
        statusLabel->setStyleSheet("color: orange;");
        break;
    }
}

void MainWindow::onErrorOccurred(QModbusDevice::Error error)
{
    QString errorStr = modbusClient->errorString();
    appendLog(QString("❌ 错误：%1").arg(errorStr));
    statusBar()->showMessage(errorStr, 5000);
}

void MainWindow::appendLog(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    combinedLogPanel->appendLog(">>", message);  // 临时用合并日志
}

void MainWindow::appendSendLog(const QByteArray &data)
{
    QString hexStr = toHex(data);
    sendLogPanel->appendLog(">>", hexStr);
    combinedLogPanel->appendLog(">>", hexStr);
}

void MainWindow::appendReceiveLog(const QByteArray &data, bool crcOk)
{
    QString hexStr = toHex(data);
    receiveLogPanel->appendLog("<<", hexStr, crcOk ? "✓" : "✗");
    combinedLogPanel->appendLog("<<", hexStr, crcOk ? "✓" : "✗");
}

void MainWindow::onReadButtonClicked()
{
    if (!isConnected) {
        QMessageBox::warning(this, "警告", "请先打开串口");
        return;
    }
    
    int slaveId = slaveIdSpinBox->value();
    int startAddr = startAddressSpinBox->value();
    int quantity = quantitySpinBox->value();
    int functionCode = functionCombo->currentData().toInt();
    
    QModbusDataUnit::RegisterType registerType;
    if (functionCode == 3)
        registerType = QModbusDataUnit::HoldingRegisters;
    else if (functionCode == 4)
        registerType = QModbusDataUnit::InputRegisters;
    else {
        appendLog("❌ 错误：读操作只支持功能码 03 和 04");
        return;
    }
    
    QModbusDataUnit dataUnit(registerType, startAddr, quantity);
    auto *reply = modbusClient->sendReadRequest(dataUnit, slaveId);
    
    if (reply) {
        QString logMsg = QString("读取：从站=%1, 地址=%2, 数量=%3").arg(slaveId).arg(startAddr).arg(quantity);
        appendLog(logMsg);
        
        connect(reply, &QModbusReply::finished, this, [this, reply, startAddr]() {
            if (reply->error() == QModbusDevice::NoError) {
                const QModbusDataUnit dataUnit = reply->result();
                appendLog("✓ 读取成功");
            } else {
                appendLog(QString("❌ 读取失败：%1").arg(reply->errorString()));
            }
            reply->deleteLater();
        });
    } else {
        appendLog("❌ 发送请求失败");
    }
}

void MainWindow::onWriteButtonClicked()
{
    if (!isConnected) {
        QMessageBox::warning(this, "警告", "请先打开串口");
        return;
    }
    
    int slaveId = slaveIdSpinBox->value();
    int startAddr = startAddressSpinBox->value();
    int functionCode = functionCombo->currentData().toInt();
    QString valueStr = writeValueEdit->text();
    
    QStringList values = valueStr.split(",", Qt::SkipEmptyParts);
    if (values.isEmpty()) {
        QMessageBox::warning(this, "警告", "请输入写入的值");
        return;
    }
    
    QVector<quint16> writeValues;
    for (const QString &v : values) {
        writeValues.append(v.trimmed().toUShort());
    }
    
    QModbusDataUnit dataUnit;
    if (functionCode == 6) {
        dataUnit = QModbusDataUnit(QModbusDataUnit::HoldingRegisters, startAddr, 1);
        dataUnit.setValue(0, writeValues[0]);
    } else if (functionCode == 16) {
        dataUnit = QModbusDataUnit(QModbusDataUnit::HoldingRegisters, startAddr, writeValues.size());
        for (int i = 0; i < writeValues.size(); ++i) {
            dataUnit.setValue(i, writeValues[i]);
        }
    } else {
        appendLog("❌ 错误：写操作只支持功能码 06 和 16");
        return;
    }
    
    auto *reply = modbusClient->sendWriteRequest(dataUnit, slaveId);
    
    if (reply) {
        appendLog(QString("写入：地址=%1, 值=%2").arg(startAddr).arg(valueStr));
        
        connect(reply, &QModbusReply::finished, this, [this, reply]() {
            if (reply->error() == QModbusDevice::NoError) {
                appendLog("✓ 写入成功");
            } else {
                appendLog(QString("❌ 写入失败：%1").arg(reply->errorString()));
            }
            reply->deleteLater();
        });
    } else {
        appendLog("❌ 发送请求失败");
    }
}

void MainWindow::onSendButtonClicked()
{
    if (!isConnected) {
        QMessageBox::warning(this, "警告", "请先打开串口");
        return;
    }
    
    QString hexStr = customDataEdit->text().trimmed();
    if (hexStr.isEmpty()) {
        QMessageBox::warning(this, "警告", "请输入发送数据");
        return;
    }
    
    QByteArray data = fromHex(hexStr);
    if (data.isEmpty()) {
        QMessageBox::warning(this, "警告", "数据格式错误，请输入十六进制数据");
        return;
    }
    
    // 自动附加 CRC
    if (autoCRCCheck->isChecked()) {
        data = appendCRC16(data);
    }
    
    appendSendLog(data);
    
    auto *reply = modbusClient->sendRawRequest(data);
    if (reply) {
        connect(reply, &QModbusReply::finished, this, [reply]() {
            reply->deleteLater();
        });
    }
}

void MainWindow::onScanButtonClicked()
{
    if (!isConnected) {
        QMessageBox::warning(this, "警告", "请先打开串口");
        return;
    }
    
    appendLog("🔍 开始扫描设备...");
    int slaveId = slaveIdSpinBox->value();
    QModbusDataUnit dataUnit(QModbusDataUnit::HoldingRegisters, 0, 10);
    auto *reply = modbusClient->sendReadRequest(dataUnit, slaveId);
    
    if (reply) {
        connect(reply, &QModbusReply::finished, this, [this, reply]() {
            if (reply->error() == QModbusDevice::NoError) {
                appendLog("✓ 设备响应正常");
            } else {
                appendLog(QString("❌ 设备无响应：%1").arg(reply->errorString()));
            }
            reply->deleteLater();
        });
    }
}

void MainWindow::onAddTaskClicked()
{
    QString hexStr = customDataEdit->text().trimmed();
    if (hexStr.isEmpty()) {
        QMessageBox::warning(this, "警告", "请先在自定义发送框输入数据");
        return;
    }
    
    int row = taskTable->rowCount();
    taskTable->insertRow(row);
    
    // 启用复选框
    QCheckBox *check = new QCheckBox();
    check->setChecked(false);
    taskTable->setCellWidget(row, 0, check);
    
    // 数据
    QTableWidgetItem *dataItem = new QTableWidgetItem(hexStr);
    taskTable->setItem(row, 1, dataItem);
    
    // 间隔
    QSpinBox *intervalSpin = new QSpinBox();
    intervalSpin->setRange(100, 60000);
    intervalSpin->setValue(1000);
    intervalSpin->setSuffix(" ms");
    taskTable->setCellWidget(row, 2, intervalSpin);
    
    // 次数
    QSpinBox *countSpin = new QSpinBox();
    countSpin->setRange(-1, 9999);
    countSpin->setValue(-1);
    countSpin->setSpecialValueText("∞");
    taskTable->setCellWidget(row, 3, countSpin);
    
    // 操作按钮
    QPushButton *delBtn = new QPushButton("🗑 删除");
    connect(delBtn, &QPushButton::clicked, this, [this, row]() {
        taskTable->removeRow(row);
        updateCycleStatus();
    });
    taskTable->setCellWidget(row, 4, delBtn);
    
    updateCycleStatus();
}

void MainWindow::updateCycleStatus()
{
    int count = taskTable->rowCount();
    int totalSent = 0;
    for (const auto &task : sendTasks) {
        totalSent += task.currentCount;
    }
    cycleStatusLabel->setText(QString("共 %1 项，已发送 %2 次").arg(count).arg(totalSent));
}

void MainWindow::onStartCycleClicked()
{
    if (taskTable->rowCount() == 0) {
        QMessageBox::warning(this, "警告", "请先添加发送任务");
        return;
    }
    
    // 读取任务配置
    sendTasks.clear();
    for (int i = 0; i < sendTasks.size(); i++) {
        if (sendTasks[i].timer) {
            sendTasks[i].timer->stop();
            delete sendTasks[i].timer;
        }
    }
    sendTasks.clear();
    
    for (int row = 0; row < taskTable->rowCount(); row++) {
        QCheckBox *check = qobject_cast<QCheckBox*>(taskTable->cellWidget(row, 0));
        QTableWidgetItem *dataItem = taskTable->item(row, 1);
        QSpinBox *intervalSpin = qobject_cast<QSpinBox*>(taskTable->cellWidget(row, 2));
        QSpinBox *countSpin = qobject_cast<QSpinBox*>(taskTable->cellWidget(row, 3));
        
        if (!check || !dataItem || !intervalSpin || !countSpin) continue;
        
        SendTaskConfig task;
        task.id = nextTaskId++;
        task.enabled = check->isChecked();
        task.data = dataItem->text();
        task.intervalMs = intervalSpin->value();
        task.maxCount = countSpin->value();
        task.currentCount = 0;
        
        // 创建定时器
        if (task.enabled) {
            task.timer = new QTimer(this);
            connect(task.timer, &QTimer::timeout, this, [this, task]() {
                onTaskSend(task.id);
            });
            task.timer->start(task.intervalMs);
        } else {
            task.timer = nullptr;
        }
        
        sendTasks.append(task);
    }
    
    startCycleButton->setEnabled(false);
    pauseCycleButton->setEnabled(true);
    stopCycleButton->setEnabled(true);
    
    appendLog("🔄 循环发送已启动");
}

void MainWindow::onPauseCycleClicked()
{
    for (auto &task : sendTasks) {
        if (task.timer) task.timer->stop();
    }
    
    pauseCycleButton->setEnabled(false);
    startCycleButton->setEnabled(true);
    startCycleButton->setText("▶ 继续");
    
    appendLog("⏸ 循环发送已暂停");
}

void MainWindow::onStopCycleClicked()
{
    for (auto &task : sendTasks) {
        if (task.timer) {
            task.timer->stop();
            delete task.timer;
        }
        task.currentCount = 0;
    }
    sendTasks.clear();
    
    startCycleButton->setEnabled(true);
    startCycleButton->setText("▶ 启动循环");
    pauseCycleButton->setEnabled(false);
    stopCycleButton->setEnabled(false);
    
    updateCycleStatus();
    appendLog("⏹ 循环发送已停止");
}

void MainWindow::onTaskSend(int taskId)
{
    for (auto &task : sendTasks) {
        if (task.id == taskId) {
            if (task.maxCount >= 0 && task.currentCount >= task.maxCount) {
                task.timer->stop();
                return;
            }
            
            QByteArray data = fromHex(task.data);
            if (autoCRCCheck->isChecked()) {
                data = appendCRC16(data);
            }
            
            appendSendLog(data);
            
            auto *reply = modbusClient->sendRawRequest(data);
            if (reply) {
                connect(reply, &QModbusReply::finished, this, [reply]() {
                    reply->deleteLater();
                });
            }
            
            task.currentCount++;
            updateCycleStatus();
            return;
        }
    }
}

void MainWindow::processReceivedData(const QByteArray &data)
{
    // 帧超时处理：接收到新数据时重置定时器
    resetFrameTimeout();
    
    // 累加到当前帧
    currentFrame.append(data);
    lastByteTime = QDateTime::currentMSecsSinceEpoch();
    
    // 启动/重置超时定时器
    frameTimeoutTimer->start(timeoutSpinBox->value());
}

void MainWindow::resetFrameTimeout()
{
    frameTimeoutTimer->stop();
}

void MainWindow::onFrameTimeout()
{
    // 超时，认为当前帧结束
    if (!currentFrame.isEmpty()) {
        // 验证 CRC
        bool crcOk = verifyCRC16(currentFrame);
        appendReceiveLog(currentFrame, crcOk);
        currentFrame.clear();
    }
}
