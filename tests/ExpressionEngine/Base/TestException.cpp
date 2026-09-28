// 本文件覆盖统一异常体系的消息、定位信息与捕获面。

#include <gtest/gtest.h>

#include <string>

#include <ExpressionEngine/Base/Exception.h>

namespace ExpressionEngine::Base
{
    namespace
    {

        /**
         * @brief 钉住：what() 与 message() 同源，不出现两套文案
         */
        TEST(ExceptionTest, MessageIsSharedByWhatAndAccessor)
        {
            const Exception exception{"单位不匹配，请先换算成同一单位"};
            EXPECT_EQ(exception.what(), std::string{"单位不匹配，请先换算成同一单位"});
            EXPECT_EQ(exception.message(), "单位不匹配，请先换算成同一单位");
        }

        /**
         * @brief 钉住：抛出点被自动记录，toString 里带文件名与行号
         */
        TEST(ExceptionTest, CapturesThrowSite)
        {
            std::string rendered;
            try
            {
                throw ValueError{"数值超出允许范围，请改成合法取值"};
            } catch (const Exception &caught)
            {
                rendered = caught.toString();
                EXPECT_EQ(caught.sourceLine(), __LINE__ - 4);
                EXPECT_EQ(std::string{caught.file()}.find("TestException.cpp") != std::string::npos, true);
            }

            EXPECT_NE(rendered.find("数值超出允许范围"), std::string::npos);
            EXPECT_NE(rendered.find("TestException.cpp"), std::string::npos);
        }

        /**
         * @brief 钉住：运行期故障都能被统一基类一条 catch 兜住
         */
        TEST(ExceptionTest, AllRuntimeFailuresDeriveFromException)
        {
            const auto expectCaughtAsBase = [](const auto &thrower)
            {
                try
                {
                    thrower();
                    return false;
                } catch (const Exception &)
                {
                    return true;
                }
            };

            EXPECT_TRUE(expectCaughtAsBase([] { throw ParserError{"解析失败"}; }));
            EXPECT_TRUE(expectCaughtAsBase([] { throw UnitsMismatchError{"单位不匹配"}; }));
            EXPECT_TRUE(expectCaughtAsBase([] { throw OverflowError{"上溢"}; }));
            EXPECT_TRUE(expectCaughtAsBase([] { throw UnderflowError{"下溢"}; }));
            EXPECT_TRUE(expectCaughtAsBase([] { throw TypeError{"类型不符"}; }));
            EXPECT_TRUE(expectCaughtAsBase([] { throw IndexError{"下标越界"}; }));
            EXPECT_TRUE(expectCaughtAsBase([] { throw AttributeError{"属性不存在"}; }));
            EXPECT_TRUE(expectCaughtAsBase([] { throw NameError{"名字未定义"}; }));
            EXPECT_TRUE(expectCaughtAsBase([] { throw ExpressionError{"求值失败"}; }));
        }

        /**
         * @brief 钉住：库的异常可被标准库运行期故障分支捕获，便于宿主统一兜底
         */
        TEST(ExceptionTest, DerivesFromStandardRuntimeError)
        {
            try
            {
                throw ParserError{"数量文本无法解析"};
            } catch (const std::runtime_error &caught)
            {
                EXPECT_EQ(caught.what(), std::string{"数量文本无法解析"});
            }
        }

        /**
         * @brief 钉住：每个异常类型报出自己的故障类别，漏标的会落到 Other 而不是静默算对
         * @details 类别是值通道（ParseFailure::kind、EvaluationFailure::kind）的唯一来源，
         *          这张表就是「库内异常体系与类别枚举一一对应」的判据。
         */
        TEST(ExceptionTest, EachErrorTypeReportsItsOwnKind)
        {
            EXPECT_EQ(Exception{"未归类的故障"}.kind(), ErrorKind::Other);
            EXPECT_EQ(ParserError{"文本无法解析"}.kind(), ErrorKind::Parser);
            // 带两个实参的构造要先落成变量：花括号里的逗号会被宏当成参数分隔符
            const ParserError deepText{"嵌套过深，请拆短", ErrorKind::TooDeep};
            const ParserError emptyText{"输入为空", ErrorKind::EmptyInput};
            EXPECT_EQ(deepText.kind(), ErrorKind::TooDeep);
            EXPECT_EQ(emptyText.kind(), ErrorKind::EmptyInput);
            EXPECT_EQ(UnitsMismatchError{"两侧单位不同"}.kind(), ErrorKind::UnitsMismatch);
            EXPECT_EQ(OverflowError{"数值过大"}.kind(), ErrorKind::Overflow);
            EXPECT_EQ(UnderflowError{"数值过小"}.kind(), ErrorKind::Underflow);
            EXPECT_EQ(TypeError{"类型不参与该运算"}.kind(), ErrorKind::Type);
            EXPECT_EQ(ValueError{"取值不被接受"}.kind(), ErrorKind::Value);
            EXPECT_EQ(IndexError{"下标越界"}.kind(), ErrorKind::Index);
            EXPECT_EQ(AttributeError{"属性不存在"}.kind(), ErrorKind::Attribute);
            EXPECT_EQ(NameError{"名字解析不到"}.kind(), ErrorKind::Name);
            EXPECT_EQ(ExpressionError{"引擎运行期故障"}.kind(), ErrorKind::Expression);
        }

        /**
         * @brief 钉住：类别经得住向上转型，catch 基类拿到的与派生类一致
         */
        TEST(ExceptionTest, KindSurvivesUpcast)
        {
            try
            {
                throw UnitsMismatchError{"加法两侧单位不同，请先换算"};
            } catch (const Exception &caught)
            {
                EXPECT_EQ(caught.kind(), ErrorKind::UnitsMismatch);
            }
        }
    } // namespace
} // namespace ExpressionEngine::Base
