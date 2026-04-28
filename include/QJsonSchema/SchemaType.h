#pragma once

#include <cmath>
#include <QtCore/QJsonValue>
#include <QtCore/QString>

/**
 * @file SchemaType.h
 * @brief JSON Schema 类型定义和类型相关工具函数
 *
 * 定义了 JSON Schema 支持的数据类型枚举和类型检查工具
 */

namespace QJsonSchema
{
/**
 * @brief JSON Schema 数据类型枚举
 *
 * 定义了 JSON Schema Draft 7 规范支持的所有数据类型
 */
enum class SchemaType
{
    Invalid, ///< 无效类型
    String,  ///< 字符串类型
    Number,  ///< 数字类型（浮点数）
    Integer, ///< 整数类型
    Boolean, ///< 布尔类型
    Object,  ///< 对象类型
    Array,   ///< 数组类型
    Null     ///< 空值类型
};

/**
 * @brief 将 SchemaType 枚举转换为字符串表示
 * @param t 要转换的 SchemaType
 * @return 对应的类型名称字符串
 */
inline QString toString(SchemaType t)
{
    switch (t) {
        case SchemaType::String:
            return "string";
        case SchemaType::Number:
            return "number";
        case SchemaType::Integer:
            return "integer";
        case SchemaType::Boolean:
            return "boolean";
        case SchemaType::Object:
            return "object";
        case SchemaType::Array:
            return "array";
        case SchemaType::Null:
            return "null";
        default:
            return "invalid";
    }
}

/**
 * @brief 将 QJsonValue 转换为对应的类型字符串
 * @param v 要检查的 JSON 值
 * @return 值的类型字符串表示
 */
inline QString toString(const QJsonValue &v)
{
    if (v.isString())
        return "string";
    if (v.isBool())
        return "boolean";
    if (v.isObject())
        return "object";
    if (v.isArray())
        return "array";
    if (v.isNull())
        return "null";
    if (v.isDouble()) {
        double value = v.toDouble();
        if (std::floor(value) == value)
            return "integer";
        return "number";
    }
    return "undefined";
}

/**
 * @brief 检查 JSON 值是否匹配指定的 SchemaType
 * @param v 要检查的 JSON 值
 * @param expectedType 期望的类型
 * @return 如果值匹配类型则返回 true
 */
inline bool checkType(const QJsonValue &v, SchemaType expectedType)
{
    switch (expectedType) {
        case SchemaType::String:
            return v.isString();
        case SchemaType::Number:
            return v.isDouble();
        case SchemaType::Integer:
            if (v.isDouble()) {
                double value = v.toDouble();
                return std::floor(value) == value;
            }
            return false;
        case SchemaType::Boolean:
            return v.isBool();
        case SchemaType::Object:
            return v.isObject();
        case SchemaType::Array:
            return v.isArray();
        case SchemaType::Null:
            return v.isNull();
        case SchemaType::Invalid:
        default:
            return true; // 无效类型时总是返回 true
    }
}

/**
 * @brief 从 JSON 值推断其 SchemaType
 * @param v 要推断的 JSON 值
 * @return 推断出的 SchemaType
 */
inline SchemaType getType(const QJsonValue &v)
{
    if (v.isString())
        return SchemaType::String;
    if (v.isDouble())
        return SchemaType::Number;
    if (v.isBool())
        return SchemaType::Boolean;
    if (v.isObject())
        return SchemaType::Object;
    if (v.isArray())
        return SchemaType::Array;
    if (v.isNull())
        return SchemaType::Null;
    return SchemaType::Invalid;
}

/**
 * @brief 解析字符串为 SchemaType
 * @param typeStr 类型字符串（如 "string", "number" 等）
 * @return 对应的 SchemaType
 */
inline SchemaType parseType(const QString &typeStr)
{
    QString lowerStr = typeStr.toLower().trimmed();
    if (lowerStr == "string")
        return SchemaType::String;
    if (lowerStr == "number")
        return SchemaType::Number;
    if (lowerStr == "integer")
        return SchemaType::Integer;
    if (lowerStr == "boolean")
        return SchemaType::Boolean;
    if (lowerStr == "object")
        return SchemaType::Object;
    if (lowerStr == "array")
        return SchemaType::Array;
    if (lowerStr == "null")
        return SchemaType::Null;
    return SchemaType::Invalid;
}
} // namespace QJsonSchema
