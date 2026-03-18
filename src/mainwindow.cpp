#include "mainwindow.h"
#include <QMessageBox>
#include <QDateTime>
#include <QHostAddress>
#include <QHeaderView>
#include <QModbusReply>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), isConnected(false)
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
}

void MainWindow::setupUI()
{
    setWindowTitle("Qt Modbus 调试工具 v1.0 - by dgutwgf 🦞");
    resize(900, 700);
    
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    mainLayout = new QVBoxLayout(centralWidget);
    
    // === 连接设置区域 ===
    connectionGroup = new QGroupBox("📡 连接设置");
    auto *connLayout = new QHBoxLayout();
    
    connLayout->addWidget(new QLabel("服务器地址:"));
    serverAddressEdit = new QLineEdit("127.0.0.1");
    serverAddressEdit->setMaximumWidth(150);
    connLayout->addWidget(serverAddressEdit);
    
    connLayout->addWidget(new QLabel("端口:"));
    portSpinBox = new QSpinBox();
    portSpinBox->setRange(1, 65535);
    portSpinBox->setValue(502);
    portSpinBox->setMaximumWidth(80);
    connLayout->addWidget(portSpinBox);
    
    connLayout->addWidget(new QLabel("从站 ID:"));
    slaveIdSpinBox = new QLineEdit("1");
    slaveIdSpinBox->setMaximumWidth(60);
    connLayout->addWidget(slaveIdSpinBox);
    
    connectButton = new QPushButton("连接");
    connectButton->setMaximumWidth(80);
    connLayout->addWidget(connectButton);
    
    statusLabel = new QLabel("状态：未连接");
    statusLabel->setStyleSheet("color: gray;");
    connLayout->addWidget(statusLabel);
    
    connLayout->addStretch();
    connectionGroup->setLayout(connLayout);
    mainLayout->addWidget(connectionGroup);
    
    // === 操作区域 ===
    operationGroup = new QGroupBox("🔧 操作");
    auto *opLayout = new QHBoxLayout();
    
    opLayout->addWidget(new QLabel("功能码:"));
    functionCombo = new QComboBox();
    functionCombo->addItem("03 - 读保持寄存器", 3);
    functionCombo->addItem("04 - 读输入寄存器", 4);
    functionCombo->addItem("06 - 写单个寄存器", 6);
    functionCombo->addItem("16 - 写多个寄存器", 16);
    functionCombo->setMaximumWidth(200);
    opLayout->addWidget(functionCombo);
    
    opLayout->addWidget(new QLabel("起始地址:"));
    startAddressSpinBox = new QSpinBox();
    startAddressSpinBox->setRange(0, 65535);
    startAddressSpinBox->setValue(0);
    startAddressSpinBox->setMaximumWidth(80);
    opLayout->addWidget(startAddressSpinBox);
    
    opLayout->addWidget(new QLabel("数量:"));
    quantitySpinBox = new QSpinBox();
    quantitySpinBox->setRange(1, 125);
    quantitySpinBox->setValue(10);
    quantitySpinBox->setMaximumWidth(60);
    opLayout->addWidget(quantitySpinBox);
    
    opLayout->addWidget(new QLabel("写入值:"));
    valueEdit = new QLineEdit("0");
    valueEdit->setPlaceholderText("逗号分隔多个值");
    opLayout->addWidget(valueEdit);
    
    readButton = new QPushButton("📖 读取");
    opLayout->addWidget(readButton);
    
    writeButton = new QPushButton("✏️ 写入");
    opLayout->addWidget(writeButton);
    
    scanButton = new QPushButton("🔍 扫描");
    opLayout->addWidget(scanButton);
    
    opLayout->addStretch();
    operationGroup->setLayout(opLayout);
    mainLayout->addWidget(operationGroup);
    
    // === 数据显示区域 ===
    dataGroup = new QGroupBox("📊 数据");
    auto *dataLayout = new QVBoxLayout();
    
    dataTable = new QTableWidget();
    dataTable->setColumnCount(2);
    dataTable->setHorizontalHeaderLabels({"地址", "值"});
    dataTable->horizontalHeader()->setStretchLastSection(true);
    dataTable->verticalHeader()->setVisible(false);
    dataLayout->addWidget(dataTable);
    
    dataGroup->setLayout(dataLayout);
    mainLayout->addWidget(dataGroup);
    
    // === 日志区域 ===
    logGroup = new QGroupBox("📝 日志");
    auto *logLayout = new QVBoxLayout();
    
    logEdit = new QTextEdit();
    logEdit->setReadOnly(true);
    logEdit->setMaximumHeight(150);
    logLayout->addWidget(logEdit);
    
    logGroup->setLayout(logLayout);
    mainLayout->addWidget(logGroup);
    
    // 状态栏
    statusBar()->showMessage("就绪");
    
    // 连接信号槽
    connect(connectButton, &QPushButton::clicked, this, &MainWindow::onConnectButtonClicked);
    connect(readButton, &QPushButton::clicked, this, &MainWindow::onReadButtonClicked);
    connect(writeButton, &QPushButton::clicked, this, &MainWindow::onWriteButtonClicked);
    connect(scanButton, &QPushButton::clicked, this, &MainWindow::onScanButtonClicked);
}

void MainWindow::setupModbus()
{
    modbusClient = new QModbusTcpClient(this);
    
    connect(modbusClient, &QModbusTcpClient::stateChanged,
            this, &MainWindow::updateConnectionStatus);
    
    connect(modbusClient, &QModbusTcpClient::errorOccurred,
            this, [this](QModbusDevice::Error error) {
        appendLog(QString("错误：%1").arg(modbusClient->errorString()));
    });
}

void MainWindow::onConnectButtonClicked()
{
    if (!isConnected) {
        // 连接
        QString serverAddress = serverAddressEdit->text();
        int port = portSpinBox->value();
        
        modbusClient->setConnectionParameter(QModbusTcpClient::NetworkAddressParameter, serverAddress);
        modbusClient->setConnectionParameter(QModbusTcpClient::NetworkPortParameter, port);
        
        if (modbusClient->connectDevice()) {
            appendLog(QString("正在连接到 %1:%2...").arg(serverAddress).arg(port));
            connectButton->setText("断开");
        } else {
            QMessageBox::critical(this, "错误", modbusClient->errorString());
            appendLog(QString("连接失败：%1").arg(modbusClient->errorString()));
        }
    } else {
        // 断开
        if (modbusClient->state() != QModbusDevice::UnconnectedState)
            modbusClient->disconnectDevice();
        appendLog("已断开连接");
        connectButton->setText("连接");
        isConnected = false;
        statusLabel->setText("状态：未连接");
        statusLabel->setStyleSheet("color: gray;");
    }
}

void MainWindow::updateConnectionStatus(QModbusDevice::State state)
{
    switch (state) {
    case QModbusDevice::UnconnectedState:
        statusLabel->setText("状态：未连接");
        statusLabel->setStyleSheet("color: gray;");
        connectButton->setText("连接");
        isConnected = false;
        break;
    case QModbusDevice::ConnectingState:
        statusLabel->setText("状态：连接中...");
        statusLabel->setStyleSheet("color: blue;");
        break;
    case QModbusDevice::ConnectedState:
        statusLabel->setText("状态：已连接 ✓");
        statusLabel->setStyleSheet("color: green;");
        connectButton->setText("断开");
        isConnected = true;
        appendLog("连接成功！");
        break;
    case QModbusDevice::ClosingState:
        statusLabel->setText("状态：断开中...");
        statusLabel->setStyleSheet("color: orange;");
        break;
    }
}

void MainWindow::onReadButtonClicked()
{
    if (!isConnected) {
        QMessageBox::warning(this, "警告", "请先连接 Modbus 服务器");
        return;
    }
    
    int slaveId = slaveIdSpinBox->text().toInt();
    int startAddr = startAddressSpinBox->value();
    int quantity = quantitySpinBox->value();
    int functionCode = functionCombo->currentData().toInt();
    
    QModbusDataUnit::RegisterType registerType;
    if (functionCode == 3)
        registerType = QModbusDataUnit::HoldingRegisters;
    else if (functionCode == 4)
        registerType = QModbusDataUnit::InputRegisters;
    else {
        appendLog("错误：只支持读功能码 03 和 04");
        return;
    }
    
    QModbusDataUnit dataUnit(registerType, startAddr, quantity);
    auto *reply = modbusClient->sendReadRequest(dataUnit, slaveId);
    
    if (reply) {
        appendLog(QString("读取请求已发送：地址=%1, 数量=%2").arg(startAddr).arg(quantity));
        
        connect(reply, &QModbusReply::finished, this, [this, reply, startAddr]() {
            if (reply->error() == QModbusReply::Error::NoError) {
                const QModbusDataUnit dataUnit = reply->result();
                dataTable->setRowCount(dataUnit.valueCount());
                
                for (int i = 0; i < dataUnit.valueCount(); ++i) {
                    dataTable->setItem(i, 0, new QTableWidgetItem(QString::number(startAddr + i)));
                    dataTable->setItem(i, 1, new QTableWidgetItem(QString::number(dataUnit.value(i))));
                }
                appendLog("读取成功！");
            } else {
                appendLog(QString("读取失败：%1").arg(reply->errorString()));
            }
            reply->deleteLater();
        });
    } else {
        appendLog("发送请求失败");
    }
}

void MainWindow::onWriteButtonClicked()
{
    if (!isConnected) {
        QMessageBox::warning(this, "警告", "请先连接 Modbus 服务器");
        return;
    }
    
    int slaveId = slaveIdSpinBox->text().toInt();
    int startAddr = startAddressSpinBox->value();
    int functionCode = functionCombo->currentData().toInt();
    QString valueStr = valueEdit->text();
    
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
        appendLog("错误：只支持写功能码 06 和 16");
        return;
    }
    
    auto *reply = modbusClient->sendWriteRequest(dataUnit, slaveId);
    
    if (reply) {
        appendLog(QString("写入请求已发送：地址=%1, 值=%2").arg(startAddr).arg(valueStr));
        
        connect(reply, &QModbusReply::finished, this, [this, reply]() {
            if (reply->error() == QModbusReply::Error::NoError) {
                appendLog("写入成功！✓");
            } else {
                appendLog(QString("写入失败：%1").arg(reply->errorString()));
            }
            reply->deleteLater();
        });
    } else {
        appendLog("发送请求失败");
    }
}

void MainWindow::onScanButtonClicked()
{
    if (!isConnected) {
        QMessageBox::warning(this, "警告", "请先连接 Modbus 服务器");
        return;
    }
    
    appendLog("开始扫描设备...");
    // 简化的扫描功能
    int slaveId = slaveIdSpinBox->text().toInt();
    QModbusDataUnit dataUnit(QModbusDataUnit::HoldingRegisters, 0, 10);
    auto *reply = modbusClient->sendReadRequest(dataUnit, slaveId);
    
    if (reply) {
        connect(reply, &QModbusReply::finished, this, [this, reply]() {
            if (reply->error() == QModbusReply::Error::NoError) {
                appendLog("设备响应正常 ✓");
            } else {
                appendLog(QString("设备无响应：%1").arg(reply->errorString()));
            }
            reply->deleteLater();
        });
    }
}

void MainWindow::appendLog(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
    logEdit->append(QString("[%1] %2").arg(timestamp).arg(message));
    statusBar()->showMessage(message, 5000);
}
