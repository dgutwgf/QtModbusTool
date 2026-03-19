#ifndef CRC16_H
#define CRC16_H

#include <QByteArray>
#include <QtGlobal>
#include <QString>
#include <QStringList>

/**
 * @brief Modbus RTU CRC-16 校验工具类
 * 
 * 多项式：0x8005 (反转：0xA001)
 * 初始值：0xFFFF
 * 输出：低字节在前，高字节在后
 */
class CRC16
{
public:
    /**
     * @brief 计算 CRC-16 校验值
     * @param data 数据字节数组
     * @return CRC-16 校验值
     */
    static quint16 calculate(const QByteArray &data);
    
    /**
     * @brief 在数据末尾附加 CRC 校验码
     * @param data 原始数据
     * @return 附加 CRC 后的完整数据
     */
    static QByteArray append(const QByteArray &data);
    
    /**
     * @brief 验证带 CRC 的数据是否正确
     * @param data 包含 CRC 的完整数据（最后 2 字节为 CRC）
     * @return true 校验成功，false 校验失败
     */
    static bool verify(const QByteArray &data);
    
    /**
     * @brief 将数据格式化为十六进制字符串
     * @param data 字节数组
     * @param separator 分隔符，默认空格
     * @return 十六进制字符串，如 "01 03 00 0A C5 CD"
     */
    static QString toHex(const QByteArray &data, const QString &separator = " ");
    
    /**
     * @brief 将十六进制字符串解析为字节数组
     * @param hex 十六进制字符串，如 "01 03 00 0A" 或 "0103000A"
     * @return 字节数组
     */
    static QByteArray fromHex(const QString &hex);
};

#endif // CRC16_H
