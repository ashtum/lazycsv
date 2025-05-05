[![CI](https://github.com/ashtum/lazycsv/actions/workflows/ci.yml/badge.svg)](https://github.com/ashtum/lazycsv/actions/workflows/ci.yml)

![lazycsv](img/logo.png)

## What's the lazycsv?

**lazycsv** is a c++17, single-header library for reading and parsing csv files.  
It's fast and lightweight and does not allocate any memory in the constructor or while parsing. It parses each row and cell just on demand on each iteration, that's why it's called lazy.

### Quick usage

The latest version of the single header can be downloaded from [`include/lazycsv.hpp`](include/lazycsv.hpp).

```c++
#include <lazycsv.hpp>

int main()
{
    lazycsv::parser parser{ "contacts.csv" };
    for (const auto row : parser)
    {
        const auto [id, name, phone] = row.cells(0, 1, 4); // indexes must be in ascending order
    }
}
```

### Performance note

Parser doesn't keep state of already parsed rows and cells, iterating through them always associated with parsing cost. This is true with `cells()` member function too, geting all needed cells by a single call is recommended.  
If it's necessary to return to the already parsed rows and cells, they can be stored in a container and used later without being parsed again (they are view objects and efficient to copy).

### Features

Returned `std::string_view` by `raw()` and `trimmed()` member functions are valid as long as the parser object is alive:

```c++
std::vector<std::string_view> cities;
for (const auto row : parser)
{
    const auto [city, state] = row.cells(0, 1);
    cities.push_back(city.trimmed());
}
```

Iterate through rows and cells:

```c++
for (const auto row : parser)
{
    for (const auto cell : row)
    {
    }
}
```

Get header row and iterate through its cells:

```c++
auto header = parser.header();
for (const auto cell : header)
{
}
```

Find column index by its name:

```c++
auto city_index = parser.index_of("city");
```

`row` and `cell` are view objects on actual data in the parser object, they can be stored and used as long as the parser object is alive:

```c++
std::vector<lazycsv::parser<>::row> desired_rows;
std::vector<lazycsv::parser<>::cell> desired_cells;
for (const auto row : parser)
{
    const auto [city] = row.cells(6);
    desired_cells.push_back(city);

    if (city.trimmed() == "Kashan")
        desired_rows.push_back(row);
}
static_assert(sizeof(lazycsv::parser<>::row) == 2 * sizeof(void*));  // i'm lightweight
static_assert(sizeof(lazycsv::parser<>::cell) == 2 * sizeof(void*)); // i'm lightweight too
```

Parser is customizable with the template parameters:

```c++
lazycsv::parser<
    lazycsv::mmap_source,           /* source type of csv data */
    lazycsv::has_header<true>,      /* first row is header or not */
    lazycsv::delimiter<','>,        /* column delimiter */
    lazycsv::quote_char<'"'>,       /* quote character */
    lazycsv::trim_chars<' ', '\t'>> /* trim characters of cells */
    my_parser{ "data.csv" };
```

By default parser uses `lazycsv::mmap_source` as its source of data, but it's possible to be used with any other types of contiguous containers:

```c++
std::string csv_data{ "name,lastname,age\nPeter,Griffin,45\nchris,Griffin,14\n" };

lazycsv::parser<std::string_view> parser_a{ csv_data };
lazycsv::parser<std::string> parser_b{ csv_data };
```

## Acknowledgments

- [Wang XiHua](https://github.com/kabxx) for adding Windows support and cross-platform line ending.
