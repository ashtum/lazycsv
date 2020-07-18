#include <lazycsv.hpp>
#include <string_view>
#include <iostream>
#include <string>
#include <cstring>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

template<class T>
void check_rows(const T& parser, const std::vector<std::vector<std::string>>& expected_rows)
{
    auto row_index = 0;
    for (const auto row : parser)
    {
        auto expected_cells = expected_rows.at(row_index++);
        auto cell_index = 0;
        for (const auto cell : row)
        {
            REQUIRE_EQ(expected_cells.at(cell_index++), cell.trimed());
        }
        REQUIRE_EQ(cell_index, expected_cells.size());
    }
    REQUIRE_EQ(row_index, expected_rows.size());
}

TEST_CASE("mmap_source zero_length")
{
    REQUIRE_THROWS(lazycsv::mmap_source{ "inputs/zero_length.csv" });
}

TEST_CASE("mmap_source non_existent")
{
    REQUIRE_THROWS(lazycsv::mmap_source{ "inputs/non_existent.csv" });
}

TEST_CASE("mmap_source basic.csv")
{
    lazycsv::mmap_source source{ "inputs/basic.csv" };
    REQUIRE_EQ(source.size(), 47);
}

TEST_CASE("parse basic.csv with mmap_source and use cells function")
{
    lazycsv::parser parser{ "inputs/basic.csv" };

    std::vector<std::vector<std::string>> expected_rows{
        { "A0", "B0", "C0", "D0" }, { "A1", "B1", "C1", "D1" }, { "A2", "B2", "C2", "D2" }, { "A3", "B3", "C3", "D3" }
    };

    const auto [a, b, c, d] = parser.header().cells(0, 1, 2, 3);
    auto header_cells = expected_rows.at(0);
    REQUIRE_EQ(header_cells[0], a.raw());
    REQUIRE_EQ(header_cells[1], b.raw());
    REQUIRE_EQ(header_cells[2], c.raw());
    REQUIRE_EQ(header_cells[3], d.raw());

    auto row_index = 1;
    for (const auto row : parser)
    {
        auto expected_cells = expected_rows.at(row_index++);
        const auto [a, b, c, d] = row.cells(0, 1, 2, 3);
        REQUIRE_EQ(expected_cells[0], a.raw());
        REQUIRE_EQ(expected_cells[1], b.raw());
        REQUIRE_EQ(expected_cells[2], c.raw());
        REQUIRE_EQ(expected_cells[3], d.raw());
    }
    REQUIRE_EQ(row_index, expected_rows.size());
}

TEST_CASE("parse empty_cell")
{
    lazycsv::parser<std::string, lazycsv::has_header<false>> parser{ ",B0,C0,D0\nA1,,,D1\nA2,B2,C2,\n,,,\n" };
    check_rows(parser, { { "", "B0", "C0", "D0" }, { "A1", "", "", "D1" }, { "A2", "B2", "C2", "" }, { "", "", "", "" } });
}

TEST_CASE("parse empty rows")
{
    lazycsv::parser<std::string, lazycsv::has_header<false>> parser{ "\nA0,B0,C0,D0\nA1,B1,C1,D1\n\n\nA2,B2,C2,D2\n\nA3,B3,C3,D3\n\n" };
    check_rows(
        parser,
        { { "" },
          { "A0", "B0", "C0", "D0" },
          { "A1", "B1", "C1", "D1" },
          { "" },
          { "" },
          { "A2", "B2", "C2", "D2" },
          { "" },
          { "A3", "B3", "C3", "D3" },
          { "" } });
}

TEST_CASE("skip last new line")
{
    lazycsv::parser<std::string, lazycsv::has_header<false>> parser{ "A0\nA1\nA2\nA3\n" };
    check_rows(parser, { { "A0" }, { "A1" }, { "A2" }, { "A3" } });
}

TEST_CASE("without last new line")
{
    lazycsv::parser<std::string, lazycsv::has_header<false>> parser{ "A0\nA1\nA2\nA3" };
    check_rows(parser, { { "A0" }, { "A1" }, { "A2" }, { "A3" } });
}

TEST_CASE("rows with different cell count")
{
    lazycsv::parser<std::string, lazycsv::has_header<false>> parser{ "A0,B0,C0\nA1,B1,C1,D1\nA2\n\n" };
    check_rows(parser, { { "A0", "B0", "C0" }, { "A1", "B1", "C1", "D1" }, { "A2" }, { "" } });
}

TEST_CASE("trim cells with TAB and SPACE characters")
{
    lazycsv::parser<std::string, lazycsv::has_header<false>> parser{
        "A0, \t  B0,C0,D0\n A1,\tB1,C1,D1\t\n\tA2,B2 , C2 ,D2\n   A3,B3\t,\tC3,D3 \n"
    };
    check_rows(parser, { { "A0", "B0", "C0", "D0" }, { "A1", "B1", "C1", "D1" }, { "A2", "B2", "C2", "D2" }, { "A3", "B3", "C3", "D3" } });
}

TEST_CASE("custom delimiter")
{
    lazycsv::parser<std::string, lazycsv::has_header<false>, lazycsv::delimiter<'-'>> parser{
        "A0-B0-C0-D0\nA1-B1-C1-D1\nA2-B2-C2-D2\nA3-B3-C3-D3\n"
    };
    check_rows(parser, { { "A0", "B0", "C0", "D0" }, { "A1", "B1", "C1", "D1" }, { "A2", "B2", "C2", "D2" }, { "A3", "B3", "C3", "D3" } });
}

TEST_CASE("Quattionas ")
{
    std::string_view test("\"longstring1234567891123456789123456789123456789112345678912345678912345678911234567891234567891234567891123456789123456789 String,  literal \"\", \"of3 \"2\"\"\"chars \"\"\" mark  but longer  \"\"\",D \"AB\"\n");
    lazycsv::parser<std::string, lazycsv::has_header<false>, lazycsv::delimiter<','>> parser{
      test
    };
       for (const auto row : parser)
    {
        size_t cellitt = 0;
        for (const auto cell : row)
        {
             if  (cellitt++)
                    REQUIRE_EQ("D \"AB\"", cell.raw().begin());
                break;
             std::string s1  = {cell.raw().begin(), cell.raw().end()};
             std::string s2  = {test.begin(), test.end() + 57 +64 +64};
                bool a = std::strcmp(s1.c_str(), s2.c_str());
                 REQUIRE_EQ( a , true);
             
        }
    }
}


TEST_CASE("parse test2.csv with mmap_source and use cells function")
{
    lazycsv::parser parser{ "inputs/test2.csv" };

    std::vector<std::vector<std::string>> expected_rows{
        { "Name Field", "Text Field" }, { "Test Name A", "\"Test 1, Test 2, Test 3\"" }, { "Test Name B", "\"\"\"Quoted text\"\"\"" }, { "Test Name C", "\"Try \"\"A, B, C\"\" for fun!\"" }
    };

    const auto [a, b] = parser.header().cells(0, 1);
    auto header_cells = expected_rows.at(0);
    REQUIRE_EQ(header_cells[0], a.raw());
    REQUIRE_EQ(header_cells[1], b.raw());


    auto row_index = 1;
    for (const auto row : parser)
    {
        auto expected_cells = expected_rows.at(row_index++);
        const auto [a, b] = row.cells(0, 1);
        REQUIRE_EQ(expected_cells[0], a.raw());
        REQUIRE_EQ(expected_cells[1], b.raw());
    }
    REQUIRE_EQ(row_index, expected_rows.size());
}