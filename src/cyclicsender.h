#ifndef CYCLICSENDER_H
#define CYCLICSENDER_H

#include <QObject>
#include <QTimer>
#include <QList>
#include <QString>
#include <QByteArray>

/**
 * @brief 循环发送任务
 */
struct CyclicTask
{
    int id;                   // 任务 ID
    bool enabled;             // 是否启用
    QByteArray data;          // 发送数据
    int intervalMs;           // 发送间隔（毫秒）
    int maxCount;             // 最大发送次数（-1 表示无限）
    int currentCount;         // 当前已发送次数
    QString lastError;        // 最后错误信息
};

/**
 * @brief 循环发送器
 * 
 * 支持：
 * - 多任务并发执行
 * - 可配置发送间隔和次数
 * - 暂停/恢复控制
 * - 任务动态添加/删除
 */
class CyclicSender : public QObject
{
    Q_OBJECT
    
public:
    explicit CyclicSender(QObject *parent = nullptr);
    ~CyclicSender();
    
    /**
     * @brief 添加循环发送任务
     * @param data 发送数据
     * @param intervalMs 发送间隔（毫秒）
     * @param maxCount 最大发送次数（-1 表示无限循环）
     * @return 任务 ID
     */
    int addTask(const QByteArray &data, int intervalMs, int maxCount = -1);
    
    /**
     * @brief 删除任务
     * @param taskId 任务 ID
     */
    void removeTask(int taskId);
    
    /**
     * @brief 启用/禁用任务
     */
    void setTaskEnabled(int taskId, bool enabled);
    
    /**
     * @brief 更新任务数据
     */
    void updateTaskData(int taskId, const QByteArray &data);
    
    /**
     * @brief 启动所有启用的任务
     */
    void start();
    
    /**
     * @brief 暂停所有任务
     */
    void pause();
    
    /**
     * @brief 停止所有任务并重置计数
     */
    void stop();
    
    /**
     * @brief 是否正在运行
     */
    bool isRunning() const { return m_running; }
    
    /**
     * @brief 获取所有任务
     */
    QList<CyclicTask> getTasks() const { return m_tasks; }
    
    /**
     * @brief 获取总发送次数
     */
    int getTotalSentCount() const;

signals:
    /**
     * @brief 需要发送数据
     * @param taskId 任务 ID
     * @param data 数据
     */
    void sendDataRequested(int taskId, const QByteArray &data);
    
    /**
     * @brief 任务状态变化
     */
    void taskStatusChanged(int taskId, const QString &status);
    
    /**
     * @brief 任务完成（达到最大次数）
     */
    void taskCompleted(int taskId);

private slots:
    void onTimerTimeout();

private:
    QList<CyclicTask> m_tasks;
    QTimer *m_timer;
    bool m_running;
    int m_nextTaskId;
    int m_currentTaskIndex;
};

#endif // CYCLICSENDER_H
