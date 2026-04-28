#ifndef KEYWORDJSONVALIDATOR_H
#define KEYWORDJSONVALIDATOR_H

#include <QtCore/QJsonValue>
#include <QtCore/QString>
#include <functional>

/**
 * @file KeywordJsonValidator.h
 * @brief 自定义 JSON Schema 关键字验证器类
 *
 * 用于注册和验证自定义的 JSON Schema 关键字
 * 支持 lambda 函数作为验证器
 */

namespace QJsonSchema {

/**
 * @brief 验证器函数类型定义
 *
 * 验证器函数接受三个参数：
 * - schemaRules: JSON Schema 规则
 * - data: 要验证的数据
 * - errorMessage: 错误消息指针（可选）
 *
 * 返回 true 表示验证通过，false 表示验证失败
 */
using Validator =
    std::function<bool(const QJsonValue &, const QJsonValue &, QString *)>;

/**
 * @brief 自定义关键字验证器类
 *
 * 封装了自定义关键字的验证逻辑，支持规则设置和验证执行
 */
class KeywordJsonValidator {
public:
  /**
   * @brief 使用验证器函数构造
   * @param validator 验证器 lambda 函数
   */
  explicit KeywordJsonValidator(Validator validator)
      : m_validator(std::move(validator)) {}

  /**
   * @brief 默认构造函数
   */
  KeywordJsonValidator() = default;

  /**
   * @brief 拷贝构造函数
   * @param other 要拷贝的对象
   */
  KeywordJsonValidator(const KeywordJsonValidator &other)
      : m_validator(other.m_validator),
        m_jsonSchemaValidator(other.m_jsonSchemaValidator) {}

  /**
   * @brief 赋值运算符
   * @param other 要赋值的对象
   * @return 当前对象的引用
   */
  KeywordJsonValidator &operator=(const KeywordJsonValidator &other) {
    if (this != &other) {
      m_validator = other.m_validator;
      m_jsonSchemaValidator = other.m_jsonSchemaValidator;
    }
    return *this;
  }

  /**
   * @brief 设置验证规则
   * @param rules JSON Schema 规则值
   */
  void setRules(const QJsonValue &rules) { m_jsonSchemaValidator = rules; }

  /**
   * @brief 验证数据是否符合规则
   * @param data 要验证的数据
   * @param errorMessage 错误消息输出参数（可选）
   * @return 验证通过返回 true，否则返回 false
   */
  bool validate(const QJsonValue &data, QString *errorMessage = nullptr) const {
    if (m_validator) {
      return m_validator(m_jsonSchemaValidator, data, errorMessage);
    }
    return false;
  }

  /**
   * @brief 比较运算符（仅比较规则，不比较验证器函数）
   * @param other 要比较的对象
   * @return 如果规则相同则返回 true
   */
  bool operator==(const KeywordJsonValidator &other) const {
    return m_jsonSchemaValidator == other.m_jsonSchemaValidator;
  }

  /**
   * @brief 析构函数
   */
  ~KeywordJsonValidator() = default;

private:
  Validator m_validator;            ///< 验证器函数
  QJsonValue m_jsonSchemaValidator; ///< JSON Schema 验证规则
};

} // namespace QJsonSchema

#endif // KEYWORDJSONVALIDATOR_H