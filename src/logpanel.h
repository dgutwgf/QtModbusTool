#ifndef LOGPANEL_H
#define LOGPANEL_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>

/**
 * @brief 日志面板组件
 * 
 * 支持：
 * - 显示发送/接收日志
 * - 统计发送次数和字节数
 * - 自动滚动到最新日志
 * - 导出为 TXT 文件
 * - 清空日志
 */
class LogPanel : public QWidget
{
    Q_OBJECT
    
public:
    explicit LogPanel(const QString &title, QWidget *parent = nullptr);
    
    /**
     * @brief 添加一条日志
     * @param direction 方向：">>" 发送，"<<" 接收
     * @param data 数据（十六进制字符串）
     * @param crcStatus CRC 校验状态，空字符串表示不显示
     */
    void appendLog(const QString &direction, const QString &data, const QString &crcStatus = "");
    
    /**
     * @brief 清空日志
     */
    void clear();
    
    /**
     * @brief 获取日志内容（纯文本）
     */
    QString getLogText() const;
    
    /**
     * @brief 获取发送/接收次数
     */
    int getCount() const { return m_count; }
    
    /**
     * @brief 获取总字节数
     */
    int getTotalBytes() const { return m_totalBytes; }
    
    /**
     * @brief 设置是否自动滚动
     */
    void setAutoScroll(bool enable) { m_autoScroll = enable; }

private slots:
    void onExportClicked();
    void onClearClicked();

private:
    void updateTitle();
    
    QPlainTextEdit *m_logEdit;
    QLabel *m_titleLabel;
    QPushButton *m_exportBtn;
    QPushButton *m_clearBtn;
    
    int m_count;          // 次数统计
    int m_totalBytes;     // 字节数统计
    bool m_autoScroll;    // 是否自动滚动
    QString m_title;      // 面板标题
};

#endif // LOGPANEL_H
