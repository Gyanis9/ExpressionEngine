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

    } // namespace
} // namespace ExpressionEngine::Base
