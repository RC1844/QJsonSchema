#include "QJsonSchema/FormatValidator.h"

#include <QRegularExpression>
#include <QString>

namespace QJsonSchema
{
// 正则表达式模式定义
const QRegularExpression FormatValidator::emailRe(R"(^[a-zA-Z0-9._%+\-]+@[a-zA-Z0-9.\-]+\.[a-zA-Z]{2,}$)");

const QRegularExpression FormatValidator::dateTimeRe(
    R"(^(?<year>\d{4})-(?<month>\d{2})-(?<day>\d{2})T(?<hour>\d{2}):(?<min>\d{2}):(?<sec>\d{2})(\.\d+)?(Z|[+\-]\d{2}:\d{2})$)");

const QRegularExpression FormatValidator::dateRe(R"(^(?<year>\d{4})-(?<month>\d{2})-(?<day>\d{2})$)");

const QRegularExpression FormatValidator::timeRe(
    R"(^(?<hour>\d{2}):(?<min>\d{2})(:(?<sec>\d{2})(\.\d+)?)?(Z|[+\-]\d{2}:\d{2})?$)");

const QRegularExpression FormatValidator::hostnameRe(
    R"(^(?=.{1,253}$)([a-zA-Z0-9](?:[a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?)(\.([a-zA-Z0-9](?:[a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?))*$)");

const QRegularExpression FormatValidator::ipv4Re(R"(^(25[0-5]|2[0-4]\d|[01]?\d?\d)\.)"
                                                 R"((25[0-5]|2[0-4]\d|[01]?\d?\d)\.)"
                                                 R"((25[0-5]|2[0-4]\d|[01]?\d?\d)\.)"
                                                 R"((25[0-5]|2[0-4]\d|[01]?\d?\d)$)");

const QRegularExpression FormatValidator::ipv6Re(R"((^(([0-9A-Fa-f]{1,4}:){7}([0-9A-Fa-f]{1,4}|:))|)"
                                                 R"(^(::([0-9A-Fa-f]{1,4}:){0,5}((([0-9A-Fa-f]{1,4}))|:))$)");

const QRegularExpression FormatValidator::uriRe(R"(^[A-Za-z][A-Za-z0-9+\-.]*:)"
                                                R"(\/\/?([^\s/]+)(\/\S*)?$)");

const QRegularExpression FormatValidator::uuidRe(
    R"(^[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}$)");

const QRegularExpression FormatValidator::phoneRe(R"(^[+\-()0-9\s]+$)" // 非常宽松的模式
);

const QRegularExpression FormatValidator::ccRe(R"(^\d{13,19}$)");

const QRegularExpression FormatValidator::digitsRe("^[0-9]+$");

// 格式验证方法
bool FormatValidator::checkFormat(const QString& value, const QString& formatName, QString* errorMessage)
{
    const QString lowerFormat = formatName.toLower();
    static QHash<QString, QPair<QRegularExpression, QString>> formatRe = {
        {      "email",                  {emailRe, "邮箱格式无效"}},
        {  "date-time", {dateTimeRe, "日期时间格式无效 (RFC3339)"}},
        {       "date",      {dateRe, "日期格式无效 (YYYY-MM-DD)"}},
        {       "time",                   {timeRe, "时间格式无效"}},
        {   "hostname",             {hostnameRe, "主机名格式无效"}},
        {       "ipv4",              {ipv4Re, "IPv4 地址格式无效"}},
        {       "ipv6",              {ipv6Re, "IPv6 地址格式无效"}},
        {        "uri",                    {uriRe, "URI 格式无效"}},
        {       "uuid",                  {uuidRe, "UUID 格式无效"}},
        {     "digits",                 {digitsRe, "数字格式无效"}},
        {      "phone",              {phoneRe, "电话号码格式无效"}},
        {"credit-card",                 {ccRe, "信用卡号格式无效"}}
    };

    const auto it = formatRe.find(lowerFormat);
    if (it == formatRe.end()) {
        if (errorMessage)
            *errorMessage = "无效的格式名称";
        return false;
    }
    if (!it.value().first.match(value).hasMatch()) {
        if (errorMessage)
            *errorMessage = it.value().second;
        return false;
    }
    if (lowerFormat == "credit-card") {
        // 对于信用卡格式，先检查正则表达式，再进行 Luhn 检查
        if (!luhnCheck(value)) {
            if (errorMessage)
                *errorMessage = "信用卡号无效 (Luhn 检查失败)";
            return false;
        }
    }
    return true;
}

// Luhn 算法检查
bool FormatValidator::luhnCheck(const QString& ccNumberInput)
{
    QString ccNumber = ccNumberInput;
    ccNumber.remove(' ');
    ccNumber.remove('-');

    // 检查是否只包含数字
    if (!digitsRe.match(ccNumber).hasMatch())
        return false;

    // 检查长度
    if (ccNumber.size() < 13 || ccNumber.size() > 19)
        return false;

    int sum = 0;
    bool alternate = false;

    // 从右向左遍历数字
    for (int i = ccNumber.size() - 1; i >= 0; --i) {
        int n = ccNumber.at(i).digitValue();

        if (alternate) {
            n *= 2;
            if (n > 9)
                n = (n % 10) + 1;
        }

        sum += n;
        alternate = !alternate;
    }

    return (sum % 10) == 0;
}
} // namespace QJsonSchema
