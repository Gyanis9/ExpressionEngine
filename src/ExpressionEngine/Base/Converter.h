/**
 * @file Converter.h
 * @brief 向量与旋转类型之间的转换辅助（VectorTraits 与 convertTo）
 * @author Gyanis
 * @date 2026-09-19
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later
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
     * @tparam VectorType 被转换的类型
     */
    template<class VectorType>
    struct VectorTraits
    {
    };

    /**
     * @brief Vector3f 的取值特征
     */
    template<>
    struct VectorTraits<Vector3f>
    {
        using VectorType   = Vector3f; ///< 被转换的类型
        using FloatingType = float;    ///< 分量类型

        /**
         * @brief 绑定被转换的向量
         * @param vector 源向量，本对象只持引用，源必须存活到 get() 之后
         */
        explicit VectorTraits(const VectorType &vector) :
            m_vector(vector)
        {
        }

        /**
         * @brief 取三分量组成的元组
         * @return (x, y, z)
         */
        [[nodiscard]] std::tuple<FloatingType, FloatingType, FloatingType> get() const
        {
            return std::make_tuple(m_vector.x, m_vector.y, m_vector.z);
        }

    private:
        const VectorType &m_vector; ///< 源向量的引用，不持所有权
    };

    /**
     * @brief Vector3d 的取值特征
     */
    template<>
    struct VectorTraits<Vector3d>
    {
        using VectorType   = Vector3d; ///< 被转换的类型
        using FloatingType = double;   ///< 分量类型

        /**
         * @brief 绑定被转换的向量
         * @param vector 源向量，本对象只持引用，源必须存活到 get() 之后
         */
        explicit VectorTraits(const VectorType &vector) :
            m_vector(vector)
        {
        }

        /**
         * @brief 取三分量组成的元组
         * @return (x, y, z)
         */
        [[nodiscard]] std::tuple<FloatingType, FloatingType, FloatingType> get() const
        {
            return std::make_tuple(m_vector.x, m_vector.y, m_vector.z);
        }

    private:
        const VectorType &m_vector; ///< 源向量的引用，不持所有权
    };

    /**
     * @brief Rotation 的取值特征
     */
    template<>
    struct VectorTraits<Rotation>
    {
        using VectorType   = Rotation; ///< 被转换的类型
        using FloatingType = double;   ///< 分量类型

        /**
         * @brief 绑定被转换的旋转
         * @param rotation 源旋转，本对象只持引用，源必须存活到 get() 之后
         */
        explicit VectorTraits(const VectorType &rotation) :
            m_rotation(rotation)
        {
        }

        /**
         * @brief 取四元数四分量组成的元组
         * @return (x, y, z, w) 顺序的四个分量
         */
        [[nodiscard]] std::tuple<FloatingType, FloatingType, FloatingType, FloatingType> get() const
        {
            FloatingType first{};
            FloatingType second{};
            FloatingType third{};
            FloatingType fourth{};
            m_rotation.getValue(first, second, third, fourth);
            return std::make_tuple(first, second, third, fourth);
        }

    private:
        const VectorType &m_rotation; ///< 源旋转的引用，不持所有权
    };

    /**
     * @brief 用三分量元组构造目标类型
     * @tparam Vector 目标类型（Vector3f 或 Vector3d）
     * @tparam FloatingType 元组元素类型
     * @param floats 三个分量
     * @return 构造好的目标对象
     */
    template<class Vector, typename FloatingType>
    Vector makeVector(const std::tuple<FloatingType, FloatingType, FloatingType> &&floats)
    {
        using TraitsType      = VectorTraits<Vector>;
        using FloatTraitsType = TraitsType::FloatingType;
        return Vector(FloatTraitsType(std::get<0>(floats)), FloatTraitsType(std::get<1>(floats)), FloatTraitsType(std::get<2>(floats)));
    }

    /**
     * @brief 用四分量元组构造目标类型
     * @tparam Vector 目标类型（Rotation）
     * @tparam FloatingType 元组元素类型
     * @param floats 四个分量
     * @return 构造好的目标对象
     */
    template<class Vector, typename FloatingType>
    Vector makeVector(const std::tuple<FloatingType, FloatingType, FloatingType, FloatingType> &&floats)
    {
        using TraitsType      = VectorTraits<Vector>;
        using FloatTraitsType = TraitsType::FloatingType;
        return Vector(FloatTraitsType(std::get<0>(floats)), FloatTraitsType(std::get<1>(floats)), FloatTraitsType(std::get<2>(floats)), FloatTraitsType(std::get<3>(floats)));
    }

    /**
     * @brief 在向量/旋转类型之间转换
     * @tparam Vector1 目标类型
     * @tparam Vector2 源类型
     * @param vector 源对象（Vector3f、Vector3d 或 Rotation）
     * @return 转换后的目标对象
     */
    template<class Vector1, class Vector2>
    Vector1 convertTo(const Vector2 &vector)
    {
        using TraitsType   = VectorTraits<Vector2>;
        using FloatingType = TraitsType::FloatingType;
        // traits 负责按源类型的存储形式取出分量，makeVector 再按目标类型重建
        TraitsType traits(vector);
        auto       tuple = traits.get();
        return makeVector<Vector1, FloatingType>(std::move(tuple));
    }
} // namespace ExpressionEngine::Base
