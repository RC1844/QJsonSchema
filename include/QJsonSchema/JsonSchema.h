#pragma once

#include "FormatValidator.h"
#include "JsonSchemaRegistry.h"
#include "KeywordJsonValidator.h"
#include "SchemaType.h"

#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QMap>
#include <QtCore/QScopedPointer>
#include <QtCore/QSet>
#include <QtCore/QStringList>

/**
 * @file JsonSchema.h
 * @brief 主要的 JSON Schema 验证器类
 *
 * 实现 JSON Schema Draft 7 规范的验证功能，支持：
 * - 从文件或 JSON 对象加载 schema
 * - 验证 JSON 数据是否符合 schema
 * - 支持 $ref, $id, $anchor 引用
 * - 支持 allOf, anyOf, oneOf, not 等组合操作符
 * - 支持 if/then/else 条件验证
 * - 支持自定义关键字验证
 */

namespace QJsonSchema
{
/**
 * @brief 主要的 JSON Schema 验证器类
 *
 * 提供完整的 JSON Schema 验证功能，支持从文件或 JSON 对象加载 schema
 * 并验证 JSON 数据是否符合 schema 定义
 */
class JsonSchema {
public:
    /**
     * @brief 从文件路径构造 schema
     * @param schemaPath schema 文件路径或 URL
     * @param parent 父 schema 指针（可选）
     */
    explicit JsonSchema(const QString &schemaPath, JsonSchema *parent = nullptr);

    /**
     * @brief 从 JSON 对象构造 schema
     * @param data JSON schema 对象
     * @param parent 父 schema 指针（可选）
     */
    explicit JsonSchema(const QJsonObject &data, JsonSchema *parent = nullptr);

    /**
     * @brief 拷贝构造函数
     * @param other 要拷贝的对象
     */
    JsonSchema(const JsonSchema &other);

    /**
     * @brief 赋值运算符
     * @param other 要赋值的对象
     * @return 当前对象的引用
     */
    JsonSchema &operator=(const JsonSchema &other);

    /**
     * @brief 析构函数
     */
    ~JsonSchema();

    /**
     * @brief 验证 JSON 值是否符合 schema
     * @param value 要验证的 JSON 值
     * @param errorMessage 错误消息输出参数（可选）
     * @return 验证通过返回 true，否则返回 false
     */
    bool validate(const QJsonValue &value, QString *errorMessage = nullptr) const;

    /**
     * @brief 检查 schema 是否有效
     * @return schema 有效返回 true
     */
    bool isValid() const
    {
        return m_isValid;
    }

    /**
     * @brief 注册自定义关键字验证器
     * @param keyword 关键字名称
     * @param validator 验证器函数
     */
    static void registerCustomKeyword(const QString &keyword, const Validator &validator);

    /**
     * @brief 获取父 schema
     * @return 父 schema 指针
     */
    JsonSchema *parent() const
    {
        return m_parent;
    }

    // ========== 新增接口：类型相关 ==========
    /**
     * @brief 获取 schema 类型
     * @return schema 类型枚举
     */
    SchemaType schemaType() const
    {
        return m_type;
    }

    // ========== 新增接口：对象属性 ==========
    /**
     * @brief 获取对象属性列表
     * @return 属性名到 schema 的映射
     */
    QMap<QString, JsonSchema> properties() const
    {
        return m_properties;
    }

    /**
     * @brief 检查是否有指定属性
     * @param name 属性名
     * @return 存在返回 true
     */
    bool hasProperty(const QString &name) const
    {
        return m_properties.contains(name);
    }

    /**
     * @brief 获取指定属性的 schema
     * @param name 属性名
     * @return 属性 schema 指针，不存在返回 nullptr
     */
    const JsonSchema *propertySchema(const QString &name) const;

    /**
     * @brief 获取 patternProperties
     * @return 模式到 schema 的映射
     */
    QMap<QString, JsonSchema> patternProperties() const
    {
        return m_patternProperties;
    }

    // ========== 新增接口：数组相关 ==========
    /**
     * @brief 获取数组项 schema
     * @return 数组项 schema 指针，无定义返回 nullptr
     */
    const JsonSchema *itemsSchema() const
    {
        return m_itemsSchema.data();
    }

    /**
     * @brief 获取 prefixItems schemas (元组验证)
     * @return schema 指针列表
     */
    QList<const JsonSchema *> prefixItemsSchemas() const;

    /**
     * @brief 获取数组最小元素数
     * @return minItems 值，未设置返回 -1
     */
    int minItems() const
    {
        return m_minItems;
    }

    /**
     * @brief 检查是否设置了 minItems
     * @return 设置了 minItems 返回 true
     */
    bool hasMinItems() const
    {
        return m_minItems >= 0;
    }

    /**
     * @brief 获取数组最大元素数
     * @return maxItems 值，未设置返回 -1
     */
    int maxItems() const
    {
        return m_maxItems;
    }

    /**
     * @brief 检查是否设置了 maxItems
     * @return 设置了 maxItems 返回 true
     */
    bool hasMaxItems() const
    {
        return m_maxItems >= 0;
    }

    /**
     * @brief 检查是否要求元素唯一
     * @return uniqueItems 值
     */
    bool uniqueItems() const
    {
        return m_uniqueItems;
    }

    /**
     * @brief 检查是否设置了 uniqueItems
     * @return 设置了 uniqueItems 返回 true
     */
    bool hasUniqueItems() const
    {
        return m_uniqueItems;
    }

    // ========== 新增接口：字符串约束 ==========
    /**
     * @brief 获取字符串最小长度
     * @return minLength 值，未设置返回 -1
     */
    int minLength() const
    {
        return m_minLength;
    }

    /**
     * @brief 检查是否设置了 minLength
     * @return 设置了 minLength 返回 true
     */
    bool hasMinLength() const
    {
        return m_minLength >= 0;
    }

    /**
     * @brief 获取字符串最大长度
     * @return maxLength 值，未设置返回 -1
     */
    int maxLength() const
    {
        return m_maxLength;
    }

    /**
     * @brief 检查是否设置了 maxLength
     * @return 设置了 maxLength 返回 true
     */
    bool hasMaxLength() const
    {
        return m_maxLength >= 0;
    }

    /**
     * @brief 获取正则表达式模式
     * @return pattern 值
     */
    QString pattern() const
    {
        return m_pattern;
    }

    /**
     * @brief 检查是否设置了 pattern
     * @return 设置了 pattern 返回 true
     */
    bool hasPattern() const
    {
        return m_hasPattern;
    }

    /**
     * @brief 获取 format 格式
     * @return format 值
     */
    QString format() const
    {
        return m_format;
    }

    // ========== 新增接口：数值约束 ==========
    /**
     * @brief 获取最小值
     * @return minimum 值
     */
    double minimum() const
    {
        return m_minimum;
    }

    /**
     * @brief 获取最大值
     * @return maximum 值
     */
    double maximum() const
    {
        return m_maximum;
    }

    /**
     * @brief 检查是否设置了 minimum
     * @return 设置了 minimum 返回 true
     */
    bool hasMinimum() const
    {
        return m_hasMinimum;
    }

    /**
     * @brief 检查是否设置了 maximum
     * @return 设置了 maximum 返回 true
     */
    bool hasMaximum() const
    {
        return m_hasMaximum;
    }

    /**
     * @brief 检查是否设置了 exclusiveMinimum
     * @return 设置了 exclusiveMinimum 返回 true
     */
    bool hasExclusiveMinimum() const
    {
        return m_hasMinimum && m_exclusiveMinimum;
    }

    /**
     * @brief 检查是否为 exclusiveMinimum
     * @return exclusiveMinimum 值
     */
    bool exclusiveMinimum() const
    {
        return m_exclusiveMinimum;
    }

    /**
     * @brief 检查是否设置了 exclusiveMaximum
     * @return 设置了 exclusiveMaximum 返回 true
     */
    bool hasExclusiveMaximum() const
    {
        return m_hasMaximum && m_exclusiveMaximum;
    }

    /**
     * @brief 检查是否为 exclusiveMaximum
     * @return exclusiveMaximum 值
     */
    bool exclusiveMaximum() const
    {
        return m_exclusiveMaximum;
    }

    /**
     * @brief 获取 multipleOf 值
     * @return multipleOf 值
     */
    double multipleOf() const
    {
        return m_multipleOf;
    }

    /**
     * @brief 检查是否设置了 multipleOf
     * @return 设置了 multipleOf 返回 true
     */
    bool hasMultipleOf() const
    {
        return m_hasMultipleOf;
    }

    // ========== 新增接口：枚举和常量 ==========
    /**
     * @brief 获取枚举值列表
     * @return enum 值列表
     */
    QList<QJsonValue> enumValues() const
    {
        return m_enumValues;
    }

    /**
     * @brief 检查是否有 enum 定义
     * @return 有 enum 返回 true
     */
    bool hasEnum() const
    {
        return !m_enumValues.isEmpty();
    }

    /**
     * @brief 获取 const 值
     * @return const 值
     */
    QJsonValue constValue() const
    {
        return m_constValue;
    }

    /**
     * @brief 检查是否有 const 定义
     * @return 有 const 返回 true
     */
    bool hasConst() const
    {
        return !m_constValue.isUndefined();
    }

    // ========== 新增接口：Required 字段 ==========
    /**
     * @brief 获取 required 字段列表
     * @return required 字段名集合
     */
    QSet<QString> requiredProperties() const
    {
        return m_required;
    }

    /**
     * @brief 检查字段是否 required
     * @param propertyName 字段名
     * @return 是 required 返回 true
     */
    bool isRequired(const QString &propertyName) const
    {
        return m_required.contains(propertyName);
    }

    /**
     * @brief 检查是否有 additionalProperties=false
     * @return additionalProperties 为 false 返回 true
     */
    bool additionalPropertiesIsFalse() const
    {
        return m_additionalPropertiesIsFalse;
    }

    /**
     * @brief 获取 additionalProperties schema
     * @return schema 指针，无定义返回 nullptr
     */
    const JsonSchema *additionalPropertiesSchema() const
    {
        return m_additionalPropertiesSchema.data();
    }

    // ========== 新增接口：逻辑组合 ==========
    /**
     * @brief 获取 allOf schemas
     * @return schema 列表
     */
    QList<JsonSchema> allOf() const
    {
        return m_allOf;
    }

    /**
     * @brief 获取 anyOf schemas
     * @return schema 列表
     */
    QList<JsonSchema> anyOf() const
    {
        return m_anyOf;
    }

    /**
     * @brief 获取 oneOf schemas
     * @return schema 列表
     */
    QList<JsonSchema> oneOf() const
    {
        return m_oneOf;
    }

    /**
     * @brief 获取 not schema
     * @return schema 指针，无定义返回 nullptr
     */
    const JsonSchema *notSchema() const
    {
        return m_notSchema.data();
    }

    QString getId() const
    {
        return m_id;
    }

    QJsonValue getValue(const QString &name) const
    {
        return m_schemaObject.value(name);
    }

private:
    // 私有方法声明
    void loadSchema(const QJsonObject &schemaObject, const JsonSchema *parent);
    bool validateInternal(const QJsonValue &value, QSet<const JsonSchema *> &visited, QString *errorMessage) const;
    bool applyConditional(const QJsonValue &value, QSet<const JsonSchema *> &visited, QString *errorMessage) const;
    bool checkAllOf(const QJsonValue &value, QSet<const JsonSchema *> &visited, QString *errorMessage) const;
    bool checkAnyOf(const QJsonValue &value, const QSet<const JsonSchema *> &visited, QString *errorMessage) const;
    bool checkOneOf(const QJsonValue &value, const QSet<const JsonSchema *> &visited, QString *errorMessage) const;
    bool validateString(const QJsonValue &value, QString *errorMessage) const;
    bool validateNumber(const QJsonValue &value, QString *errorMessage) const;
    bool validateObject(const QJsonValue &value, const QSet<const JsonSchema *> &visited, QString *errorMessage) const;
    bool validateArray(const QJsonValue &value, const QSet<const JsonSchema *> &visited, QString *errorMessage) const;
    bool checkType(const QJsonValue &v) const;
    void deduceTypeFromConstraints();
    bool matchesAnyPattern(const QString &propertyName) const;
    void copyFrom(const JsonSchema &other);
    JsonSchema *findMainSchema();

    static SchemaType getType(const QJsonValue &v);
    static SchemaType parseType(const QJsonValue &val);
    static QString resolveUri(const JsonSchema *parent, const QString &id);
    static JsonSchemaRegistry *getRegistry(const QString &baseUri);
    static QMap<QString, Validator> &getCustomKeywordRegistry();
    static bool setError(QString *errorMessage, const QString &msg);

    QJsonObject m_schemaObject;

    // 数据成员
    QString m_dollarSchema; ///< $schema 属性
    QString m_baseUri;      ///< 基础 URI
    QString m_dollarAnchor; ///< $anchor 属性
    QString m_dollarRef;    ///< $ref 属性
    QString m_id = "root";

    SchemaType m_type = SchemaType::Invalid;                     ///< schema 类型
    QList<QJsonValue> m_enumValues;                              ///< enum 枚举值列表
    QJsonValue m_constValue = QJsonValue(QJsonValue::Undefined); ///< const 常量值

    // 数值验证相关
    double m_multipleOf = 0.0;       ///< multipleOf 约束
    bool m_hasMultipleOf = false;    ///< 是否设置了 multipleOf
    double m_minimum = 0.0;          ///< minimum 最小值
    double m_maximum = 0.0;          ///< maximum 最大值
    bool m_exclusiveMinimum = false; ///< exclusiveMinimum 标志
    bool m_exclusiveMaximum = false; ///< exclusiveMaximum 标志
    bool m_hasMinimum = false;       ///< 是否设置了 minimum
    bool m_hasMaximum = false;       ///< 是否设置了 maximum

    // 字符串验证相关
    int m_minLength = -1;      ///< minLength 最小长度
    int m_maxLength = -1;      ///< maxLength 最大长度
    QString m_pattern;         ///< pattern 正则表达式
    bool m_hasPattern = false; ///< 是否设置了 pattern
    QString m_format;          ///< format 格式

    // 数组验证相关
    int m_minItems = -1;                                ///< minItems 最小元素数
    int m_maxItems = -1;                                ///< maxItems 最大元素数
    bool m_uniqueItems = false;                         ///< uniqueItems 唯一性标志
    QScopedPointer<JsonSchema> m_itemsSchema;           ///< items schema
    QList<JsonSchema *> m_prefixItemsSchemas;           ///< prefixItems schemas
    QScopedPointer<JsonSchema> m_additionalItemsSchema; ///< additionalItems schema
    JsonSchema *m_recursiveSchema = nullptr;            ///< 递归 schema 引用

    QScopedPointer<JsonSchema> m_containsSchema; ///< contains schema
    int m_minContains = -1;                      ///< minContains 最小包含数
    int m_maxContains = -1;                      ///< maxContains 最大包含数

    // 对象验证相关
    QMap<QString, JsonSchema> m_properties;                  ///< properties 属性映射
    QMap<QString, JsonSchema> m_patternProperties;           ///< patternProperties 模式属性映射
    bool m_additionalPropertiesIsFalse = false;              ///< additionalProperties 为 false
    QScopedPointer<JsonSchema> m_additionalPropertiesSchema; ///< additionalProperties schema
    QSet<QString> m_required;                                ///< required 必需属性集合
    QMap<QString, QStringList> m_dependentRequired;          ///< dependentRequired 依赖属性

    // 逻辑组合验证
    QList<JsonSchema> m_allOf;              ///< allOf schemas
    QList<JsonSchema> m_anyOf;              ///< anyOf schemas
    QList<JsonSchema> m_oneOf;              ///< oneOf schemas
    QScopedPointer<JsonSchema> m_notSchema; ///< not schema

    // 条件验证
    QScopedPointer<JsonSchema> m_ifSchema;   ///< if schema
    QScopedPointer<JsonSchema> m_thenSchema; ///< then schema
    QScopedPointer<JsonSchema> m_elseSchema; ///< else schema

    // 定义
    QMap<QString, JsonSchema> m_defs; ///< $defs 定义

    // 条件验证中验证的属性集合
    mutable QSet<QString> m_ifThenElsePropertyValidated;

    bool m_isValid = false;                                       ///< schema 是否有效
    JsonSchema *m_parent;                                         ///< 父 schema 指针
    QList<KeywordJsonValidator> m_internalCustomKeywordValidator; ///< 自定义关键字验证器
};
} // namespace QJsonSchema
