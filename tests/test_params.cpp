/**
 * @file test_params.cpp
 * @brief 参数系统单元测试
 */

#include <gtest/gtest.h>
#include "bt/params.hpp"

using namespace bt;

TEST(ParamsTest, SetAndGetInt) {
    Params p;
    p.set("period", 14);
    EXPECT_EQ(p.get<int>("period"), 14);
}

TEST(ParamsTest, SetAndGetDouble) {
    Params p;
    p.set("factor", 2.5);
    EXPECT_DOUBLE_EQ(p.get<double>("factor"), 2.5);
}

TEST(ParamsTest, SetAndGetString) {
    Params p;
    p.set("name", std::string("test"));
    EXPECT_EQ(p.get<std::string>("name"), "test");
}

TEST(ParamsTest, SetAndGetBool) {
    Params p;
    p.set("enabled", true);
    EXPECT_TRUE(p.get<bool>("enabled"));
}

TEST(ParamsTest, HasParameter) {
    Params p;
    p.set("exists", 1);
    
    EXPECT_TRUE(p.has("exists"));
    EXPECT_FALSE(p.has("notexists"));
}

TEST(ParamsTest, GetWithDefault) {
    Params p;
    
    // 参数不存在时返回默认值
    EXPECT_EQ(p.get<int>("missing", 42), 42);
    EXPECT_DOUBLE_EQ(p.get<double>("missing", 3.14), 3.14);
}

TEST(ParamsTest, GetNonExistent) {
    Params p;
    EXPECT_THROW(p.get<int>("missing"), std::runtime_error);
}

TEST(ParamsTest, Merge) {
    Params base;
    base.set("a", 1);
    base.set("b", 2);
    
    Params other;
    other.set("b", 20);  // 覆盖
    other.set("c", 30);  // 新增
    
    base.merge(other);
    
    EXPECT_EQ(base.get<int>("a"), 1);   // 保持
    EXPECT_EQ(base.get<int>("b"), 2);   // merge 不覆盖已存在的
    EXPECT_EQ(base.get<int>("c"), 30);  // 新增
}

TEST(ParamsTest, Override) {
    Params base;
    base.set("a", 1);
    base.set("b", 2);
    
    Params other;
    other.set("b", 20);
    
    base.override(other);
    
    EXPECT_EQ(base.get<int>("a"), 1);   // 保持
    EXPECT_EQ(base.get<int>("b"), 20);  // override 覆盖
}

TEST(ParamsTest, Keys) {
    Params p;
    p.set("alpha", 1);
    p.set("beta", 2);
    p.set("gamma", 3);
    
    auto keys = p.keys();
    EXPECT_EQ(keys.size(), 3);
}

TEST(ParamsTest, ParamsBuilder) {
    Params p = ParamsBuilder()
        .add("period", 14)
        .add("factor", 2.0)
        .add("name", std::string("SMA"))
        .build();
    
    EXPECT_EQ(p.get<int>("period"), 14);
    EXPECT_DOUBLE_EQ(p.get<double>("factor"), 2.0);
    EXPECT_EQ(p.get<std::string>("name"), "SMA");
}

// 测试宏定义的参数
class TestClass : public Parametrized<TestClass> {
public:
    using Parametrized<TestClass>::Parametrized;  // Inherit constructors
    
    BT_PARAMS_BEGIN()
        BT_PARAM(period, 20)
        BT_PARAM(factor, 2.5)
    BT_PARAMS_END()
};

TEST(ParamsTest, MacroDefinedParams) {
    TestClass obj;
    
    EXPECT_EQ(obj.p().get<int>("period"), 20);
    EXPECT_DOUBLE_EQ(obj.p().get<double>("factor"), 2.5);
}

TEST(ParamsTest, OverrideDefaultParams) {
    Params custom;
    custom.set("period", 50);
    
    TestClass obj(custom);
    
    EXPECT_EQ(obj.p().get<int>("period"), 50);
    EXPECT_DOUBLE_EQ(obj.p().get<double>("factor"), 2.5);  // 默认值保持
}
