#include "QJsonSchema/QJsonSchema.h"
#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

/**
 * @file test_basic.cpp
 * @brief QJsonSchema 基本功能测试
 *
 * 测试 JSON Schema 验证器的基本功能，包括：
 * - 从 JSON 对象加载 schema
 * - 基本类型验证
 * - 简单约束验证
 */

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    qDebug() << "=== QJsonSchema 基本功能测试 ===";

    // 测试1：基本字符串验证
    {
        qDebug() << "\n1. 测试字符串验证";

        // 创建字符串 schema
        QJsonObject schemaObj;
        schemaObj["type"] = "string";
        schemaObj["minLength"] = 3;
        schemaObj["maxLength"] = 10;

        QJsonSchema::JsonSchema schema(schemaObj);

        // 测试有效字符串
        QString error;
        bool result = schema.validate("hello", &error);
        qDebug() << "验证 'hello':" << (result ? "通过" : "失败") << (result ? "" : error);

        // 测试过短字符串
        result = schema.validate("hi", &error);
        qDebug() << "验证 'hi':" << (result ? "通过" : "失败") << (result ? "" : error);

        // 测试过长字符串
        result = schema.validate("thisiswaytoolong", &error);
        qDebug() << "验证 'thisiswaytoolong':" << (result ? "通过" : "失败") << (result ? "" : error);
    }

    // 测试2：数值验证
    {
        qDebug() << "\n2. 测试数值验证";

        QJsonObject schemaObj;
        schemaObj["type"] = "number";
        schemaObj["minimum"] = 0;
        schemaObj["maximum"] = 100;
        schemaObj["multipleOf"] = 5;

        QJsonSchema::JsonSchema schema(schemaObj);

        QString error;

        // 测试有效数值
        bool result = schema.validate(25, &error);
        qDebug() << "验证 25:" << (result ? "通过" : "失败") << (result ? "" : error);

        // 测试无效数值（不是5的倍数）
        result = schema.validate(23, &error);
        qDebug() << "验证 23:" << (result ? "通过" : "失败") << (result ? "" : error);

        // 测试超出范围数值
        result = schema.validate(150, &error);
        qDebug() << "验证 150:" << (result ? "通过" : "失败") << (result ? "" : error);
    }

    // 测试3：对象验证
    {
        qDebug() << "\n3. 测试对象验证";

        QJsonObject schemaObj;
        schemaObj["type"] = "object";

        // 定义属性
        QJsonObject properties;

        QJsonObject nameProp;
        nameProp["type"] = "string";
        properties["name"] = nameProp;

        QJsonObject ageProp;
        ageProp["type"] = "integer";
        ageProp["minimum"] = 0;
        properties["age"] = ageProp;

        schemaObj["properties"] = properties;

        // 必需属性
        QJsonArray required;
        required.append("name");
        schemaObj["required"] = required;

        QJsonSchema::JsonSchema schema(schemaObj);

        // 测试有效对象
        QJsonObject validObj;
        validObj["name"] = "张三";
        validObj["age"] = 25;

        QString error;
        bool result = schema.validate(validObj, &error);
        qDebug() << "验证有效对象:" << (result ? "通过" : "失败") << (result ? "" : error);

        // 测试缺少必需属性
        QJsonObject invalidObj;
        invalidObj["age"] = 25;

        result = schema.validate(invalidObj, &error);
        qDebug() << "验证缺少name的对象:" << (result ? "通过" : "失败") << (result ? "" : error);
    }

    // 测试4：数组验证
    {
        qDebug() << "\n4. 测试数组验证";

        QJsonObject schemaObj;
        schemaObj["type"] = "array";
        schemaObj["minItems"] = 2;
        schemaObj["maxItems"] = 5;

        // 数组元素 schema
        QJsonObject itemSchema;
        itemSchema["type"] = "string";
        schemaObj["items"] = itemSchema;

        QJsonSchema::JsonSchema schema(schemaObj);

        // 测试有效数组
        QJsonArray validArray;
        validArray.append("apple");
        validArray.append("banana");
        validArray.append("cherry");

        QString error;
        bool result = schema.validate(validArray, &error);
        qDebug() << "验证有效数组:" << (result ? "通过" : "失败") << (result ? "" : error);

        // 测试过短数组
        QJsonArray shortArray;
        shortArray.append("apple");

        result = schema.validate(shortArray, &error);
        qDebug() << "验证过短数组:" << (result ? "通过" : "失败") << (result ? "" : error);

        // 测试无效元素类型
        QJsonArray invalidTypeArray;
        invalidTypeArray.append("apple");
        invalidTypeArray.append(123); // 应该是字符串

        result = schema.validate(invalidTypeArray, &error);
        qDebug() << "验证无效类型数组:" << (result ? "通过" : "失败") << (result ? "" : error);
    }

    // 测试5：枚举验证
    {
        qDebug() << "\n5. 测试枚举验证";

        QJsonObject schemaObj;
        schemaObj["enum"] = QJsonArray {"red", "green", "blue"};

        QJsonSchema::JsonSchema schema(schemaObj);

        QString error;

        // 测试有效枚举值
        bool result = schema.validate("red", &error);
        qDebug() << "验证 'red':" << (result ? "通过" : "失败") << (result ? "" : error);

        // 测试无效枚举值
        result = schema.validate("yellow", &error);
        qDebug() << "验证 'yellow':" << (result ? "通过" : "失败") << (result ? "" : error);
    }

    qDebug() << "\n=== 测试完成 ===";

    return 0;
}
