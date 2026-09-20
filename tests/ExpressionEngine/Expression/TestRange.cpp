// 本文件钉住单元格地址与区间的编解码、比较与遍历：列标往返、绝对引用标记、端点整理与访问顺序。

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Expression/Range.h>

namespace ExpressionEngine::Expression
{
    namespace
    {

        /// 按 next() 的走法收集区间访问到的地址文本
        std::vector<std::string> visitedCells(const Range &range)
        {
            std::vector<std::string> cells;
            do
            {
                cells.push_back(range.address());
            } while (range.next());
            return cells;
        }

        /**
         * @brief 钉住：列标与行号的解码边界，非法文本按 silent 选择返回 -1 或抛错
         */
        TEST(CellAddress, ColumnAndRowDecoding)
        {
            EXPECT_EQ(decodeColumn("A"), 0);
            EXPECT_EQ(decodeColumn("Z"), 25);
            EXPECT_EQ(decodeColumn("AA"), 26);
            EXPECT_EQ(decodeColumn("AZ"), 51);
            EXPECT_EQ(decodeColumn("ZZ"), CellAddress::s_maxColumns - 1);

            EXPECT_EQ(decodeRow("1"), 0);
            EXPECT_EQ(decodeRow("16384"), CellAddress::s_maxRows - 1);
            // 前导零只是写法差异，仍然指向同一行
            EXPECT_EQ(decodeRow("007"), 6);

            // 合法面之外一律拒绝，且不能把 "12a" 当成 12
            EXPECT_NO_THROW(static_cast<void>(decodeColumn("AAA", true)));
            EXPECT_EQ(decodeColumn("AAA", true), -1);
            EXPECT_EQ(decodeColumn("a", true), -1);
            EXPECT_EQ(decodeColumn("", true), -1);
            EXPECT_EQ(decodeRow("0", true), -1);
            EXPECT_EQ(decodeRow("16385", true), -1);
            EXPECT_EQ(decodeRow("12a", true), -1);
            EXPECT_EQ(decodeRow("", true), -1);
            EXPECT_THROW(static_cast<void>(decodeColumn("a")), Base::IndexError);
            EXPECT_THROW(static_cast<void>(decodeRow("0")), Base::IndexError);

            EXPECT_TRUE(validColumn("ZZ"));
            EXPECT_FALSE(validColumn("AAA"));
            // 行数上限是五位数字，超上限即便形状合法也不接受
            EXPECT_EQ(validRow("99999"), -1);
        }

        /**
         * @brief 钉住：列标编码与解码互为逆运算，覆盖全部 702 列
         */
        TEST(CellAddress, ColumnTextRoundTrips)
        {
            for (int column = 0; column < CellAddress::s_maxColumns; ++column)
            {
                const CellAddress address(0, column);
                // 只输出列标时不带 '$'，绝对标记要显式点名
                const std::string text = address.toString(CellAddress::Cell::ShowColumn);
                ASSERT_TRUE(validColumn(text)) << "第 " << column << " 列编码出的列标 '" << text << "' 不合法";
                EXPECT_EQ(decodeColumn(text), column) << text;
            }
        }

        /**
         * @brief 钉住：地址文本里的 '$' 只标记绝对引用，非法文本按 silent 返回无效地址
         */
        TEST(CellAddress, ParsesAbsoluteMarkers)
        {
            const CellAddress relative = stringToAddress("B2");
            EXPECT_EQ(relative.row(), 1);
            EXPECT_EQ(relative.column(), 1);
            EXPECT_FALSE(relative.isAbsoluteRow());
            EXPECT_FALSE(relative.isAbsoluteColumn());

            const CellAddress absolute = stringToAddress("$B$2");
            EXPECT_TRUE(absolute.isAbsoluteRow());
            EXPECT_TRUE(absolute.isAbsoluteColumn());
            EXPECT_EQ(absolute.row(), relative.row());
            EXPECT_EQ(absolute.column(), relative.column());

            // 只锁列或只锁行都算合法写法
            const CellAddress columnOnly = stringToAddress("$B2");
            EXPECT_TRUE(columnOnly.isAbsoluteColumn());
            EXPECT_FALSE(columnOnly.isAbsoluteRow());
            const CellAddress rowOnly = stringToAddress("B$2");
            EXPECT_TRUE(rowOnly.isAbsoluteRow());
            EXPECT_FALSE(rowOnly.isAbsoluteColumn());

            // 小写列标与越界行号都进不了地址，静默模式给无效地址而不是抛
            EXPECT_THROW(static_cast<void>(stringToAddress("b2")), Base::ParserError);
            EXPECT_THROW(static_cast<void>(stringToAddress("A0")), Base::ParserError);
            EXPECT_THROW(static_cast<void>(stringToAddress("AAA1")), Base::ParserError);
            const CellAddress invalid = stringToAddress("b2", true);
            EXPECT_FALSE(invalid.isValid());
            EXPECT_EQ(invalid.row(), -1);
            EXPECT_EQ(invalid.column(), -1);
        }

        /**
         * @brief 钉住：比较只看行列先后，绝对引用标记与无效地址的参与方式
         */
        TEST(CellAddress, OrderingIgnoresAbsoluteMarkers)
        {
            const CellAddress a1 = stringToAddress("A1");
            const CellAddress a2 = stringToAddress("A2");
            const CellAddress b1 = stringToAddress("B1");

            // 行相同时比列，行不同时行优先
            EXPECT_TRUE(a1 < b1);
            EXPECT_TRUE(a1 < a2);
            EXPECT_TRUE(b1 < a2);
            EXPECT_TRUE(a2 > a1);
            EXPECT_TRUE(a1 <= a1);
            EXPECT_TRUE(a1 == stringToAddress("A1"));
            // 等式只看行列：$A$1 与 A1 指同一个格子，绝对标记留给宿主决定要不要偏移
            EXPECT_TRUE(a1 == stringToAddress("$A$1"));
            EXPECT_FALSE(a1 != stringToAddress("$A$1"));

            // 无效地址编码成全 1，排序时落在所有合法地址之后
            const CellAddress invalid;
            EXPECT_FALSE(invalid.isValid());
            EXPECT_TRUE(a1 < invalid);
            EXPECT_TRUE(b1 < invalid);
        }

        /**
         * @brief 钉住：按样式输出地址文本，两字母列标与行号各走一半
         */
        TEST(CellAddress, ToStringHonoursStyle)
        {
            const CellAddress b2 = stringToAddress("B2");
            EXPECT_EQ(b2.toString(), "B2");
            EXPECT_EQ(b2.toString(CellAddress::Cell::ShowColumn), "B");
            EXPECT_EQ(b2.toString(CellAddress::Cell::ShowRow), "2");
            EXPECT_EQ(b2.toString(CellAddress::Cell::ShowRowColumn), "B2");
            EXPECT_EQ(b2.toString(CellAddress::Cell::Absolute), "");

            const CellAddress absolute = stringToAddress("$B$2");
            EXPECT_EQ(absolute.toString(), "$B$2");
            // 只点名行号时，绝对标记只落在行上
            EXPECT_EQ(absolute.toString(CellAddress::Cell::Absolute | CellAddress::Cell::ShowRow), "$2");
            EXPECT_EQ(stringToAddress("$AA$1").toString(), "$AA$1");
            EXPECT_EQ(stringToAddress("$ZZ$16384").toString(), "$ZZ$16384");
        }

        /**
         * @brief 钉住：区间先把当前列走完再换列，行数、列数与单元格个数按首尾跨度算
         */
        TEST(Range, VisitsColumnByColumn)
        {
            const Range range("A1:B3");
            EXPECT_EQ(range.rowCount(), 3);
            EXPECT_EQ(range.columnCount(), 2);
            EXPECT_EQ(range.size(), 6);
            EXPECT_EQ(range.address(), "A1");
            EXPECT_EQ(visitedCells(range), (std::vector<std::string>{"A1", "A2", "A3", "B1", "B2", "B3"}));

            // 单个单元格也是合法区间，只是没有下一步
            const Range single("B2");
            EXPECT_EQ(single.size(), 1);
            EXPECT_EQ(visitedCells(single), (std::vector<std::string>{"B2"}));
        }

        /**
         * @brief 钉住：区间文本与两端文本互相对得上，且都不带绝对引用标记
         */
        TEST(Range, TextsAndEndpoints)
        {
            const Range range("A1:C2");
            EXPECT_EQ(range.rangeText(), "A1:C2");
            EXPECT_EQ(range.fromCellText(), "A1");
            EXPECT_EQ(range.toCellText(), "C2");
            EXPECT_EQ(range.from(), stringToAddress("A1"));
            EXPECT_EQ(range.to(), stringToAddress("C2"));
            EXPECT_EQ(range.current(), stringToAddress("A1"));

            // 用带绝对标记的地址构造，区间只取行列，文本里不出现 '$'
            const Range fromAbsolute(CellAddress("$B$2"), CellAddress("A1"), true);
            EXPECT_EQ(fromAbsolute.rangeText(), "A1:B2");
            EXPECT_EQ(fromAbsolute.size(), 4);
        }

        /**
         * @brief 钉住：整理反向端点后游标回到左上角，遍历与正向区间完全一致
         */
        TEST(Range, NormalizeResetsCursor)
        {
            const std::vector<std::string> forward = visitedCells(Range("A1:B2"));
            ASSERT_EQ(forward, (std::vector<std::string>{"A1", "A2", "B1", "B2"}));

            // 构造时就整理
            const Range normalizedAtConstruction("B2:A1", true);
            EXPECT_EQ(normalizedAtConstruction.rangeText(), "A1:B2");
            EXPECT_EQ(visitedCells(normalizedAtConstruction), forward);

            // 先拿到反向端点、之后再整理：游标必须跟着回到左上角，否则遍历从区间外开始
            Range normalizedLater("B2:A1");
            EXPECT_EQ(normalizedLater.rowCount(), 0);
            normalizedLater.normalize();
            EXPECT_EQ(normalizedLater.rangeText(), "A1:B2");
            EXPECT_EQ(normalizedLater.rowCount(), 2);
            EXPECT_EQ(normalizedLater.columnCount(), 2);
            EXPECT_EQ(normalizedLater.size(), 4);
            EXPECT_EQ(visitedCells(normalizedLater), forward);
        }

        /**
         * @brief 钉住：按行列号构造与区间比较，排序先看起点再看终点
         */
        TEST(Range, ConstructsFromIndicesAndCompares)
        {
            const Range byIndices(0, 0, 2, 1);
            EXPECT_EQ(byIndices.rangeText(), "A1:B3");
            EXPECT_EQ(byIndices.size(), 6);

            // 起点相同的区间按终点排序，便于宿主去重与合并选区
            const Range small("A1:A2");
            const Range large("A1:A3");
            EXPECT_TRUE(small < large);
            EXPECT_FALSE(large < small);
            EXPECT_FALSE(small < small);
            EXPECT_TRUE(Range("A1:B1") < Range("B1:B1"));
        }

    } // namespace
}     // namespace ExpressionEngine::Expression
