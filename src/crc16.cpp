#include "crc16.h"
#include <QString>
#include <QByteArray>

quint16 CRC16::calculate(const QByteArray &data)
{
    quint16 crc = 0xFFFF;  // 初始值
    
    for (char c : data) {
        crc ^= (quint16)(unsigned char)c;
        for (int i = 0; i < 8; i++) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;  // 反转多项式
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc;
}

QByteArray CRC16::append(const QByteArray &data)
{
    quint16 crc = calculate(data);
    QByteArray result = data;
    result.append((char)(crc & 0xFF));          // 低字节
    result.append((char)((crc >> 8) & 0xFF));   // 高字节
    return result;
}

bool CRC16::verify(const QByteArray &data)
{
    if (data.size() < 2) return false;
    
    QByteArray payload = data.left(data.size() - 2);
    quint16 received = ((quint16)(unsigned char)data[data.size()-1] << 8) |
                        (quint16)(unsigned char)data[data.size()-2];
    
    return calculate(payload) == received;
}

QString CRC16::toHex(const QByteArray &data, const QString &separator)
{
    QStringList hexList;
    for (unsigned char c : data) {
        hexList << QString("%1").arg(c, 2, 16, QChar('0')).toUpper();
    }
    return hexList.join(separator);
}

QByteArray CRC16::fromHex(const QString &hex)
{
    QByteArray result;
    // 移除所有空格和分隔符
    QString cleanHex = hex.trimmed().remove(QRegularExpression("\\s+"));
    
    // 每两个字符解析为一个字节
    for (int i = 0; i + 1 < cleanHex.size(); i += 2) {
        bool ok;
        quint8 byte = cleanHex.mid(i, 2).toUInt(&ok, 16);
        if (ok) {
            result.append((char)byte);
        }
    }
    
    return result;
}
