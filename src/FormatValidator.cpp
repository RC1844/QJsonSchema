#include "QJsonSchema/FormatValidator.h"

#include <QRegularExpression>
#include <QString>

namespace QJsonSchema {

// 正则表达式模式定义
const QRegularExpression FormatValidator::emailRe(
    R"(^[a-zA-Z0-9._%+\-]+@[a-zA-Z0-9.\-]+\.[a-zA-Z]{2,}$)"
);

const QRegularExpression FormatValidator::dateTimeRe(
    R"(^(?<year>\d{4})-(?<month>\d{2})-(?<day>\d{2})T(?<hour>\d{2}):(?<min>\d{2}):(?<sec>\d{2})(\.\d+)?(Z|[+\-]\d{2}:\d{2})$)"
);

const QRegularExpression FormatValidator::dateRe(
    R"(^(?<year>\d{4})-(?<month>\d{2})-(?<day>\d{2})$)"
);

const QRegularExpression FormatValidator::timeRe(
    R"(^(?<hour>\d{2}):(?<min>\d{2})(:(?<sec>\d{2})(\.\d+)?)?(Z|[+\-]\d{2}:\d{2})?$)"
);

const QRegularExpression FormatValidator::hostnameRe(
    R"(^(?=.{1,253}$)([a-zA-Z0-9](?:[a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?)(\.([a-zA-Z0-9](?:[a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?))*$)"
);

const QRegularExpression FormatValidator::ipv4Re(
    R"(^(25[0-5]|2[0-4]\d|[01]?\d?\d)\.)"
    R"((25[0-5]|2[0-4]\d|[01]?\d?\d)\.)"
    R"((25[0-5]|2[0-4]\d|[01]?\d?\d)\.)"
    R"((25[0-5]|2[0-4]\d|[01]?\d?\d)$)"
);

const QRegularExpression FormatValidator::ipv6Re(
    R"((^(([0-9A-Fa-f]{1,4}:){7}([0-9A-Fa-f]{1,4}|:))|)"
    R"(^(::([0-9A-Fa-f]{1,4}:){0,5}((([0-9A-Fa-f]{1,4}))|:))$)"
);

const QRegularExpression FormatValidator::uriRe(
    R"(^[A-Za-z][A-Za-z0-9+\-.]*:)"
    R"(\/\/?([^\s/]+)(\/\S*)?$)"
);

const QRegularExpression FormatValidator::uuidRe(
    R"(^[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}$)"
);

const QRegularExpression FormatValidator::phoneRe(
    R"(^[+\-()0-9\s]+$)"  // 非常宽松的模式
);

const QRegularExpression FormatValidator::ccRe(
    R"(^\d{13,19}$)"
);

const QRegularExpression FormatValidator::digitsRe(
    "^[0-9]+$"
);

// 格式验证方法
bool FormatValidator::checkFormat(const QString &value, const QString &formatName, QString *errorMessage)
{
    QString lowerFormat = formatName.toLower();
    
    // -- "email" --
    if (lowerFormat == "email") {
        if (!emailRe.match(value).hasMatch()) {
            if (errorMessage) {
                *errorMessage = "邮箱格式无效";
            }
            return false;
        }
        return true;
    }
    
    // -- "date-time" (RFC3339 简化版) --
    else if (lowerFormat == "date-time") {
        if (!dateTimeRe.match(value).hasMatch()) {
            if (errorMessage) {
                *errorMessage = "日期时间格式无效 (RFC3339)";
            }
            return false;
        }
        return true;
    }
    
    // -- "date" (YYYY-MM-DD) --
    else if (lowerFormat == "date") {
        if (!dateRe.match(value).hasMatch()) {
            if (errorMessage) {
                *errorMessage = "日期格式无效 (YYYY-MM-DD)";
            }
            return false;
        }
        return true;
    }
    
    // -- "time" (hh:mm:ss(.fraction)?(Z|±hh:mm)?) --
    else if (lowerFormat == "time") {
        if (!timeRe.match(value).hasMatch()) {
            if (errorMessage) {
                *errorMessage = "时间格式无效";
            }
            return false;
        }
        return true;
    }
    
    // -- "hostname" (RFC1123 简化版) --
    else if (lowerFormat == "hostname") {
        if (!hostnameRe.match(value).hasMatch()) {
            if (errorMessage) {
                *errorMessage = "主机名格式无效";
            }
            return false;
        }
        return true;
    }
    
    // -- "ipv4" --
    else if (lowerFormat == "ipv4") {
        if (!ipv4Re.match(value).hasMatch()) {
            if (errorMessage) {
                *errorMessage = "IPv4 地址格式无效";
            }
            return false;
        }
        return true;
    }
    
    // -- "ipv6" (简化版，包含 '::') --
    else if (lowerFormat == "ipv6") {
        if (!ipv6Re.match(value).hasMatch()) {
            if (errorMessage) {
                *errorMessage = "IPv6 地址格式无效";
            }
            return false;
        }
        return true;
    }
    
    // -- "uri" (简化版) --
    else if (lowerFormat == "uri") {
        if (!uriRe.match(value).hasMatch()) {
            if (errorMessage) {
                *errorMessage = "URI 格式无效";
            }
            return false;
        }
        return true;
    }
    
    // -- "uuid" (RFC 4122 版本4/标准) --
    else if (lowerFormat == "uuid") {
        if (!uuidRe.match(value).hasMatch()) {
            if (errorMessage) {
                *errorMessage = "UUID 格式无效";
            }
            return false;
        }
        return true;
    }
    
    // -- "phone" (非常通用的模式) --
    else if (lowerFormat == "phone") {
        if (!phoneRe.match(value).hasMatch()) {
            if (errorMessage) {
                *errorMessage = "电话号码格式无效";
            }
            return false;
        }
        return true;
    }
    
    // -- "credit-card" : Regex + Luhn 检查 --
    else if (lowerFormat == "credit-card") {
        // 简单正则：13到19位数字
        if (!ccRe.match(value).hasMatch()) {
            if (errorMessage) {
                *errorMessage = "信用卡号格式无效 (必须是13-19位数字)";
            }
            return false;
        }
        // Luhn 算法验证
        if (!luhnCheck(value)) {
            if (errorMessage) {
                *errorMessage = "信用卡号无效 (Luhn 检查失败)";
            }
            return false;
        }
        return true;
    }
    
    // 未知格式
    if (errorMessage) {
        *errorMessage = QString("未知的格式: %1").arg(formatName);
    }
    return false;
}

// Luhn 算法检查
bool FormatValidator::luhnCheck(const QString &ccNumberInput)
{
    QString ccNumber = ccNumberInput;
    ccNumber.remove(' ');
    ccNumber.remove('-');
    
    // 检查是否只包含数字
    if (!digitsRe.match(ccNumber).hasMatch()) {
        return false;
    }
    
    // 检查长度
    if (ccNumber.size() < 13 || ccNumber.size() > 19) {
        return false;
    }

    int sum = 0;
    bool alternate = false;
    
    // 从右向左遍历数字
    for (int i = ccNumber.size() - 1; i >= 0; --i) {
        int n = ccNumber.at(i).digitValue();
        
        if (alternate) {
            n *= 2;
            if (n > 9) {
                n = (n % 10) + 1;
            }
        }
        
        sum += n;
        alternate = !alternate;
    }
    
    return (sum % 10) == 0;
}

} // namespace QJsonSchema