#pragma once

#include <QtCore/QMap>
#include <QtCore/QString>

// 前向声明
namespace QJsonSchema
{
class JsonSchema;
}

/**
 * @file JsonSchemaRegistry.h
 * @brief JSON Schema 注册表类
 *
 * 用于管理 JSON Schema 的 $id 和 $anchor 引用
 * 支持 $ref 引用的解析和查找
 */

namespace QJsonSchema
{
/**
 * @brief JSON Schema 注册表类
 *
 * 负责存储和管理 JsonSchema 实例，支持通过 $id 和 $anchor 进行查找
 * 处理 $ref 引用的解析，如 "<id>#anchor" 或 "#anchor" 格式
 */
class JsonSchemaRegistry {
public:
    /**
     * @brief 默认构造函数
     */
    JsonSchemaRegistry() = default;

    /**
     * @brief 析构函数
     */
    ~JsonSchemaRegistry() = default;

    /**
     * @brief 通过 anchor 注册 schema
     * @param fullAnchor 完整的 anchor 标识符（如 "#anchor" 或 "id#anchor"）
     * @param schema 要注册的 schema 指针
     */
    void registerSchemaByAnchor(const QString &fullAnchor, JsonSchema *schema)
    {
        if (!fullAnchor.isEmpty())
            m_schemasByAnchor[fullAnchor] = schema;
    }

    /**
     * @brief 通过路径注册 schema
     * @param path schema 文件路径
     * @param schema 要注册的 schema 指针
     */
    void registerSchemaByRef(const QString &path, JsonSchema *schema)
    {
        if (!path.isEmpty())
            m_schemasByRef[path] = schema;
    }

    /**
     * @brief 通过 anchor 查找 schema
     * @param anchor anchor 标识符
     * @return 找到的 schema 指针，未找到返回 nullptr
     */
    JsonSchema *findSchemaByAnchor(const QString &anchor) const
    {
        return m_schemasByAnchor.value(anchor, nullptr);
    }

    /**
     * @brief 通过路径查找 schema
     * @param path schema 文件路径
     * @return 找到的 schema 指针，未找到返回 nullptr
     */
    JsonSchema *findSchemaByRef(const QString &path) const
    {
        return m_schemasByRef.value(path, nullptr);
    }

    /**
     * @brief 清空所有注册的 schema
     */
    void clear()
    {
        m_schemasByAnchor.clear();
        m_schemasByRef.clear();
    }

    /**
     * @brief 获取所有 anchor 映射
     * @return anchor 到 schema 的映射
     */
    QMap<QString, JsonSchema *> schemasByAnchor() const
    {
        return m_schemasByAnchor;
    }

    /**
     * @brief 获取所有路径映射
     * @return 路径到 schema 的映射
     */
    QMap<QString, JsonSchema *> schemasByRef() const
    {
        return m_schemasByRef;
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
    JsonSchema *resolveRef(const QString &ref, const QString &baseUri, bool &found) const
    {
        found = true;

        // 简化解析：基于 '#' 字符分割
        const QString localBaseUri = baseUri.split("/").last();
        QString localRef = ref;

        // 移除基础 URI 前缀
        if (localRef.startsWith(baseUri))
            localRef.remove(0, baseUri.length());
        if (localRef.startsWith(localBaseUri))
            localRef.remove(0, localBaseUri.length());

        // 首先尝试直接查找 anchor
        if (m_schemasByAnchor.contains(localRef))
            return m_schemasByAnchor.value(localRef);

        // 分割 id 和 anchor 部分
        QString anchor;
        QString idPart = ref;
        if (ref.contains('#')) {
            const int idx = ref.indexOf('#');
            idPart = ref.left(idx).trimmed();
            anchor = ref.mid(idx + 1).trimmed(); // 不包括 '#'
        }

        // 1) 如果 idPart 为空，直接查找 "#anchor"
        if (idPart.isEmpty()) {
            const QString fullAnchor = "#" + anchor;
            const QString fullLocalAnchor = baseUri + "#" + anchor;

            if (m_schemasByAnchor.contains(fullAnchor))
                return m_schemasByAnchor.value(fullAnchor);
            if (m_schemasByAnchor.contains(fullLocalAnchor))
                return m_schemasByAnchor.value(fullLocalAnchor);
        }

        // 2) 尝试 "idPart#anchor" 格式
        const QString fullAnchor = baseUri + "#" + anchor;
        if (m_schemasByAnchor.contains(fullAnchor))
            return m_schemasByAnchor.value(fullAnchor);

        // 3) 在路径映射中查找
        for (const QString &key : m_schemasByRef.keys()) {
            if (!idPart.isEmpty() && key.endsWith(idPart)) {
                found = false;
                return m_schemasByRef.value(key);
            }
        }

        return nullptr;
    }

private:
    QMap<QString, JsonSchema *> m_schemasByAnchor; ///< anchor 到 schema 的映射
    QMap<QString, JsonSchema *> m_schemasByRef;    ///< 路径到 schema 的映射
};
} // namespace QJsonSchema
