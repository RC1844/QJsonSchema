#pragma once

#include <QtCore/QRegularExpression>
#include <QtCore/QString>

/**
 * @file FormatValidator.h
 * @brief JSON Schema 格式验证器
 *
 * 实现了 JSON Schema Draft 7 规范中定义的格式验证功能
 * 支持 email, date-time, date, time, hostname, ipv4, ipv6, uri, uuid, phone,
 * credit-card 等格式
 */

namespace QJsonSchema
{
/**
 * @brief 格式验证器类
 *
 * 提供静态方法来验证各种预定义格式
 */
class FormatValidator {
public:
    /**
     * @brief 验证字符串是否符合指定的格式
     * @param value 要验证的字符串
     * @param formatName 格式名称（如 "email", "date-time" 等）
     * @param errorMessage 错误消息输出参数（可选）
     * @return 验证通过返回 true，否则返回 false
     */
    static bool checkFormat(const QString &value, const QString &formatName, QString *errorMessage = nullptr);

    /**
     * @brief Luhn 算法检查（用于信用卡号验证）
     * @param ccNumberInput 信用卡号字符串
     * @return 通过 Luhn 检查返回 true，否则返回 false
     */
    static bool luhnCheck(const QString &ccNumberInput);

private:
    // 各种格式的正则表达式模式
    static const QRegularExpression emailRe;    ///< 邮箱格式正则
    static const QRegularExpression dateTimeRe; ///< 日期时间格式正则
    static const QRegularExpression dateRe;     ///< 日期格式正则
    static const QRegularExpression timeRe;     ///< 时间格式正则
    static const QRegularExpression hostnameRe; ///< 主机名格式正则
    static const QRegularExpression ipv4Re;     ///< IPv4 地址格式正则
    static const QRegularExpression ipv6Re;     ///< IPv6 地址格式正则
    static const QRegularExpression uriRe;      ///< URI 格式正则
    static const QRegularExpression uuidRe;     ///< UUID 格式正则
    static const QRegularExpression phoneRe;    ///< 电话号码格式正则
    static const QRegularExpression ccRe;       ///< 信用卡号格式正则
    static const QRegularExpression digitsRe;   ///< 数字格式正则
};
} // namespace QJsonSchema
