#include "logpanel.h"
#include <QMessageBox>

LogPanel::LogPanel(const QString &title, QWidget *parent)
    : QWidget(parent), m_count(0), m_totalBytes(0), m_autoScroll(true), m_title(title)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    
    // 标题栏
    auto *titleLayout = new QHBoxLayout();
    m_titleLabel = new QLabel(title);
    m_titleLabel->setStyleSheet("font-weight: bold; font-size: 12px;");
    titleLayout->addWidget(m_titleLabel);
    
    m_exportBtn = new QPushButton("💾 导出");
    m_exportBtn->setMaximumWidth(80);
    titleLayout->addWidget(m_exportBtn);
    
    m_clearBtn = new QPushButton("🗑 清空");
    m_clearBtn->setMaximumWidth(80);
    titleLayout->addWidget(m_clearBtn);
    
    titleLayout->addStretch();
    layout->addLayout(titleLayout);
    
    // 日志编辑框
    m_logEdit = new QPlainTextEdit();
    m_logEdit->setReadOnly(true);
    m_logEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_logEdit->setFont(QFont("Consolas", 9));
    m_logEdit->setMinimumHeight(120);
    layout->addWidget(m_logEdit);
    
    // 连接信号槽
    connect(m_exportBtn, &QPushButton::clicked, this, &LogPanel::onExportClicked);
    connect(m_clearBtn, &QPushButton::clicked, this, &LogPanel::onClearClicked);
}

void LogPanel::appendLog(const QString &direction, const QString &data, const QString &crcStatus)
{
    m_count++;
    m_totalBytes += data.split(' ', Qt::SkipEmptyParts).size();
    
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    QString crcMark = crcStatus.isEmpty() ? "" : (crcStatus == "✓" ? " ✓" : " ✗");
    
    QString logLine = QString("[%1] %2 %3%4")
        .arg(timestamp)
        .arg(direction, -3)
        .arg(data)
        .arg(crcMark);
    
    m_logEdit->appendPlainText(logLine);
    
    // 自动滚动
    if (m_autoScroll) {
        QTextCursor cursor = m_logEdit->textCursor();
        cursor.movePosition(QTextCursor::End);
        m_logEdit->setTextCursor(cursor);
    }
    
    updateTitle();
}

void LogPanel::clear()
{
    m_logEdit->clear();
    m_count = 0;
    m_totalBytes = 0;
    updateTitle();
}

QString LogPanel::getLogText() const
{
    return m_logEdit->toPlainText();
}

void LogPanel::updateTitle()
{
    m_titleLabel->setText(QString("%1 (%2 次，%3B)")
        .arg(m_title)
        .arg(m_count)
        .arg(m_totalBytes));
}

void LogPanel::onExportClicked()
{
    if (m_count == 0) {
        QMessageBox::information(this, "提示", "暂无日志可导出");
        return;
    }
    
    QString defaultName = QString("%1_%2.txt")
        .arg(m_title.replace(QRegularExpression("[^\\w\\u4e00-\\u9fa5]"), "_"))
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    
    QString fileName = QFileDialog::getSaveFileName(this, "导出日志", defaultName, "文本文件 (*.txt)");
    
    if (fileName.isEmpty()) return;
    
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << getLogText();
        file.close();
        QMessageBox::information(this, "成功", QString("日志已导出到:\n%1").arg(fileName));
    } else {
        QMessageBox::critical(this, "错误", QString("无法写入文件:\n%1").arg(fileName));
    }
}

void LogPanel::onClearClicked()
{
    clear();
}
