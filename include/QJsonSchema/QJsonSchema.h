#ifndef QJSONSCHEMA_H
#define QJSONSCHEMA_H

/**
 * @file QJsonSchema.h
 * @brief QJsonSchema 库主头文件
 * 
 * 基于 Qt 的 JSON Schema 验证器库，支持 JSON Schema Draft 7 规范
 * 
 * 主要组件：
 * - SwJsonSchema: 主要的 JSON Schema 验证器类
 * - SwJsonSchemaRegistry: 模式注册表，用于管理 $ref 引用
 * - KeywordJsonValidator: 自定义关键字验证器
 * 
 * 使用示例：
 * \code
 * SwJsonSchema schema("path/to/schema.json");
 * if (schema.validate(jsonValue)) {
 *     qDebug() << "Validation passed";
 * } else {
 *     qDebug() << "Validation failed";
 * }
 * \endcode
 */

#include "SchemaType.h"
#include "KeywordJsonValidator.h"
#include "SwJsonSchemaRegistry.h"
#include "SwJsonSchema.h"

#endif // QJSONSCHEMA_H