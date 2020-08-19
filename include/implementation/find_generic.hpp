#pragma once
#include <algorithm>
#include <array>
#include <bitset>
#include <cassert>
#include <immintrin.h>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <tuple>
#include <type_traits>
#include <wmmintrin.h>
namespace lazycsv
{
template<class T, template<class...> class Template>
struct is_specialization_of : std::false_type
{
};

template<template<class...> class Template, class... Args>
struct is_specialization_of<Template<Args...>, Template> : std::true_type
{
};

template<class T, template<char...> class Template>
struct is_char_specialization_of : std::false_type
{
};

template<template<char...> class Template, char... Args>
struct is_char_specialization_of<Template<Args...>, Template> : std::true_type
{
};

template<typename>
struct Debug;

template<template<class...> class T, class U>
struct isDerivedFrom
{
    static constexpr bool value = decltype(isDerivedFrom::test(U()))::value;

  private:
    template<class... tS>
    static std::true_type test(T<tS...>);
    static std::false_type test(...);
};

template<char character>
struct delimiter
{
    constexpr static char value = character;
};

template<char character>
struct line_ending
{
    constexpr static char value = character;
};
template<char character>
struct quotationMark
{
    constexpr static char value = character;
};

namespace Quotation_Policy
{
template<typename Q, typename T, typename K>
struct settings
{
    static_assert(is_char_specialization_of<Q, lazycsv::quotationMark>::value);
    static_assert(is_char_specialization_of<T, delimiter>::value);
    static_assert(is_char_specialization_of<K, line_ending>::value);
    constexpr static char quotationMark = Q::value;
    constexpr static char delimiter = T::value;
    constexpr static char line_ending = K::value;
};
template<typename... TS>
class None : public settings<TS...>
{
};
template<typename... TS>
class SIMD : public settings<TS...>
{
};
template<typename... TS>
class Generic : public settings<TS...>
{
};

using NoneD = None<quotationMark<'"'>>;
}
namespace detail
{
namespace find_mode
{
/// mode to retrun the first
struct find_delim
{
    static constexpr auto name = "find_delim";
};
struct find_both
{
    static constexpr auto name = "find_both";
};
struct find_lineEnd
{
    static constexpr auto name = "find_lineEnd";
};
}
template<typename Policy>
class chunker
{
    // Debug<setting> d;
    static_assert(isDerivedFrom<Quotation_Policy::settings, Policy>::value);

  private:
    template<class value_type, std::size_t size>
    class CharOffsetArray;

  protected:
    const char* chunkLast{};
    const char* chunkBegin{};
    bool carrybit1 = false;
    bool IsQuotedBlock = false;
    static constexpr size_t SizeOfBuffer = 20;
    /// a Offset array were accumulate(buffer.begin(), buffer[N], chunkBegin) will point to the Nth char containing a delim or new-line.
    CharOffsetArray<unsigned char, SizeOfBuffer> buffer{};

  public:
    template<typename findMode, class U>
    std::tuple<const char*, const char*> find(U, findMode, const char* first, const char* last);

    template<typename findMode, typename T, class... TS>
    std::tuple<T, T> find_I(Quotation_Policy::Generic<TS...>, findMode, T first, T last);

    template<typename findMode, class... TS>
    std::tuple<const char*, const char*> find_I(Quotation_Policy::SIMD<TS...>, findMode, const char* first, const char* last);

  private:
    template<class value_type = unsigned char, std::size_t Size = 0>
    class CharOffsetArray
    {
        /// A value showing that the array does not contain a valiud value
      public:
        static constexpr value_type InvalidBufferValue = std::numeric_limits<value_type>::max();
        std::array<value_type, Size> theArray;
        /// A bitset into the buffer to show if the pointer points to delimiter (false) or endline( True).
        std::bitset<SizeOfBuffer> bitBuffer{};
        CharOffsetArray()
            : theArray()
        {
            theArray.fill(InvalidBufferValue);
        };
        // returns the iterator 1 past the
        inline auto end()
        {
            return std::find(theArray.begin(), theArray.end(), InvalidBufferValue);
        }
        inline auto begin()
        {
            return theArray.begin();
        }
        inline auto size()
        {
            return std::distance(theArray.begin(), end());
        }
        inline bool isDelim(size_t i)
        {
            return !bitBuffer[i];
        }
        inline bool isLineEnd(size_t i)
        {
            return bitBuffer[i];
        }
        inline auto operator[](size_t i)
        {
            return theArray[i];
        }

        inline void fill(value_type val = InvalidBufferValue)
        {
            theArray.fill(val);
        }
        friend chunker<Policy>;
    };
    inline void saveInBuffer(const char* charToSave, bool isLineEnd)
    {
        auto end = buffer.end();
        if (end == buffer.theArray.end())
            return;
        auto init = chunkBegin;
        const char* lastPointer = std::accumulate(buffer.begin(), end, init);
        auto offset = charToSave - lastPointer;
        using c = CharOffsetArray<unsigned char, SizeOfBuffer>;
        *end = offset >= c::InvalidBufferValue ? c::InvalidBufferValue : offset;
        buffer.bitBuffer.set(std::distance(buffer.begin(), end), isLineEnd);
    }
};

template<typename Policy>
template<typename findMode, class U>
std::tuple<const char*, const char*> chunker<Policy>::find(U, findMode, const char* first, const char* last)
{
    assert((last != nullptr) && (first != nullptr));
    if (first == last)
        return std::make_tuple(last, last);
    if ((first >= chunkBegin) & (last <= this->chunkLast))
    {
        const char* sum = chunkBegin;
        const char* firstDelim = last;
        for (size_t i = 0; i < buffer.size(); i++)
        {
            auto value = buffer[i];
            if (value == CharOffsetArray<>::InvalidBufferValue)
                break;
            sum += value;
            if (sum > first)
            {
                if constexpr (std::is_same<findMode, find_mode::find_delim>::value)
                    if (buffer.isDelim(i))
                        return std::make_tuple(sum, last);
                if constexpr (std::is_same<findMode, find_mode::find_lineEnd>::value)
                    if (buffer.isLineEnd(i))
                        return std::make_tuple(last, sum);
                if constexpr (std::is_same<findMode, find_mode::find_both>::value)
                {
                    if ((firstDelim == last) & (buffer.isDelim(i)))
                        firstDelim = sum;
                    if (buffer.isLineEnd(i))
                        return std::make_tuple(firstDelim, sum);
                }
            }
        }
        if constexpr (std::is_same<findMode, find_mode::find_both>::value)
            if (firstDelim != last)
                return std::make_tuple(firstDelim, last);
    }
    this->chunkBegin = first;
    this->chunkLast = last;
    buffer.fill();
    return find_I(U{}, findMode{}, first, last);
}

template<typename Policy>
template<typename findMode, typename T, class... TS>
std::tuple<T, T> chunker<Policy>::find_I(Quotation_Policy::Generic<TS...>, findMode, T first, T last)
{

    if (first >= last) [[unlikely]]
        return std::make_tuple(last, last);

    std::cout << "find generic called with " << last - first << "chars in mode: " << findMode::name << '\n';
    static constexpr char quotation = Policy::quotationMark;
    static constexpr char delim = Policy::delimiter;
    static constexpr char newline = Policy::line_ending;
    // static_assert(chunkSize == 512, "chunksize needs to be 512");
    /// Lambda to set the proper state on exit
    auto Return = [&](const char* firstDelim, const char* FirstEndLIne) {
        auto lastChar = *(std::prev(last));
        if (lastChar == quotation)
            this->carrybit1 = true;
        else
            this->carrybit1 = false;
        auto ret = std::min(firstDelim, FirstEndLIne);
        std::cout << "generic returns \t " << std::string_view(first, ret - first) << "::::\n";
        std::cout << "carry set to \t " << this->carrybit1 << "::::\n";
        std::cout << "Quatoed Block\t " << this->IsQuotedBlock << "::::\n";
        return std::make_tuple(firstDelim, FirstEndLIne);
    };


    auto string = std::string_view{ first, (size_t)std::distance(first, last) };
    T first_deliminter = last;
    auto itterator = string.begin();
    if ((*itterator == quotation) & this->carrybit1)
        // if the carry bit was set we ended in prev iterations of find with a odd sequence of quotations.
        this->IsQuotedBlock = not this->IsQuotedBlock;
    for (; itterator != string.end(); itterator++)
    {
        auto value = *itterator;
        if (value == quotation)
            this->IsQuotedBlock = not this->IsQuotedBlock;
        if (value == delim && this->IsQuotedBlock == false)
            saveInBuffer(itterator, false);
        if constexpr (!std::is_same<findMode, find_mode::find_lineEnd>::value)
            if ((value == delim) & (this->IsQuotedBlock == false))
                if (first_deliminter == last)
                {
                    if constexpr (std::is_same<findMode, find_mode::find_delim>::value)
                        return Return(itterator, last);
                    else
                        first_deliminter = itterator;
                }
                else
                {
                    if constexpr (!std::is_same<findMode, find_mode::find_delim>::value)
                        saveInBuffer(itterator, false);
                }

        if (value == newline && this->IsQuotedBlock == false)
            return Return(first_deliminter, itterator);
    }
    return Return(last, last);
}

template<class Policy = Quotation_Policy::None<>>
struct chunk_rows
{
    static auto chunk(const char* begin, const char* dead_end)
    {
        // /static_assert(std::is_same<  std::tuple_element<1, std::tuple<PackedCharsToSearchFor...>>::type , char >);
        if (const auto* end = static_cast<const char*>(memchr(begin, Policy::line_ending, dead_end - begin)))
            return end;
        return dead_end;
    }
};

template<class InputIt, class T, class U = Quotation_Policy::None<>>
InputIt find(InputIt first, InputIt last, const T& value, U)
{
    // static_assert(!std::is_same<std::remove_reference<T>,char>::value, "A overload for char exists");
    return std::find(first, last, value);
}
template<class InputIt, class U = Quotation_Policy::None<>>
InputIt find(InputIt first, InputIt last, const char value, U)
{
    assert("fallback called ");
    return std::find(first, last, value);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
// [[maybe_unused]]  is not working on older versions off GCC this pragma disables the warning just for this section

template<typename... TS>
struct chunk_rows<Quotation_Policy::SIMD<TS...>>
{
    using Policy = Quotation_Policy::SIMD<TS...>;
    auto chunk(const char* begin, const char* dead_end)
    {

        [[maybe_unused]] auto [_, row_end] = state.find(Policy{}, find_mode::find_lineEnd{}, begin, dead_end);
        return row_end;
        // return dead_end;
    }

  private:
    chunker<Policy> state{};
};

template<class Policy>
struct chunk_cells
{
    static auto chunk(const char* begin, const char* dead_end)
    {
        //  using mode = std::integral_constant<detail::chunker::find_mode,detail::chunker::find_mode::find_both>;
        return std::find(begin, dead_end, Policy::delim.value);
    }
};

template<typename... TS>
struct chunk_cells<Quotation_Policy::SIMD<TS...>>
{
    using Policy = Quotation_Policy::SIMD<TS...>;
    auto chunk(const char* begin, const char* dead_end)
    {
        // using mode = std::integral_constant<detail::chunker::find_mode ,detail::chunker::find_mode::find_both>;
        [[maybe_unused]] auto [cell_end, row_end] = state.find(Policy{}, find_mode::find_delim{}, begin, dead_end);
        return cell_end;
        return dead_end;
    }

  private:
    chunker<Policy> state{};
};

#pragma GCC diagnostic pop
}
} // end namespace detail