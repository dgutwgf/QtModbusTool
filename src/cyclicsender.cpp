#include "cyclicsender.h"

CyclicSender::CyclicSender(QObject *parent)
    : QObject(parent), m_timer(new QTimer(this)), m_running(false), m_nextTaskId(1), m_currentTaskIndex(0)
{
    connect(m_timer, &QTimer::timeout, this, &CyclicSender::onTimerTimeout);
    m_timer->setInterval(10);  // 10ms 轮询间隔
}

CyclicSender::~CyclicSender()
{
    stop();
}

int CyclicSender::addTask(const QByteArray &data, int intervalMs, int maxCount)
{
    CyclicTask task;
    task.id = m_nextTaskId++;
    task.enabled = false;  // 默认禁用，需要手动启用
    task.data = data;
    task.intervalMs = intervalMs;
    task.maxCount = maxCount;
    task.currentCount = 0;
    m_tasks.append(task);
    return task.id;
}

void CyclicSender::removeTask(int taskId)
{
    for (int i = 0; i < m_tasks.size(); i++) {
        if (m_tasks[i].id == taskId) {
            m_tasks.removeAt(i);
            break;
        }
    }
}

void CyclicSender::setTaskEnabled(int taskId, bool enabled)
{
    for (auto &task : m_tasks) {
        if (task.id == taskId) {
            task.enabled = enabled;
            break;
        }
    }
}

void CyclicSender::updateTaskData(int taskId, const QByteArray &data)
{
    for (auto &task : m_tasks) {
        if (task.id == taskId) {
            task.data = data;
            break;
        }
    }
}

void CyclicSender::start()
{
    if (m_tasks.isEmpty()) return;
    m_running = true;
    m_timer->start();
}

void CyclicSender::pause()
{
    m_running = false;
    m_timer->stop();
}

void CyclicSender::stop()
{
    m_running = false;
    m_timer->stop();
    
    // 重置所有计数
    for (auto &task : m_tasks) {
        task.currentCount = 0;
    }
}

int CyclicSender::getTotalSentCount() const
{
    int total = 0;
    for (const auto &task : m_tasks) {
        total += task.currentCount;
    }
    return total;
}

void CyclicSender::onTimerTimeout()
{
    if (!m_running || m_tasks.isEmpty()) return;
    
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    
    // 轮询所有启用的任务
    for (auto &task : m_tasks) {
        if (!task.enabled) continue;
        
        // 检查是否达到最大次数
        if (task.maxCount >= 0 && task.currentCount >= task.maxCount) {
            emit taskCompleted(task.id);
            task.enabled = false;  // 自动禁用
            continue;
        }
        
        // 检查时间间隔（简单实现：每个任务独立计时）
        // 实际应该记录每个任务的上次发送时间
        // 这里简化处理，由外部控制发送节奏
    }
    
    // 简化的轮转发送
    m_currentTaskIndex = (m_currentTaskIndex + 1) % m_tasks.size();
}
