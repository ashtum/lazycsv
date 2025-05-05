#pragma once

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

#if defined(_WIN32)
#include <windows.h>
#else // defined(_WIN32)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace lazycsv
{
namespace detail
{
struct chunk_rows
{
    static const char* chunk(const char* begin, const char* dead_end)
    {
        if (const char* end = static_cast<const char*>(memchr(begin, '\n', dead_end - begin)))
            return end;
        return dead_end;
    }
};

template<char delimiter, char quote_char>
struct chunk_cells
{
    static const char* chunk(const char* begin, const char* dead_end)
    {
        bool quote_opened = false;
        const char* quote_location = {};

        for (const char* i = begin; i < dead_end; i++)
        {
            if (*i == delimiter && !quote_opened)
                return i;

            if (*i == quote_char)
            {
                if (!quote_opened)
                {
                    quote_opened = true;
                    quote_location = i;
                }
                else
                {
                    quote_opened = false;
                    bool escaped = (quote_location == i - 1);
                    if (escaped)
                    {
                        quote_location = {};
                    }
                    else
                    {
                        quote_location = i;
                    }
                }
            }
        }
        return dead_end;
    }
};

template<class T, class chunk_policy>
class fw_iterator
{
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
        : begin_(begin)
        , end_(chunk_policy::chunk(begin, dead_end))
        , dead_end_(dead_end)
    {
    }

    fw_iterator operator++(int)
    {
        const auto tmp = *this;
        ++*this;
        return tmp;
    }

    fw_iterator& operator++()
    {
        begin_ = end_ + 1;
        if (end_ != dead_end_) // check it is not the last chunk
            end_ = chunk_policy::chunk(begin_, dead_end_);
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

    T operator*() const
    {
        return { begin_, end_ };
    }

    T operator->() const
    {
        return { begin_, end_ };
    }
};
} // namespace detail

struct error : std::runtime_error
{
    using std::runtime_error::runtime_error;
};

template<char character>
struct delimiter
{
    constexpr static char value = character;
};

template<char character>
struct quote_char
{
    constexpr static char value = character;
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
    constexpr static bool is_trim_char(char)
    {
        return false;
    }

    template<class... Other_chars>
    constexpr static bool is_trim_char(char c, char trim_char, Other_chars... other_chars)
    {
        return c == trim_char || is_trim_char(c, other_chars...);
    }

  public:
    constexpr static std::pair<const char*, const char*> trim(const char* begin, const char* end)
    {
        const char* trimmed_begin = begin;
        while (trimmed_begin != end && is_trim_char(*trimmed_begin, Trim_chars...))
            ++trimmed_begin;
        const char* trimmed_end = end;
        while (trimmed_end != trimmed_begin && is_trim_char(*(trimmed_end - 1), Trim_chars...))
            --trimmed_end;
        return { trimmed_begin, trimmed_end };
    }
};

class mmap_source
{
    const char* data_{ nullptr };
    std::size_t size_;
#if defined(_WIN32)
    HANDLE fd_{ INVALID_HANDLE_VALUE };
    HANDLE map_{ NULL };
#else // defined(_WIN32)
    int fd_;
#endif

  public:
    explicit mmap_source(const std::string& path)
    {
#if defined(_WIN32)
        fd_ = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

        if (fd_ == INVALID_HANDLE_VALUE)
            throw error{ "can't open file, path: " + path + ", error code:" + std::to_string(GetLastError()) };

        LARGE_INTEGER file_size;
        if (!GetFileSizeEx(fd_, &file_size))
        {
            CloseHandle(fd_);
            throw error{ "can't get file size, error code:" + std::to_string(GetLastError()) };
        }

        size_ = static_cast<std::size_t>(file_size.QuadPart);

        if (size_ > 0)
        {
            map_ = CreateFileMappingA(fd_, NULL, PAGE_READONLY, 0, 0, NULL);

            if (map_ == NULL)
            {
                CloseHandle(fd_);
                throw error{ "can't create file mapping, error code:" + std::to_string(GetLastError()) };
            }

            data_ = static_cast<const char*>(MapViewOfFile(map_, FILE_MAP_READ, 0, 0, 0));

            if (data_ == nullptr)
            {
                CloseHandle(map_);
                CloseHandle(fd_);
                throw error{ "can't map view of file, error code:" + std::to_string(GetLastError()) };
            }
        }
        else
        {
            CloseHandle(fd_);
        }
#else // defined(_WIN32)
        fd_ = open(path.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd_ == -1)
            throw error{ "can't open file, path: " + path + ", error:" + std::string{ std::strerror(errno) } };

        struct stat sb = {};
        if (fstat(fd_, &sb) == -1)
        {
            close(fd_);
            throw error{ "can't get file size, error:" + std::string{ std::strerror(errno) } };
        }

        size_ = sb.st_size;

        if (size_ > 0)
        {
            data_ = static_cast<const char*>(mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd_, 0U));
            if (data_ == MAP_FAILED)
            {
                close(fd_);
                throw error{ "can't mmap file, error:" + std::string{ std::strerror(errno) } };
            }
        }
        else
        {
            close(fd_);
        }
#endif
    }

    mmap_source(const mmap_source&) = delete;
    mmap_source& operator=(const mmap_source&) = delete;

    mmap_source(mmap_source&& other) noexcept
        : data_(other.data_)
        , size_(other.size_)
        , fd_(other.fd_)
#if defined(_WIN32)
        , map_(other.map_)
#endif // defined(_WIN32)
    {
        other.data_ = nullptr;
    }

    mmap_source& operator=(mmap_source&& other) noexcept
    {
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
        std::swap(fd_, other.fd_);
#if defined(_WIN32)
        std::swap(map_, other.map_);
#endif // defined(_WIN32)
        return *this;
    }

    const char* data() const
    {
        return data_;
    }

    std::size_t size() const
    {
        return size_;
    }

    ~mmap_source()
    {
        if (data_)
        {
#if defined(_WIN32)
            UnmapViewOfFile(const_cast<char*>(data_));
            CloseHandle(map_);
            CloseHandle(fd_);
#else // defined(_WIN32)
            munmap(const_cast<char*>(data_), size_);
            close(fd_);
#endif
        }
    }
};

template<
    class source = mmap_source,
    class has_header = has_header<true>,
    class delimiter = delimiter<','>,
    class quote_char = quote_char<'"'>,
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

    class cell
    {
        const char* begin_{ nullptr };
        const char* end_{ nullptr };

      public:
        cell() = default;

        cell(const char* begin, const char* end)
            : begin_(escape_leading_quote(begin, end))
            , end_(escape_trailing_quote(begin, end))
        {
        }

        const cell* operator->() const
        {
            return this;
        }

        std::string_view raw() const
        {
            return { begin_, static_cast<std::size_t>(end_ - begin_) };
        }

        std::string_view trimmed() const
        {
            auto [trimmed_begin, trimmed_end] = trim_policy::trim(begin_, end_);
            return { trimmed_begin, static_cast<std::size_t>(trimmed_end - trimmed_begin) };
        }

        std::string unescaped() const
        {
            auto [trimmed_begin, trimmed_end] = trim_policy::trim(begin_, end_);
            std::string result;
            result.reserve(trimmed_end - trimmed_begin);
            for (const char* i = trimmed_begin; i < trimmed_end; i++)
            {
                if (*i == quote_char::value && i + 1 < trimmed_end && *(i + 1) == quote_char::value)
                    i++;
                result.push_back(*i);
            }
            return result;
        }

      private:
        static const char* escape_leading_quote(const char* begin, const char* end)
        {
            if (end - begin >= 2 && *begin == quote_char::value && *(end - 1) == quote_char::value)
                return begin + 1;

            return begin;
        }

        static const char* escape_trailing_quote(const char* begin, const char* end)
        {
            if (end - begin >= 2 && *begin == quote_char::value && *(end - 1) == quote_char::value)
                return end - 1;

            return end;
        }
    };

    using cell_iterator = detail::fw_iterator<cell, detail::chunk_cells<delimiter::value, quote_char::value>>;

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
            // Handle CRLF line endings by adjusting end_
            if (end_ > begin_ && *(end_ - 1) == '\r')
                --end_;
        }

        const row* operator->() const
        {
            return this;
        }

        std::string_view raw() const
        {
            return { begin_, end_ - begin_ };
        }

        template<typename... Indexes>
        std::array<cell, sizeof...(Indexes)> cells(Indexes... indexes) const
        {
            std::array<cell, sizeof...(Indexes)> results;
            std::array<int, sizeof...(Indexes)> desired_indexes{ indexes... };

            auto desired_indexes_it = desired_indexes.begin();
            int index = 0;
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

        cell_iterator begin() const
        {
            return { begin_, end_ };
        }

        cell_iterator end() const
        {
            return { end_ + 1, end_ + 1 };
        }
    };

    using row_iterator = detail::fw_iterator<row, detail::chunk_rows>;

    row_iterator begin() const
    {
        row_iterator it{ source_.data(), source_.data() + source_.size() };
        if constexpr (has_header::value)
            ++it;
        return it;
    }

    row_iterator end() const
    {
        const char* pos = source_.data() + source_.size() + 1;
        if (source_.size() && *(pos - 2) == '\n') // skip the last new line if exists
            pos--;
        return row_iterator{ pos, pos };
    }

    row header() const
    {
        return *row_iterator{ source_.data(), source_.data() + source_.size() };
    }

    int index_of(std::string_view column_name) const
    {
        int index = 0;
        for (const auto cell : header())
        {
            if (column_name == cell.trimmed())
                return index;
            index++;
        }
        throw error{ "Column does not exist" };
    }
};
} // namespace lazycsv
