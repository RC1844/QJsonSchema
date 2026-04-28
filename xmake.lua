-- QJsonSchema 项目 xmake 配置文件
-- 基于 Qt 的 JSON Schema 验证器库

add_rules("mode.debug", "mode.release")
set_encodings("utf-8")
 

-- 主库目标
set_project("QJsonSchema")
set_version("1.0.0")

-- 静态库目标
target("QJsonSchema")
    add_rules("qt.static")

    -- 添加头文件目录
    add_includedirs("include", {public = true})
    
    -- 添加源文件
    add_files("src/*.cpp")
    
    -- 添加头文件
    add_headerfiles("include/(QJsonSchema/*.h)")

-- 测试程序目标
target("test")
    add_rules("qt.widgetapp")
    
    -- 添加主库依赖
    add_deps("QJsonSchema")
    
    -- 添加测试源文件
    add_files("tests/*.cpp")
