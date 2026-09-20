#include <ExpressionEngine/Expression/Range.h>
#include <ExpressionEngine/Base/Exception.h>

#include <algorithm>
#include <charconv>
#include <format>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>


namespace ExpressionEngine::Expression
{

    namespace
    {

        /// 列标最多两个字母（A..ZZ），行号最多五位数字，与地址文本的合法形状一致
        constexpr std::size_t s_maxColumnLetters = 2;
        constexpr std::size_t s_maxRowDigits     = 5;

        /// 字母表长度，列标与列号之间的换算基数
        constexpr int s_alphabetSize = 26;

        /**
         * @brief 把格式已合法的列标文本转成 0 起的列号
         * @param columnText 仅含 A..Z 的非空列标
         * @return 0 起的列号
         */
        int columnTextToNumber(const std::string_view columnText)
        {
            int value = 0;
            for (const char letter: columnText)
            {
                // 列标按 26 进制进位，A 视为 1 而不是 0，否则 A 与 AA 无法区分
                value = value * s_alphabetSize + (letter - 'A' + 1);
            }
            return value - 1;
        }

    } // namespace

    CellAddress::CellAddress(const int row, const int column, const bool absoluteRow, const bool absoluteColumn) :
        m_row(-1), m_column(-1), m_absoluteRow(absoluteRow), m_absoluteColumn(absoluteColumn)
    {
        // 越界坐标一律记为无效：静默截断会让引用悄悄落到别的单元格上
        if (row >= 0 && row < s_maxRows)
        {
            m_row = static_cast<short>(row);
        }
        if (column >= 0 && column < s_maxColumns)
        {
            m_column = static_cast<short>(column);
        }
    }

    CellAddress::CellAddress(const std::string &address) :
        CellAddress(stringToAddress(address))
    {
    }

    int CellAddress::row() const noexcept
    {
        return m_row;
    }

    int CellAddress::column() const noexcept
    {
        return m_column;
    }

    void CellAddress::setRow(const int row, const bool clip)
    {
        if (row < 0 || row >= s_maxRows)
        {
            // 越界时按调用方意愿夹到末行或记为无效，绝不当成 0 行处理
            m_row = clip ? static_cast<short>(s_maxRows - 1) : static_cast<short>(-1);
            return;
        }
        m_row = static_cast<short>(row);
    }

    void CellAddress::setColumn(const int column, const bool clip)
    {
        if (column < 0 || column >= s_maxColumns)
        {
            m_column = clip ? static_cast<short>(s_maxColumns - 1) : static_cast<short>(-1);
            return;
        }
        m_column = static_cast<short>(column);
    }

    bool CellAddress::isValid() const noexcept
    {
        return m_row >= 0 && m_row < s_maxRows && m_column >= 0 && m_column < s_maxColumns;
    }

    bool CellAddress::isAbsoluteRow() const noexcept
    {
        return m_absoluteRow;
    }

    bool CellAddress::isAbsoluteColumn() const noexcept
    {
        return m_absoluteColumn;
    }

    std::string CellAddress::toString(Cell style) const
    {
        const auto  flags = std::to_underlying(style);
        std::string text;

        if ((flags & std::to_underlying(Cell::ShowColumn)) != 0)
        {
            if (m_absoluteColumn && (flags & std::to_underlying(Cell::Absolute)) != 0)
            {
                text += '$';
            }
            if (m_column < s_alphabetSize)
            {
                text += static_cast<char>('A' + m_column);
            } else
            {
                // Z 之后列标变成两个字母，AA 对应 26
                const int offset = m_column - s_alphabetSize;

                text += static_cast<char>('A' + offset / s_alphabetSize);
                text += static_cast<char>('A' + offset % s_alphabetSize);
            }
        }

        if ((flags & std::to_underlying(Cell::ShowRow)) != 0)
        {
            if (m_absoluteRow && (flags & std::to_underlying(Cell::Absolute)) != 0)
            {
                text += '$';
            }
            text += std::format("{}", m_row + 1);
        }

        return text;
    }

    std::strong_ordering CellAddress::operator<=>(const CellAddress &other) const noexcept
    {
        return asInteger() <=> other.asInteger();
    }

    bool CellAddress::operator==(const CellAddress &other) const noexcept
    {
        return asInteger() == other.asInteger();
    }

    bool CellAddress::operator!=(const CellAddress &other) const noexcept
    {
        return asInteger() != other.asInteger();
    }

    unsigned int CellAddress::asInteger() const noexcept
    {
        // 行号在高 16 位、列号在低 16 位；无效地址（-1）编码为全 1，排序时排最后
        const auto rowPart    = static_cast<unsigned int>(static_cast<unsigned short>(m_row));
        const auto columnPart = static_cast<unsigned int>(static_cast<unsigned short>(m_column));
        return (rowPart << 16) | columnPart;
    }

    CellAddress stringToAddress(std::string_view address, bool silent)
    {
        std::size_t position = 0;

        bool absoluteColumn = false;
        if (position < address.size() && address[position] == '$')
        {
            absoluteColumn = true;
            ++position;
        }

        const std::size_t columnBegin = position;
        while (position < address.size() && address[position] >= 'A' && address[position] <= 'Z')
        {
            ++position;
        }
        const std::string_view columnText = address.substr(columnBegin, position - columnBegin);

        bool absoluteRow = false;
        if (position < address.size() && address[position] == '$')
        {
            absoluteRow = true;
            ++position;
        }
        const std::string_view rowText = address.substr(position);

        const int row    = validRow(rowText);
        const int column = validColumn(columnText) ? columnTextToNumber(columnText) : -1;
        if (row < 0 || column < 0)
        {
            if (silent)
            {
                return CellAddress();
            }
            throw Base::ParserError(std::format("'{}' 不是合法的单元格地址，合法写法形如 A1、$B$2（列标 A 到 ZZ，行号 1 到 {}）；"
                                                "请检查是否漏写行号或用了小写列标",
                                                address, CellAddress::s_maxRows));
        }

        return CellAddress(row, column, absoluteRow, absoluteColumn);
    }

    int decodeColumn(std::string_view columnText, bool silent)
    {
        if (validColumn(columnText))
        {
            return columnTextToNumber(columnText);
        }
        if (silent)
        {
            return -1;
        }
        throw Base::IndexError(std::format("列标 '{}' 不是合法列，合法范围是 A 到 ZZ；请改成大写列标，如 A、B、AA", columnText));
    }

    int decodeRow(std::string_view rowText, bool silent)
    {
        const int row = validRow(rowText);
        if (row >= 0)
        {
            return row;
        }
        if (silent)
        {
            return -1;
        }
        throw Base::IndexError(std::format("行号 '{}' 不是合法行，合法范围是 1 到 {}；请改成十进制行号，如 1、2", rowText, CellAddress::s_maxRows));
    }

    bool validColumn(const std::string_view columnText)
    {
        if (columnText.empty() || columnText.size() > s_maxColumnLetters)
        {
            return false;
        }
        for (const char letter: columnText)
        {
            if (letter < 'A' || letter > 'Z')
            {
                return false;
            }
        }
        // 形状合法还要看是否越过 ZZ，否则解码出来的列号会越界
        return columnTextToNumber(columnText) < CellAddress::s_maxColumns;
    }

    int validRow(const std::string_view rowText)
    {
        if (rowText.empty() || rowText.size() > s_maxRowDigits)
        {
            return -1;
        }

        int        value  = 0;
        const auto parsed = std::from_chars(rowText.data(), rowText.data() + rowText.size(), value);
        // 必须整段都是数字：'12a' 这类文本要当作无效，而不是解析出 12
        if (parsed.ec != std::errc() || parsed.ptr != rowText.data() + rowText.size())
        {
            return -1;
        }
        if (value < 1 || value > CellAddress::s_maxRows)
        {
            return -1;
        }
        return value - 1;
    }

    Range::Range(const std::string_view rangeText, const bool normalize)
    {
        const std::size_t      separator = rangeText.find(':');
        const std::string_view beginText = separator == std::string_view::npos ? rangeText : rangeText.substr(0, separator);
        const std::string_view endText   = separator == std::string_view::npos ? rangeText : rangeText.substr(separator + 1);

        const CellAddress begin = stringToAddress(beginText);
        const CellAddress end   = stringToAddress(endText);

        m_rowBegin    = begin.row();
        m_columnBegin = begin.column();
        m_rowEnd      = end.row();
        m_columnEnd   = end.column();

        if (normalize)
        {
            this->normalize();
        }
        m_rowCurrent    = m_rowBegin;
        m_columnCurrent = m_columnBegin;
    }

    Range::Range(const int rowBegin, const int columnBegin, const int rowEnd, const int columnEnd, const bool normalize) :
        m_rowCurrent(rowBegin), m_columnCurrent(columnBegin), m_rowBegin(rowBegin), m_columnBegin(columnBegin), m_rowEnd(rowEnd), m_columnEnd(columnEnd)
    {
        if (normalize)
        {
            this->normalize();
        }
        // 整理后起点可能改变，游标必须重新对齐到左上角
        m_rowCurrent    = m_rowBegin;
        m_columnCurrent = m_columnBegin;
    }

    Range::Range(const CellAddress &from, const CellAddress &to, const bool normalize) :
        m_rowCurrent(from.row()), m_columnCurrent(from.column()), m_rowBegin(from.row()), m_columnBegin(from.column()), m_rowEnd(to.row()), m_columnEnd(to.column())
    {
        if (normalize)
        {
            this->normalize();
        }
        m_rowCurrent    = m_rowBegin;
        m_columnCurrent = m_columnBegin;
    }

    void Range::normalize()
    {
        if (m_rowBegin > m_rowEnd)
        {
            std::swap(m_rowBegin, m_rowEnd);
        }
        if (m_columnBegin > m_columnEnd)
        {
            std::swap(m_columnBegin, m_columnEnd);
        }
        // 端点换过之后旧游标可能落在区间之外，遍历会漏掉前面一整片
        m_rowCurrent    = m_rowBegin;
        m_columnCurrent = m_columnBegin;
    }

    bool Range::next() const
    {
        // 先把当前列自上而下走完，再换到下一列（A1, A2, …, B1, B2, …）
        if (m_rowCurrent < m_rowEnd)
        {
            ++m_rowCurrent;
            return true;
        }
        if (m_columnCurrent >= m_columnEnd)
        {
            return false;
        }
        m_rowCurrent = m_rowBegin;
        ++m_columnCurrent;
        return true;
    }

    int Range::row() const noexcept
    {
        return m_rowCurrent;
    }

    int Range::column() const noexcept
    {
        return m_columnCurrent;
    }

    int Range::rowCount() const noexcept
    {
        return m_rowEnd - m_rowBegin + 1;
    }

    int Range::columnCount() const noexcept
    {
        return m_columnEnd - m_columnBegin + 1;
    }

    CellAddress Range::from() const
    {
        return CellAddress(m_rowBegin, m_columnBegin);
    }

    CellAddress Range::to() const
    {
        return CellAddress(m_rowEnd, m_columnEnd);
    }

    CellAddress Range::current() const
    {
        return CellAddress(m_rowCurrent, m_columnCurrent);
    }

    std::string Range::fromCellText() const
    {
        return from().toString();
    }

    std::string Range::toCellText() const
    {
        return to().toString();
    }

    std::string Range::address() const
    {
        return current().toString();
    }

    std::string Range::rangeText() const
    {
        return fromCellText() + ":" + toCellText();
    }

    int Range::size() const noexcept
    {
        return rowCount() * columnCount();
    }

    bool Range::operator<(const Range &other) const
    {
        if (from() < other.from())
        {
            return true;
        }
        if (from() > other.from())
        {
            return false;
        }
        return to() < other.to();
    }

} // namespace ExpressionEngine::Expression
