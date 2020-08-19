#pragma once

#include <algorithm>
#include <array>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <iterator>
#include <tuple>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "implementation/find.hpp"
#include "implementation/find_generic.hpp"
namespace lazycsv
{

namespace detail
{
template<class T, class chunk_policy>
class fw_iterator
{
    chunk_policy chunker;
    const char* begin_;
    const char* end_;
    const char* dead_end_;

  public:
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::forward_iterator_tag;
    using pointer = T;
    using reference = T;

    fw_iterator(const char* begin, const char* dead_end)
        : chunker()
        , begin_(begin)
        , end_(chunker.chunk(begin, dead_end))
        , dead_end_(dead_end)
    {
    }

    auto operator++(int)
    {
        const auto tmp = *this;
        ++*this;
        return tmp;
    }

    auto& operator++()
    {
        begin_ = end_ + 1;
        if (end_ < dead_end_) // check it is not the last chunk
            end_ = chunker.chunk(begin_, dead_end_);
        return *this;
    }

    bool operator!=(const fw_iterator& rhs) const
    {
        return begin_ != rhs.begin_;
    }

    bool operator==(const fw_iterator& rhs) const
    {
        return begin_ == rhs.begin_;
    }

    auto operator*() const
    {
        return T{ begin_, end_ };
    }

    auto operator-> () const
    {
        return T{ begin_, end_ };
    }
};
} // namespace detail

struct error : std::runtime_error
{
    using std::runtime_error::runtime_error;
};

template<bool flag>
struct has_header
{
    constexpr static bool value = flag;
};

template<char... Trim_chars>
struct trim_chars
{
  private:
    constexpr static auto is_trim_char(char)
    {
        return false;
    }

    template<class... Other_chars>
    constexpr static auto is_trim_char(char c, char trim_char, Other_chars... other_chars)
    {
        return c == trim_char || is_trim_char(c, other_chars...);
    }

  public:
    constexpr static auto trim(const char* begin, const char* end)
    {
        const char* trimed_begin = begin;
        while (trimed_begin != end && is_trim_char(*trimed_begin, Trim_chars...))
            ++trimed_begin;
        const char* trimed_end = end;
        while (trimed_end != begin && is_trim_char(*(trimed_end - 1), Trim_chars...))
            --trimed_end;
        return std::pair{ trimed_begin, trimed_end };
    }
};

class mmap_source
{
    const char* data_;
    size_t size_;
    int fd_;

  public:
    explicit mmap_source(const std::string& path)
    {
        fd_ = open(path.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd_ == -1)
            throw error{ "can't open file, path: " + path + ", error:" + std::string{ std::strerror(errno) } };

        struct stat sb;
        if (fstat(fd_, &sb) == -1)
        {
            close(fd_);
            throw error{ "can't get file size, error:" + std::string{ std::strerror(errno) } };
        }

        data_ = static_cast<const char*>(mmap(nullptr, sb.st_size, PROT_READ, MAP_PRIVATE, fd_, 0U));
        if (data_ == MAP_FAILED)
        {
            close(fd_);
            throw error{ "can't mmap file, error:" + std::string{ std::strerror(errno) } };
        }
        size_ = sb.st_size;
    }

    mmap_source(const mmap_source&) = delete;
    mmap_source& operator=(const mmap_source&) = delete;

    mmap_source(mmap_source&& other) noexcept
        : data_(other.data_)
        , size_(other.size_)
        , fd_(other.fd_)
    {
        other.data_ = nullptr;
    }

    mmap_source& operator=(mmap_source&& other) noexcept
    {
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
        std::swap(fd_, other.fd_);
        return *this;
    }

    const auto* data() const
    {
        return data_;
    }

    auto size() const
    {
        return size_;
    }

    ~mmap_source()
    {
        if (data_)
        {
            munmap(const_cast<char*>(data_), size_);
            close(fd_);
        }
    }
};

template<
    class source = mmap_source,
    class has_header = has_header<true>,
    class delimiter = delimiter<','>,
    class line_ending = line_ending<'\n'>,
    class quotationMark = quotationMark<'"'>,
    class trim_policy = trim_chars<' ', '\t'>>
class parser
{
    source source_;

  public:
    template<typename... Args>
    explicit parser(Args&&... args)
        : source_(std::forward<Args>(args)...)
    {
    }

    auto begin() const
    {
        row_iterator it{ source_.data(), source_.data() + source_.size() };
        if constexpr (has_header::value)
            ++it;
        return it;
    }

    auto end() const
    {
        auto pos = source_.data() + source_.size() + 1;
        if (source_.size() && *(pos - 2) == '\n') // skip the last new line if exists
            pos--;
        return row_iterator{ pos, pos };
    }

    auto header() const
    {
        return *row_iterator{ source_.data(), source_.data() + source_.size() };
    }

    auto index_of(std::string_view column_name) const
    {
        int index = 0;
        for (const auto cell : header())
        {
            if (column_name == cell.trimed())
                return index;
            index++;
        }
        throw error{ "Column does not exist" };
    }

    class cell
    {
        const char* begin_{ nullptr };
        const char* end_{ nullptr };

      public:
        cell() = default;

        cell(const char* begin, const char* end)
            : begin_(begin)
            , end_(end)
        {
        }

        const auto* operator-> () const
        {
            return this;
        }

        auto raw() const
        {
            return std::string_view(begin_, end_ - begin_);
        }

        auto trimed() const
        {
            auto [trimed_begin, trimed_end] = trim_policy::trim(begin_, end_);
            return std::string_view(trimed_begin, trimed_end - trimed_begin);
        }
    };
    using Policy = Quotation_Policy::SIMD<quotationMark, delimiter, line_ending>;
    using cell_iterator = detail::fw_iterator<cell, detail::chunk_cells<Policy>>;

    class row
    {
        const char* begin_{ nullptr };
        const char* end_{ nullptr };

      public:
        row() = default;

        row(const char* begin, const char* end)
            : begin_(begin)
            , end_(end)
        {
        }

        const auto* operator-> () const
        {
            return this;
        }

        auto raw() const
        {
            return std::string_view(begin_, end_ - begin_);
        }

        template<typename... Indexes>
        auto cells(Indexes... indexes) const
        {
            std::array<cell, sizeof...(Indexes)> results;
            std::array<int, sizeof...(Indexes)> desired_indexes{ indexes... };

            auto desired_indexes_it = desired_indexes.begin();
            auto index = 0;
            for (const auto cell : *this)
            {
                if (index++ == *desired_indexes_it)
                {
                    results[desired_indexes_it - desired_indexes.begin()] = cell;
                    if (++desired_indexes_it == desired_indexes.end())
                        return results;
                }
            }
            throw error{ "Row has fewer cells than desired" };
        }

        auto begin() const
        {
            return cell_iterator{ begin_, end_ };
        }

        auto end() const
        {
            return cell_iterator{ end_ + 1, end_ + 1 };
        }
    };

    using row_iterator = detail::fw_iterator<row, detail::chunk_rows<Policy>>;
};
} // namespace lazycsv