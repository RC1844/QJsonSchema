#ifndef SWJSONSCHEMAREGISTRY_H
#define SWJSONSCHEMAREGISTRY_H

#include <QtCore/QMap>
#include <QtCore/QString>

// 前向声明
namespace QJsonSchema {
class SwJsonSchema;
}

/**
 * @file SwJsonSchemaRegistry.h
 * @brief JSON Schema 注册表类
 *
 * 用于管理 JSON Schema 的 $id 和 $anchor 引用
 * 支持 $ref 引用的解析和查找
 */

namespace QJsonSchema {

/**
 * @brief JSON Schema 注册表类
 *
 * 负责存储和管理 SwJsonSchema 实例，支持通过 $id 和 $anchor 进行查找
 * 处理 $ref 引用的解析，如 "<id>#anchor" 或 "#anchor" 格式
 */
class SwJsonSchemaRegistry {
public:
  /**
   * @brief 默认构造函数
   */
  SwJsonSchemaRegistry() = default;

  /**
   * @brief 析构函数
   */
  ~SwJsonSchemaRegistry() = default;

  /**
   * @brief 通过 anchor 注册 schema
   * @param fullAnchor 完整的 anchor 标识符（如 "#anchor" 或 "id#anchor"）
   * @param schema 要注册的 schema 指针
   */
  void registerSchemaByAnchor(const QString &fullAnchor, SwJsonSchema *schema) {
    if (!fullAnchor.isEmpty()) {
      m_schemasByAnchor[fullAnchor] = schema;
    }
  }

  /**
   * @brief 通过路径注册 schema
   * @param path schema 文件路径
   * @param schema 要注册的 schema 指针
   */
  void registerSchemaByRef(const QString &path, SwJsonSchema *schema) {
    if (!path.isEmpty()) {
      m_schemasByRef[path] = schema;
    }
  }

  /**
   * @brief 解析 $ref 引用
   *
   * 支持以下格式的引用：
   * - "https://example.com/sch1#someAnchor"
   * - "#/defs/foo"
   * - "#myAnchor"
   *
   * @param ref 引用字符串
   * @param baseUri 基础 URI
   * @param found 输出参数，表示是否找到引用
   * @return 找到的 schema 指针，未找到返回 nullptr
   */
  SwJsonSchema *resolveRef(const QString &ref, const QString &baseUri,
                           bool &found) const {
    found = true;

    // 简化解析：基于 '#' 字符分割
    QString localBaseURI = baseUri.split("/").last();
    QString localRef = ref;

    // 移除基础 URI 前缀
    if (localRef.startsWith(baseUri)) {
      localRef.remove(0, baseUri.length());
    }
    if (localRef.startsWith(localBaseURI)) {
      localRef.remove(0, localBaseURI.length());
    }

    // 首先尝试直接查找 anchor
    if (m_schemasByAnchor.contains(localRef)) {
      return m_schemasByAnchor.value(localRef);
    }

    // 分割 id 和 anchor 部分
    QString anchor;
    QString idPart = ref;
    if (ref.contains('#')) {
      int idx = ref.indexOf('#');
      idPart = ref.left(idx).trimmed();
      anchor = ref.mid(idx + 1).trimmed(); // 不包括 '#'
    }

    // 1) 如果 idPart 为空，直接查找 "#anchor"
    if (idPart.isEmpty()) {
      QString fullAnchor = "#" + anchor;
      QString fullLocalAnchor = baseUri + "#" + anchor;

      if (m_schemasByAnchor.contains(fullAnchor)) {
        return m_schemasByAnchor.value(fullAnchor);
      }
      if (m_schemasByAnchor.contains(fullLocalAnchor)) {
        return m_schemasByAnchor.value(fullLocalAnchor);
      }
    }

    // 2) 尝试 "idPart#anchor" 格式
    QString fullAnchor = baseUri + "#" + anchor;
    if (m_schemasByAnchor.contains(fullAnchor)) {
      return m_schemasByAnchor.value(fullAnchor);
    }

    // 3) 在路径映射中查找
    for (auto key : m_schemasByRef.keys()) {
      if (!idPart.isEmpty() && key.endsWith(idPart)) {
        found = false;
        return m_schemasByRef.value(key);
      }
    }

    return nullptr;
  }

private:
  QMap<QString, SwJsonSchema *> m_schemasByAnchor; ///< anchor 到 schema 的映射
  QMap<QString, SwJsonSchema *> m_schemasByRef;    ///< 路径到 schema 的映射
};

} // namespace QJsonSchema

#endif // SWJSONSCHEMAREGISTRY_H