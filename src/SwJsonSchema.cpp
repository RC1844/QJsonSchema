#include "QJsonSchema/SwJsonSchema.h"
#include "QJsonSchema/FormatValidator.h"

#include <QtCore/QByteArray>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonParseError>
#include <QtCore/QJsonValue>
#include <QtCore/QMap>
#include <QtCore/QRegularExpression>
#include <QtCore/QScopedPointer>
#include <QtCore/QSet>
#include <QtCore/QStringList>
#include <QtCore/QUrl>
#include <QtCore/QtMath>

namespace QJsonSchema {

// 构造函数：从文件路径加载 schema
SwJsonSchema::SwJsonSchema(const QString &schemaPath, SwJsonSchema *parent)
    : m_parent(parent), m_baseUri(schemaPath) {
  if (parent) {
    m_baseUri = resolveUri(parent, schemaPath);
  }

  // 尝试打开本地文件
  QUrl url(schemaPath);
  QString localFile = url.isLocalFile() ? url.toLocalFile() : schemaPath;

  QFile f(localFile);
  if (!f.open(QIODevice::ReadOnly)) {
    // 文件打开失败，schema 无效
    return;
  }

  QByteArray data = f.readAll();
  f.close();

  QJsonParseError jerr;
  QJsonDocument doc = QJsonDocument::fromJson(data, &jerr);
  if (jerr.error != QJsonParseError::NoError) {
    // JSON 解析错误
    return;
  }

  if (!doc.isObject()) {
    // 不是 JSON 对象，schema 无效
    return;
  }

  QJsonObject rootObj = doc.object();
  m_isValid = !rootObj.isEmpty();
  if (m_isValid) {
    loadSchema(rootObj, parent);
  }
}

// 构造函数：从 JSON 对象加载 schema
SwJsonSchema::SwJsonSchema(const QJsonObject &data, SwJsonSchema *parent)
    : m_parent(parent) {
  m_isValid = !data.isEmpty();
  if (m_isValid) {
    loadSchema(data, parent);
  }
}

// 拷贝构造函数
SwJsonSchema::SwJsonSchema(const SwJsonSchema &other) { copyFrom(other); }

// 赋值运算符
SwJsonSchema &SwJsonSchema::operator=(const SwJsonSchema &other) {
  if (this == &other)
    return *this;
  copyFrom(other);
  return *this;
}

// 析构函数
SwJsonSchema::~SwJsonSchema() {
  // 清理 prefixItems schemas
  qDeleteAll(m_prefixItemsSchemas);
  m_prefixItemsSchemas.clear();
}

// 验证 JSON 值是否符合 schema
bool SwJsonSchema::validate(const QJsonValue &value,
                            QString *errorMessage) const {
  QSet<const SwJsonSchema *> visited;
  return validateInternal(value, visited, errorMessage);
}

// 注册自定义关键字
void SwJsonSchema::registerCustomKeyword(const QString &keyword,
                                         Validator validator) {
  getCustomKeywordRegistry()[keyword] = validator;
}

// 私有方法：加载 schema
void SwJsonSchema::loadSchema(const QJsonObject &schemaObject,
                              const SwJsonSchema *parent) {
  // 1) 读取 $schema（可选）
  if (schemaObject.contains("$schema") &&
      schemaObject.value("$schema").isString()) {
    m_dollarSchema = schemaObject.value("$schema").toString();
  }

  // 2) 读取 $id
  if (schemaObject.contains("$id") && schemaObject.value("$id").isString()) {
    m_baseUri = schemaObject.value("$id").toString().trimmed();
  }

  // 3) 计算基础 URI
  m_baseUri = resolveUri(parent, m_baseUri);

  // 4) 读取 $anchor
  if (schemaObject.contains("$anchor") &&
      schemaObject.value("$anchor").isString()) {
    m_dollarAnchor = schemaObject.value("$anchor").toString().trimmed();
    if (!m_dollarAnchor.isEmpty()) {
      QString fullAnchor = m_baseUri + "#" + m_dollarAnchor;
      getRegistry(m_baseUri)->registerSchemaByAnchor(fullAnchor, this);
    }
  }

  // 5) 读取 $ref
  if (schemaObject.contains("$ref") && schemaObject.value("$ref").isString()) {
    m_dollarRef = schemaObject.value("$ref").toString().trimmed();
    if (!m_dollarRef.contains("$def")) {
      if (!m_dollarRef.trimmed().startsWith("#")) {
        QStringList tmpLst = m_baseUri.split("/");
        tmpLst.removeLast();
        tmpLst.append(m_dollarRef.split("#").first());
        SwJsonSchema *ref = new SwJsonSchema(tmpLst.join("/"), this);
        if (ref->m_isValid) {
          getRegistry(m_baseUri)->registerSchemaByRef(tmpLst.join("/"), ref);
        } else {
          delete ref;
          m_dollarRef = "";
        }
      } else if (m_dollarRef.trimmed() == "#") {
        // 递归引用
        m_recursiveSchema = findMainSchema();
      }
    }
  }

  // 6) 加载 $defs
  if (schemaObject.contains("$defs") &&
      schemaObject.value("$defs").isObject()) {
    QJsonObject defsObj = schemaObject.value("$defs").toObject();
    for (auto it = defsObj.begin(); it != defsObj.end(); ++it) {
      if (it.value().isObject()) {
        SwJsonSchema *def = new SwJsonSchema(it.value().toObject(), this);
        getRegistry(m_baseUri)->registerSchemaByAnchor("#/$defs/" + it.key(),
                                                       def);
      }
    }
  }

  // 7) 加载 definitions（旧版本兼容）
  if (schemaObject.contains("definitions") &&
      schemaObject.value("definitions").isObject()) {
    QJsonObject defsObj = schemaObject.value("definitions").toObject();
    for (auto it = defsObj.begin(); it != defsObj.end(); ++it) {
      if (it.value().isObject()) {
        SwJsonSchema *def = new SwJsonSchema(it.value().toObject(), this);
        getRegistry(m_baseUri)->registerSchemaByAnchor(
            "#/definitions/" + it.key(), def);
      }
    }
  }

  // 8) 读取 "type"
  if (schemaObject.contains("type")) {
    m_type = parseType(schemaObject.value("type"));
  }

  // 9) enum / const
  if (schemaObject.contains("enum") && schemaObject.value("enum").isArray()) {
    QJsonArray arr = schemaObject.value("enum").toArray();
    for (const auto &v : arr) {
      m_enumValues.append(v);
    }
  }
  if (schemaObject.contains("const")) {
    m_constValue = schemaObject.value("const");
  }

  // 10) 数值约束
  if (schemaObject.contains("multipleOf")) {
    m_multipleOf = schemaObject.value("multipleOf").toDouble(0.0);
    m_hasMultipleOf = true;
  }
  if (schemaObject.contains("minimum")) {
    m_minimum = schemaObject.value("minimum").toDouble(0.0);
    m_hasMinimum = true;
  }
  if (schemaObject.contains("maximum")) {
    m_maximum = schemaObject.value("maximum").toDouble(0.0);
    m_hasMaximum = true;
  }
  if (schemaObject.contains("exclusiveMinimum")) {
    const QJsonValue exMin = schemaObject.value("exclusiveMinimum");
    if (exMin.isBool()) {
      m_exclusiveMinimum = exMin.toBool(false);
    } else {
      m_minimum = exMin.toDouble(0.0);
      m_hasMinimum = true;
      m_exclusiveMinimum = true;
    }
  }
  if (schemaObject.contains("exclusiveMaximum")) {
    const QJsonValue exMax = schemaObject.value("exclusiveMaximum");
    if (exMax.isBool()) {
      m_exclusiveMaximum = exMax.toBool(false);
    } else {
      m_maximum = exMax.toDouble(0.0);
      m_hasMaximum = true;
      m_exclusiveMaximum = true;
    }
  }

  // 11) 字符串约束
  if (schemaObject.contains("minLength")) {
    m_minLength = schemaObject.value("minLength").toInt(-1);
  }
  if (schemaObject.contains("maxLength")) {
    m_maxLength = schemaObject.value("maxLength").toInt(-1);
  }
  if (schemaObject.contains("pattern")) {
    m_pattern = schemaObject.value("pattern").toString();
    m_hasPattern = true;
  }
  if (schemaObject.contains("format")) {
    m_format = schemaObject.value("format").toString();
  }

  // 12) 数组约束
  if (schemaObject.contains("items")) {
    QJsonValue val = schemaObject.value("items");
    if (val.isObject()) {
      m_itemsSchema.reset(new SwJsonSchema(val.toObject(), this));
    } else if (val.isArray()) {
      QJsonArray arr = val.toArray();
      for (const auto &item : arr) {
        if (item.isObject()) {
          SwJsonSchema *sub = new SwJsonSchema(item.toObject(), this);
          m_prefixItemsSchemas.append(sub);
        }
      }
    }
  }
  if (schemaObject.contains("additionalItems") &&
      schemaObject.value("additionalItems").isObject()) {
    m_additionalItemsSchema.reset(new SwJsonSchema(
        schemaObject.value("additionalItems").toObject(), this));
  }
  if (schemaObject.contains("minItems")) {
    m_minItems = schemaObject.value("minItems").toInt(-1);
  }
  if (schemaObject.contains("maxItems")) {
    m_maxItems = schemaObject.value("maxItems").toInt(-1);
  }
  if (schemaObject.contains("uniqueItems")) {
    m_uniqueItems = schemaObject.value("uniqueItems").toBool(false);
  }

  // 13) contains 约束
  if (schemaObject.contains("contains") &&
      schemaObject.value("contains").isObject()) {
    m_containsSchema.reset(
        new SwJsonSchema(schemaObject.value("contains").toObject(), this));
  }
  if (schemaObject.contains("minContains")) {
    m_minContains = schemaObject.value("minContains").toInt(-1);
  }
  if (schemaObject.contains("maxContains")) {
    m_maxContains = schemaObject.value("maxContains").toInt(-1);
  }

  // 14) 对象约束
  if (schemaObject.contains("properties") &&
      schemaObject.value("properties").isObject()) {
    QJsonObject props = schemaObject.value("properties").toObject();
    for (auto it = props.begin(); it != props.end(); ++it) {
      if (it.value().isObject()) {
        SwJsonSchema sub(it.value().toObject(), this);
        m_properties.insert(it.key(), sub);
      }
    }
  }
  if (schemaObject.contains("patternProperties") &&
      schemaObject.value("patternProperties").isObject()) {
    QJsonObject pprops = schemaObject.value("patternProperties").toObject();
    for (auto it = pprops.begin(); it != pprops.end(); ++it) {
      if (it.value().isObject()) {
        SwJsonSchema sub(it.value().toObject(), this);
        m_patternProperties.insert(it.key(), sub);
      }
    }
  }
  if (schemaObject.contains("additionalProperties")) {
    QJsonValue apVal = schemaObject.value("additionalProperties");
    if (apVal.isBool()) {
      if (!apVal.toBool()) {
        m_additionalPropertiesIsFalse = true;
      }
    } else if (apVal.isObject()) {
      m_additionalPropertiesSchema.reset(
          new SwJsonSchema(apVal.toObject(), this));
    }
  }

  // 15) required / dependentRequired
  if (schemaObject.contains("required") &&
      schemaObject.value("required").isArray()) {
    QJsonArray reqArr = schemaObject.value("required").toArray();
    for (const auto &r : reqArr) {
      if (r.isString()) {
        m_required.insert(r.toString());
      }
    }
  }
  if (schemaObject.contains("dependentRequired") &&
      schemaObject.value("dependentRequired").isObject()) {
    QJsonObject drObj = schemaObject.value("dependentRequired").toObject();
    for (auto it = drObj.begin(); it != drObj.end(); ++it) {
      if (it.value().isArray()) {
        QStringList deps;
        for (const auto &d : it.value().toArray()) {
          deps << d.toString();
        }
        m_dependentRequired.insert(it.key(), deps);
      }
    }
  }

  // 16) 逻辑组合
  if (schemaObject.contains("allOf") && schemaObject.value("allOf").isArray()) {
    QJsonArray arr = schemaObject.value("allOf").toArray();
    for (const auto &sch : arr) {
      if (sch.isObject()) {
        SwJsonSchema sub(sch.toObject(), this);
        m_allOf.append(sub);
      }
    }
  }
  if (schemaObject.contains("anyOf") && schemaObject.value("anyOf").isArray()) {
    QJsonArray arr = schemaObject.value("anyOf").toArray();
    for (const auto &sch : arr) {
      if (sch.isObject()) {
        SwJsonSchema sub(sch.toObject(), this);
        m_anyOf.append(sub);
      }
    }
  }
  if (schemaObject.contains("oneOf") && schemaObject.value("oneOf").isArray()) {
    QJsonArray arr = schemaObject.value("oneOf").toArray();
    for (const auto &sch : arr) {
      if (sch.isObject()) {
        SwJsonSchema sub(sch.toObject(), this);
        m_oneOf.append(sub);
      }
    }
  }
  if (schemaObject.contains("not") && schemaObject.value("not").isObject()) {
    m_notSchema.reset(
        new SwJsonSchema(schemaObject.value("not").toObject(), this));
  }

  // 17) if/then/else
  if (schemaObject.contains("if") && schemaObject.value("if").isObject()) {
    m_ifSchema.reset(
        new SwJsonSchema(schemaObject.value("if").toObject(), this));
  }
  if (schemaObject.contains("then") && schemaObject.value("then").isObject()) {
    m_thenSchema.reset(
        new SwJsonSchema(schemaObject.value("then").toObject(), this));
  }
  if (schemaObject.contains("else") && schemaObject.value("else").isObject()) {
    m_elseSchema.reset(
        new SwJsonSchema(schemaObject.value("else").toObject(), this));
  }

  // 18) 如果类型未定义，尝试从约束推断类型
  if (m_type == SchemaType::Invalid) {
    deduceTypeFromConstraints();
  }

  // 19) 加载自定义关键字
  const auto &customKeywords = getCustomKeywordRegistry();
  for (const QString &key : schemaObject.keys()) {
    if (customKeywords.contains(key)) {
      KeywordJsonValidator userKey(customKeywords.value(key));
      userKey.setRules(schemaObject.value(key));
      m_internalCustomKeywordValidator.append(userKey);
    }
  }
}

// 内部验证方法
bool SwJsonSchema::validateInternal(const QJsonValue &value,
                                    QSet<const SwJsonSchema *> &visited,
                                    QString *errorMessage) const {
  if (visited.contains(this)) {
    return setError(errorMessage, "检测到 schema 递归引用");
  }
  visited.insert(this);

  // 处理 $ref 引用
  if (!m_dollarRef.isEmpty() && m_dollarRef != "#") {
    bool isFound = false;
    SwJsonSchema *refSchema = const_cast<SwJsonSchema *>(this);
    while (!isFound && refSchema != nullptr) {
      refSchema = getRegistry(refSchema->m_baseUri)
                      ->resolveRef(m_dollarRef, refSchema->m_baseUri, isFound);
    }
    if (!refSchema) {
      return setError(errorMessage,
                      QString("无法解析引用 '%1'").arg(m_dollarRef));
    }
    return refSchema->validateInternal(value, visited, errorMessage);
  }

  // if/then/else 条件验证
  if (!applyConditional(value, visited, errorMessage)) {
    return false;
  }

  // not 验证
  if (m_notSchema) {
    if (m_notSchema->validateInternal(value, visited, nullptr)) {
      return setError(errorMessage, "满足 'not' schema，这是不允许的");
    }
  }

  // 逻辑组合验证
  if (!checkAllOf(value, visited, errorMessage))
    return false;
  if (!checkAnyOf(value, visited, errorMessage))
    return false;
  if (!checkOneOf(value, visited, errorMessage))
    return false;

  // enum / const 验证
  if (!m_enumValues.isEmpty()) {
    bool found = false;
    for (auto &ev : m_enumValues) {
      if (ev == value) {
        found = true;
        break;
      }
    }
    if (!found) {
      return setError(errorMessage, "值不在 'enum' 列表中");
    }
  }
  if (!m_constValue.isUndefined()) {
    if (m_constValue != value) {
      return setError(errorMessage, "值与 'const' 不匹配");
    }
  }

  // 类型检查
  if (m_type != SchemaType::Invalid) {
    if (!checkType(value)) {
      return setError(errorMessage, QString("类型无效。期望: %1，实际: %2")
                                        .arg(toString(m_type))
                                        .arg(toString(value)));
    }
  }

  // 详细类型验证
  SchemaType currentType = m_type;
  if (currentType == SchemaType::Invalid) {
    currentType = getType(value);
  }

  switch (currentType) {
  case SchemaType::String:
    if (!validateString(value, errorMessage))
      return false;
    break;
  case SchemaType::Number:
    if (!validateNumber(value, errorMessage))
      return false;
    break;
  case SchemaType::Integer:
    if (!validateNumber(value, errorMessage))
      return false;
    break;
  case SchemaType::Boolean:
    // 布尔类型无需特殊验证
    break;
  case SchemaType::Object:
    if (!validateObject(value, visited, errorMessage))
      return false;
    break;
  case SchemaType::Array:
    if (!validateArray(value, visited, errorMessage))
      return false;
    break;
  case SchemaType::Null:
    // 空值无需特殊验证
    break;
  case SchemaType::Invalid:
  default:
    // 无类型约束
    break;
  }

  // 自定义关键字验证
  for (const KeywordJsonValidator &customValidator :
       m_internalCustomKeywordValidator) {
    if (!customValidator.validate(value, errorMessage)) {
      return setError(errorMessage,
                      QString("自定义关键字验证失败: %1")
                          .arg(errorMessage ? *errorMessage : ""));
    }
  }

  return true;
}

// 条件验证（if/then/else）
bool SwJsonSchema::applyConditional(const QJsonValue &value,
                                    QSet<const SwJsonSchema *> &visited,
                                    QString *errorMessage) const {
  m_ifThenElsePropertyValidated.clear();
  if (!m_ifSchema) {
    return true;
  }

  // 如果 if schema 满足
  if (m_ifSchema->validateInternal(value, visited, nullptr)) {
    // then schema
    if (m_thenSchema &&
        !m_thenSchema->validateInternal(value, visited, errorMessage)) {
      return false;
    }
    if (m_thenSchema) {
      m_ifThenElsePropertyValidated.unite(m_thenSchema->m_required);
    }
  } else {
    // else schema
    if (m_elseSchema &&
        !m_elseSchema->validateInternal(value, visited, errorMessage)) {
      return false;
    }
    if (m_elseSchema) {
      m_ifThenElsePropertyValidated.unite(m_elseSchema->m_required);
    }
  }
  return true;
}

// allOf 验证
bool SwJsonSchema::checkAllOf(const QJsonValue &value,
                              QSet<const SwJsonSchema *> &visited,
                              QString *errorMessage) const {
  for (int i = 0; i < m_allOf.size(); ++i) {
    if (!m_allOf[i].validateInternal(value, visited, errorMessage)) {
      return setError(errorMessage,
                      QString("allOf[%1] 验证失败。%2")
                          .arg(i)
                          .arg(errorMessage ? *errorMessage : ""));
    }
  }
  return true;
}

// anyOf 验证
bool SwJsonSchema::checkAnyOf(const QJsonValue &value,
                              QSet<const SwJsonSchema *> &visited,
                              QString *errorMessage) const {
  if (m_anyOf.isEmpty())
    return true;

  for (int i = 0; i < m_anyOf.size(); ++i) {
    QString localErr;
    QSet<const SwJsonSchema *> visitedCopy(visited);
    if (m_anyOf[i].validateInternal(value, visitedCopy, &localErr)) {
      return true; // 至少一个匹配 => 通过
    }
  }
  return setError(errorMessage, "'anyOf' 中的任何 schema 都不满足");
}

// oneOf 验证
bool SwJsonSchema::checkOneOf(const QJsonValue &value,
                              QSet<const SwJsonSchema *> &visited,
                              QString *errorMessage) const {
  if (m_oneOf.isEmpty())
    return true;

  int countValid = 0;
  QString lastError;
  for (int i = 0; i < m_oneOf.size(); ++i) {
    QString localErr;
    QSet<const SwJsonSchema *> visitedCopy(visited);
    if (m_oneOf[i].validateInternal(value, visitedCopy, &localErr)) {
      countValid++;
      if (countValid > 1) {
        return setError(errorMessage, "'oneOf' 中有多个 schema 满足");
      }
    } else {
      lastError = localErr;
    }
  }

  if (countValid == 1) {
    return true;
  } else {
    return setError(
        errorMessage,
        QString("'oneOf' 中没有任何 schema 满足。最后错误: %1").arg(lastError));
  }
}

// 字符串验证
bool SwJsonSchema::validateString(const QJsonValue &value,
                                  QString *errorMessage) const {
  QString str = value.toString();
  if (m_minLength >= 0 && str.size() < m_minLength) {
    return setError(
        errorMessage,
        QString("字符串长度过短: %1 < %2").arg(str.size()).arg(m_minLength));
  }
  if (m_maxLength >= 0 && str.size() > m_maxLength) {
    return setError(
        errorMessage,
        QString("字符串长度过长: %1 > %2").arg(str.size()).arg(m_maxLength));
  }
  if (m_hasPattern) {
    QRegularExpression re(m_pattern);
    if (!re.match(str).hasMatch()) {
      return setError(errorMessage,
                      QString("不匹配正则表达式: %1").arg(m_pattern));
    }
  }
  if (!m_format.isEmpty()) {
    if (!FormatValidator::checkFormat(str, m_format, errorMessage)) {
      return false;
    }
  }
  return true;
}

// 数值验证
bool SwJsonSchema::validateNumber(const QJsonValue &value,
                                  QString *errorMessage) const {
  if (!checkType(value)) {
    return setError(errorMessage,
                    QString("值 %1 不是数字类型").arg(value.toString()));
  }

  double d = value.toDouble();

  // multipleOf 验证
  if (m_hasMultipleOf && !qFuzzyIsNull(m_multipleOf)) {
    double ratio = d / m_multipleOf;
    double frac = ratio - qFloor(ratio);
    double eps = 1e-12;
    if (qAbs(frac) > eps && qAbs(frac - 1.0) > eps) {
      return setError(errorMessage,
                      QString("%1 不是 %2 的倍数").arg(d).arg(m_multipleOf));
    }
  }

  // minimum 验证
  if (m_hasMinimum) {
    if (m_exclusiveMinimum) {
      if (!(d > m_minimum)) {
        return setError(errorMessage,
                        QString("必须 > %1 (exclusiveMinimum)").arg(m_minimum));
      }
    } else {
      if (d < m_minimum) {
        return setError(errorMessage, QString("必须 >= %1").arg(m_minimum));
      }
    }
  }

  // maximum 验证
  if (m_hasMaximum) {
    if (m_exclusiveMaximum) {
      if (!(d < m_maximum)) {
        return setError(errorMessage,
                        QString("必须 < %1 (exclusiveMaximum)").arg(m_maximum));
      }
    } else {
      if (d > m_maximum) {
        return setError(errorMessage, QString("必须 <= %1").arg(m_maximum));
      }
    }
  }

  return true;
}

// 对象验证
bool SwJsonSchema::validateObject(const QJsonValue &value,
                                  QSet<const SwJsonSchema *> &visited,
                                  QString *errorMessage) const {
  if (!value.isObject()) {
    return setError(errorMessage, "值不是对象");
  }

  QJsonObject obj = value.toObject();

  // required 验证
  for (auto &req : m_required) {
    if (!obj.contains(req)) {
      return setError(errorMessage, QString("缺少必需属性 '%1'").arg(req));
    }
  }

  // dependentRequired 验证
  for (auto it = m_dependentRequired.begin(); it != m_dependentRequired.end();
       ++it) {
    if (obj.contains(it.key())) {
      for (auto &dep : it.value()) {
        if (!obj.contains(dep)) {
          return setError(errorMessage,
                          QString("属性 '%1' 是必需的，因为 '%2' 存在")
                              .arg(dep)
                              .arg(it.key()));
        }
      }
    }
  }

  // properties 验证
  for (auto it = m_properties.begin(); it != m_properties.end(); ++it) {
    if (obj.contains(it.key())) {
      QString localErr;
      QSet<const SwJsonSchema *> visitedCopy(visited);
      if (!it.value().validateInternal(obj.value(it.key()), visitedCopy,
                                       &localErr)) {
        return setError(
            errorMessage,
            QString("属性 '%1' 无效: %2").arg(it.key()).arg(localErr));
      }
    }
  }

  // patternProperties 验证
  for (auto pit = m_patternProperties.begin(); pit != m_patternProperties.end();
       ++pit) {
    QRegularExpression re(pit.key());
    for (auto it = obj.begin(); it != obj.end(); ++it) {
      if (re.match(it.key()).hasMatch()) {
        QString localErr;
        QSet<const SwJsonSchema *> visitedCopy(visited);
        if (!pit.value().validateInternal(it.value(), visitedCopy, &localErr)) {
          return setError(errorMessage,
                          QString("属性 '%1' 无效 (patternProperties / %2): %3")
                              .arg(it.key())
                              .arg(pit.key())
                              .arg(localErr));
        }
      }
    }
  }

  // additionalProperties 验证
  if (m_additionalPropertiesIsFalse) {
    // 拒绝任何未列出的属性
    for (auto it = obj.begin(); it != obj.end(); ++it) {
      if (!m_properties.contains(it.key()) && !matchesAnyPattern(it.key()) &&
          !m_ifThenElsePropertyValidated.contains(it.key())) {
        return setError(
            errorMessage,
            QString("属性 '%1' 不被允许 (additionalProperties=false)")
                .arg(it.key()));
      }
    }
  } else if (m_additionalPropertiesSchema) {
    for (auto it = obj.begin(); it != obj.end(); ++it) {
      if (!m_properties.contains(it.key()) && !matchesAnyPattern(it.key()) &&
          !m_ifThenElsePropertyValidated.contains(it.key())) {
        QString localErr;
        QSet<const SwJsonSchema *> visitedCopy(visited);
        if (!m_additionalPropertiesSchema->validateInternal(
                it.value(), visitedCopy, &localErr)) {
          return setError(errorMessage,
                          QString("属性 '%1' 无效 (additionalProperties): %2")
                              .arg(it.key())
                              .arg(localErr));
        }
      }
    }
  }

  // 递归 schema 验证
  if (m_recursiveSchema) {
    QScopedPointer<SwJsonSchema> _circularRef(
        new SwJsonSchema(*m_recursiveSchema));

    // properties 验证
    for (auto it = _circularRef->m_properties.begin();
         it != _circularRef->m_properties.end(); ++it) {
      if (obj.contains(it.key())) {
        QString localErr;
        QSet<const SwJsonSchema *> visitedCopy(visited);
        if (!it.value().validateInternal(obj.value(it.key()), visitedCopy,
                                         &localErr)) {
          return setError(
              errorMessage,
              QString("属性 '%1' 无效: %2").arg(it.key()).arg(localErr));
        }
      }
    }

    if (_circularRef->m_additionalPropertiesIsFalse) {
      // 拒绝任何未列出的属性
      for (auto it = obj.begin(); it != obj.end(); ++it) {
        if (!_circularRef->m_properties.contains(it.key()) &&
            !_circularRef->matchesAnyPattern(it.key()) &&
            !_circularRef->m_ifThenElsePropertyValidated.contains(it.key())) {
          return setError(
              errorMessage,
              QString("属性 '%1' 不被允许 (additionalProperties=false)")
                  .arg(it.key()));
        }
      }
    } else if (_circularRef->m_additionalPropertiesSchema) {
      for (auto it = obj.begin(); it != obj.end(); ++it) {
        if (!_circularRef->m_properties.contains(it.key()) &&
            !_circularRef->matchesAnyPattern(it.key()) &&
            !_circularRef->m_ifThenElsePropertyValidated.contains(it.key())) {
          QString localErr;
          QSet<const SwJsonSchema *> visitedCopy(visited);
          if (!_circularRef->m_additionalPropertiesSchema->validateInternal(
                  it.value(), visitedCopy, &localErr)) {
            return setError(errorMessage,
                            QString("属性 '%1' 无效 (additionalProperties): %2")
                                .arg(it.key())
                                .arg(localErr));
          }
        }
      }
    }
  }

  return true;
}

// 数组验证
bool SwJsonSchema::validateArray(const QJsonValue &value,
                                 QSet<const SwJsonSchema *> &visited,
                                 QString *errorMessage) const {
  if (!value.isArray()) {
    return setError(errorMessage, "值不是数组");
  }

  QJsonArray arr = value.toArray();

  if (m_minItems >= 0 && arr.size() < m_minItems) {
    return setError(
        errorMessage,
        QString("元素数量过少: %1 < %2").arg(arr.size()).arg(m_minItems));
  }
  if (m_maxItems >= 0 && arr.size() > m_maxItems) {
    return setError(
        errorMessage,
        QString("元素数量过多: %1 > %2").arg(arr.size()).arg(m_maxItems));
  }
  if (m_uniqueItems) {
    for (int i = 0; i < arr.size(); ++i) {
      for (int j = i + 1; j < arr.size(); ++j) {
        if (arr[i] == arr[j]) {
          return setError(errorMessage, "发现重复元素，但 uniqueItems=true");
        }
      }
    }
  }

  // items / prefixItems 验证
  if (m_itemsSchema) {
    for (int i = 0; i < arr.size(); ++i) {
      QString localErr;
      QSet<const SwJsonSchema *> visitedCopy(visited);
      if (!m_itemsSchema->validateInternal(arr[i], visitedCopy, &localErr)) {
        return setError(errorMessage,
                        QString("元素 [%1] 无效: %2").arg(i).arg(localErr));
      }
    }
  } else if (!m_prefixItemsSchemas.isEmpty()) {
    int i = 0;
    for (; i < arr.size() && i < m_prefixItemsSchemas.size(); ++i) {
      QString localErr;
      QSet<const SwJsonSchema *> visitedCopy(visited);
      if (!m_prefixItemsSchemas[i]->validateInternal(arr[i], visitedCopy,
                                                     &localErr)) {
        return setError(
            errorMessage,
            QString("元素 [%1] 无效 (prefixItems): %2").arg(i).arg(localErr));
      }
    }

    // 超出 prefixItems 范围，应用 additionalItemsSchema
    if (i < arr.size()) {
      if (m_additionalItemsSchema) {
        for (; i < arr.size(); ++i) {
          QString localErr;
          QSet<const SwJsonSchema *> visitedCopy(visited);
          if (!m_additionalItemsSchema->validateInternal(arr[i], visitedCopy,
                                                         &localErr)) {
            return setError(errorMessage,
                            QString("元素 [%1] 无效 (additionalItems): %2")
                                .arg(i)
                                .arg(localErr));
          }
        }
      }
      // 否则接受（draft 2019-09）
    }
  }

  // contains 验证
  if (m_containsSchema) {
    int count = 0;
    for (int i = 0; i < arr.size(); ++i) {
      QSet<const SwJsonSchema *> visitedCopy(visited);
      if (m_containsSchema->validateInternal(arr[i], visitedCopy, nullptr)) {
        count++;
      }
    }

    if (m_minContains >= 0 && count < m_minContains) {
      return setError(errorMessage,
                      QString("满足 'contains' 的元素数量不足: %1 < %2")
                          .arg(count)
                          .arg(m_minContains));
    }
    if (m_maxContains >= 0 && count > m_maxContains) {
      return setError(errorMessage,
                      QString("满足 'contains' 的元素数量过多: %1 > %2")
                          .arg(count)
                          .arg(m_maxContains));
    }
    if (m_minContains < 0 && m_maxContains < 0 && count == 0) {
      return setError(errorMessage, "没有任何元素满足 'contains'");
    }
  }

  return true;
}

// 检查属性名是否匹配任何模式
bool SwJsonSchema::matchesAnyPattern(const QString &propertyName) const {
  for (auto it = m_patternProperties.cbegin(); it != m_patternProperties.cend();
       ++it) {
    QRegularExpression re(it.key());
    if (re.match(propertyName).hasMatch()) {
      return true;
    }
  }
  return false;
}

// 从约束推断类型
void SwJsonSchema::deduceTypeFromConstraints() {
  bool mightBeString = (m_minLength >= 0 || m_maxLength >= 0 || m_hasPattern ||
                        !m_format.isEmpty());
  bool mightBeNumber = (m_hasMinimum || m_hasMaximum || m_hasMultipleOf);
  bool mightBeObject =
      (!m_properties.isEmpty() || !m_patternProperties.isEmpty() ||
       !m_required.isEmpty() || m_additionalPropertiesSchema ||
       m_additionalPropertiesIsFalse);
  bool mightBeArray = (!m_prefixItemsSchemas.isEmpty() || m_itemsSchema ||
                       (m_minItems >= 0) || (m_maxItems >= 0));

  int count = 0;
  if (mightBeString)
    count++;
  if (mightBeNumber)
    count++;
  if (mightBeObject)
    count++;
  if (mightBeArray)
    count++;

  if (count == 1) {
    if (mightBeString)
      m_type = SchemaType::String;
    else if (mightBeNumber)
      m_type = SchemaType::Number;
    else if (mightBeObject)
      m_type = SchemaType::Object;
    else if (mightBeArray)
      m_type = SchemaType::Array;
  }
}

// 检查类型
bool SwJsonSchema::checkType(const QJsonValue &v) const {
  return QJsonSchema::checkType(v, m_type);
}

// 获取类型
SchemaType SwJsonSchema::getType(const QJsonValue &v) const {
  return QJsonSchema::getType(v);
}

// 解析类型
SchemaType SwJsonSchema::parseType(const QJsonValue &val) const {
  if (!val.isString()) {
    return SchemaType::Invalid;
  }
  return QJsonSchema::parseType(val.toString());
}

// 拷贝数据
void SwJsonSchema::copyFrom(const SwJsonSchema &other) {
  m_dollarSchema = other.m_dollarSchema;
  m_baseUri = other.m_baseUri;
  m_dollarAnchor = other.m_dollarAnchor;
  m_dollarRef = other.m_dollarRef;

  m_type = other.m_type;
  m_enumValues = other.m_enumValues;
  m_constValue = other.m_constValue;

  m_multipleOf = other.m_multipleOf;
  m_hasMultipleOf = other.m_hasMultipleOf;
  m_minimum = other.m_minimum;
  m_maximum = other.m_maximum;
  m_exclusiveMinimum = other.m_exclusiveMinimum;
  m_exclusiveMaximum = other.m_exclusiveMaximum;
  m_hasMinimum = other.m_hasMinimum;
  m_hasMaximum = other.m_hasMaximum;

  m_minLength = other.m_minLength;
  m_maxLength = other.m_maxLength;
  m_pattern = other.m_pattern;
  m_hasPattern = other.m_hasPattern;
  m_format = other.m_format;

  m_minItems = other.m_minItems;
  m_maxItems = other.m_maxItems;
  m_uniqueItems = other.m_uniqueItems;

  // 拷贝指针对象
  m_itemsSchema.reset(
      other.m_itemsSchema ? new SwJsonSchema(*other.m_itemsSchema) : nullptr);
  m_additionalItemsSchema.reset(
      other.m_additionalItemsSchema
          ? new SwJsonSchema(*other.m_additionalItemsSchema)
          : nullptr);

  // 拷贝 prefixItems
  qDeleteAll(m_prefixItemsSchemas);
  m_prefixItemsSchemas.clear();
  for (auto *p : other.m_prefixItemsSchemas) {
    m_prefixItemsSchemas.append(new SwJsonSchema(*p));
  }

  // contains
  m_containsSchema.reset(other.m_containsSchema
                             ? new SwJsonSchema(*other.m_containsSchema)
                             : nullptr);
  m_minContains = other.m_minContains;
  m_maxContains = other.m_maxContains;

  // properties, patternProperties
  m_properties = other.m_properties;
  m_patternProperties = other.m_patternProperties;
  m_additionalPropertiesIsFalse = other.m_additionalPropertiesIsFalse;
  m_additionalPropertiesSchema.reset(
      other.m_additionalPropertiesSchema
          ? new SwJsonSchema(*other.m_additionalPropertiesSchema)
          : nullptr);

  m_required = other.m_required;
  m_dependentRequired = other.m_dependentRequired;
  m_allOf = other.m_allOf;
  m_anyOf = other.m_anyOf;
  m_oneOf = other.m_oneOf;

  m_notSchema.reset(other.m_notSchema ? new SwJsonSchema(*other.m_notSchema)
                                      : nullptr);
  m_ifSchema.reset(other.m_ifSchema ? new SwJsonSchema(*other.m_ifSchema)
                                    : nullptr);
  m_thenSchema.reset(other.m_thenSchema ? new SwJsonSchema(*other.m_thenSchema)
                                        : nullptr);
  m_elseSchema.reset(other.m_elseSchema ? new SwJsonSchema(*other.m_elseSchema)
                                        : nullptr);

  m_defs = other.m_defs;
  m_isValid = other.m_isValid;
  m_parent = other.m_parent;
  m_recursiveSchema = other.m_recursiveSchema;
  m_internalCustomKeywordValidator = other.m_internalCustomKeywordValidator;
}

// 解析 URI
QString SwJsonSchema::resolveUri(const SwJsonSchema *parent,
                                 const QString &id) const {
  if (!parent || parent->m_baseUri.isEmpty()) {
    return id.trimmed();
  }

  QUrl baseUrl(parent->m_baseUri);
  QUrl relativeUrl(id.trimmed());
  QUrl resolved = baseUrl.resolved(relativeUrl);

  return resolved.toString();
}

// 获取注册表
SwJsonSchemaRegistry *SwJsonSchema::getRegistry(const QString &baseUri) {
  static QMap<QString, SwJsonSchemaRegistry *> registryBook;
  if (!registryBook.contains(baseUri.toLower())) {
    registryBook.insert(baseUri.toLower(), new SwJsonSchemaRegistry());
  }
  return registryBook.value(baseUri.toLower());
}

// 查找主 schema
SwJsonSchema *SwJsonSchema::findMainSchema() {
  SwJsonSchema *searched = this;
  while (searched->parent()) {
    searched = searched->parent();
  }
  return searched;
}

// 获取自定义关键字注册表
QMap<QString, Validator> &SwJsonSchema::getCustomKeywordRegistry() {
  static QMap<QString, Validator> customKeywordRegistry;
  return customKeywordRegistry;
}

// 设置错误消息
bool SwJsonSchema::setError(QString *errorMessage, const QString &msg) {
  if (errorMessage) {
    *errorMessage = msg;
  }
  return false;
}

} // namespace QJsonSchema