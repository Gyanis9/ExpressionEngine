/**
 * @file FirstByteDispatch.h
 * @brief 按首字节分组的关键字查表
 * @author Gyanis
 * @date 2026-09-19
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later
 */

#pragma once

#include <array>
#include <cstdint>
#include <limits>

namespace ExpressionEngine::Base
{

    /**
     * @brief 首字节相同的一组候选在分组数组里占据的区间
     * @details 组内保持候选总表里的先后顺序，因此等长匹配时靠前者胜出，与逐条全表扫描的语义一致。
     */
    struct FirstByteBucket
    {
        std::uint16_t begin{0}; ///< 区间在分组数组里的起始下标
        std::uint16_t count{0}; ///< 区间里的候选条数
    };

    /**
     * @brief 首字节分派表
     * @details 256 项索引加一张按首字节分好组的候选指针数组。整表在编译期建好，
     *          无运行时初始化、无堆分配；查找时先按首字节把候选缩到一组，再在组内比较。
     * @tparam Entry 候选类型
     * @tparam Count 候选条数
     */
    template<typename Entry, std::size_t Count>
    struct FirstByteDispatch
    {
        std::array<const Entry *, Count> entries{}; ///< 按首字节分组的候选指针
        std::array<FirstByteBucket, 256> buckets{}; ///< 每个首字节对应的候选区间
    };

    /**
     * @brief 编译期按首字节做计数排序，把候选指针按组铺进分派表
     * @tparam Entry 候选类型
     * @tparam Count 候选条数
     * @tparam FirstByteProjection 取首字节的投影，签名等价于 std::size_t(const Entry&)，返回值须落在 0-255
     * @param entries 候选总表
     * @param firstByteOf 取候选首字节的投影
     * @return 建好的分派表，可直接作为常量初始化
     */
    template<typename Entry, std::size_t Count, typename FirstByteProjection>
    constexpr FirstByteDispatch<Entry, Count> buildFirstByteDispatch(const std::array<Entry, Count> &entries, FirstByteProjection firstByteOf)
    {
        // 分组下标用 uint16 存：条数越界时必须报编译错，不能静默截断
        static_assert(Count <= std::numeric_limits<std::uint16_t>::max(), "候选条数超出分派表下标类型可表示的范围");

        FirstByteDispatch<Entry, Count> dispatch;
        std::array<std::size_t, 256>    counts{};
        for (const Entry &entry: entries)
        {
            ++counts[firstByteOf(entry)];
        }

        // 前缀和给出每组区间；游标随后从各分组起点往后推进，于是组内保持总表顺序
        std::size_t begin = 0;
        for (std::size_t byte = 0; byte < counts.size(); ++byte)
        {
            dispatch.buckets[byte] = {static_cast<std::uint16_t>(begin), static_cast<std::uint16_t>(counts[byte])};
            begin                  += counts[byte];
        }

        std::array<std::size_t, 256> cursors{};
        for (std::size_t byte = 0; byte < cursors.size(); ++byte)
        {
            cursors[byte] = dispatch.buckets[byte].begin;
        }
        for (const Entry &entry: entries)
        {
            dispatch.entries[cursors[firstByteOf(entry)]++] = &entry;
        }

        return dispatch;
    }

} // namespace ExpressionEngine::Base
