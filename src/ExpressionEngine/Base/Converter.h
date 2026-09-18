/**
 * @file Converter.h
 * @brief 向量与旋转类型之间的转换辅助（vec_traits 与 convertTo）
 * @author Gyanis
 * @date 2026-09-19
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later，派生自 FreeCAD
 */

#pragma once

#include <tuple>
#include <utility>

#include <ExpressionEngine/Base/Rotation.h>
#include <ExpressionEngine/Base/Vector3D.h>

namespace ExpressionEngine::Base
{
    /**
     * @brief 转换源类型的取值特征模板
     * @details 每个可转换类型都要特化本模板：把自身分量打包成元组，交给 convertTo 重建目标类型。
     * @tparam vecT 被转换的类型
     */
    template<class vecT>
    struct vec_traits
    {
    };

    /**
     * @brief Vector3f 的取值特征
     */
    template<>
    struct vec_traits<Vector3f>
    {
        using vec_type   = Vector3f; ///< 被转换的类型
        using float_type = float;    ///< 分量类型

        /**
         * @brief 绑定被转换的向量
         * @param vector 源向量，本对象只持引用，源必须存活到 get() 之后
         */
        explicit vec_traits(const vec_type &vector) : m_vector(vector)
        {
        }

        /**
         * @brief 取三分量组成的元组
         * @return (x, y, z)
         */
        [[nodiscard]] std::tuple<float_type, float_type, float_type> get() const
        {
            return std::make_tuple(m_vector.x, m_vector.y, m_vector.z);
        }

    private:
        const vec_type &m_vector; ///< 源向量的引用，不持所有权
    };

    /**
     * @brief Vector3d 的取值特征
     */
    template<>
    struct vec_traits<Vector3d>
    {
        using vec_type   = Vector3d; ///< 被转换的类型
        using float_type = double;   ///< 分量类型

        /**
         * @brief 绑定被转换的向量
         * @param vector 源向量，本对象只持引用，源必须存活到 get() 之后
         */
        explicit vec_traits(const vec_type &vector) : m_vector(vector)
        {
        }

        /**
         * @brief 取三分量组成的元组
         * @return (x, y, z)
         */
        [[nodiscard]] std::tuple<float_type, float_type, float_type> get() const
        {
            return std::make_tuple(m_vector.x, m_vector.y, m_vector.z);
        }

    private:
        const vec_type &m_vector; ///< 源向量的引用，不持所有权
    };

    /**
     * @brief Rotation 的取值特征
     */
    template<>
    struct vec_traits<Rotation>
    {
        using vec_type   = Rotation; ///< 被转换的类型
        using float_type = double;   ///< 分量类型

        /**
         * @brief 绑定被转换的旋转
         * @param rotation 源旋转，本对象只持引用，源必须存活到 get() 之后
         */
        explicit vec_traits(const vec_type &rotation) : m_rotation(rotation)
        {
        }

        /**
         * @brief 取四元数四分量组成的元组
         * @return (x, y, z, w) 顺序的四个分量
         */
        [[nodiscard]] std::tuple<float_type, float_type, float_type, float_type> get() const
        {
            float_type first{};
            float_type second{};
            float_type third{};
            float_type fourth{};
            m_rotation.getValue(first, second, third, fourth);
            return std::make_tuple(first, second, third, fourth);
        }

    private:
        const vec_type &m_rotation; ///< 源旋转的引用，不持所有权
    };

    /**
     * @brief 用三分量元组构造目标类型
     * @tparam Vec 目标类型（Vector3f 或 Vector3d）
     * @tparam float_type 元组元素类型
     * @param floats 三个分量
     * @return 构造好的目标对象
     */
    template<class Vec, typename float_type>
    Vec make_vec(const std::tuple<float_type, float_type, float_type> &&floats)
    {
        using traits_type       = vec_traits<Vec>;
        using float_traits_type = typename traits_type::float_type;
        return Vec(float_traits_type(std::get<0>(floats)), float_traits_type(std::get<1>(floats)), float_traits_type(std::get<2>(floats)));
    }

    /**
     * @brief 用四分量元组构造目标类型
     * @tparam Vec 目标类型（Rotation）
     * @tparam float_type 元组元素类型
     * @param floats 四个分量
     * @return 构造好的目标对象
     */
    template<class Vec, typename float_type>
    Vec make_vec(const std::tuple<float_type, float_type, float_type, float_type> &&floats)
    {
        using traits_type       = vec_traits<Vec>;
        using float_traits_type = typename traits_type::float_type;
        return Vec(float_traits_type(std::get<0>(floats)), float_traits_type(std::get<1>(floats)), float_traits_type(std::get<2>(floats)), float_traits_type(std::get<3>(floats)));
    }

    /**
     * @brief 在向量/旋转类型之间转换
     * @tparam Vec1 目标类型
     * @tparam Vec2 源类型
     * @param vector 源对象（Vector3f、Vector3d 或 Rotation）
     * @return 转换后的目标对象
     */
    template<class Vec1, class Vec2>
    Vec1 convertTo(const Vec2 &vector)
    {
        using traits_type = vec_traits<Vec2>;
        using float_type  = typename traits_type::float_type;
        // traits 负责按源类型的存储形式取出分量，make_vec 再按目标类型重建
        traits_type traits(vector);
        auto        tuple = traits.get();
        return make_vec<Vec1, float_type>(std::move(tuple));
    }
} // namespace ExpressionEngine::Base
