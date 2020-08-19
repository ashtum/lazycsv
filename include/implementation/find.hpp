#pragma once
#include "find_generic.hpp"
#include <algorithm>
#include <bitset>
#include <climits>
#include <cstddef>
#include <immintrin.h>
#include <tuple>
#include <iostream>
#include <iomanip>
#include <limits>
#include <memory>
#include <string_view>
#include <array>
#include <type_traits>
#include <wmmintrin.h>
#include <optional>
namespace lazycsv
{
namespace detail
{
    template <class T, std::size_t N, std::size_t... Is>
auto unpack_impl(std::array<T, N> &&arr, std::index_sequence<Is...>) -> decltype(std::make_tuple(arr[Is]...)) {
    return std::make_tuple( arr[Is]... );
}
/// Unpa
template <class T, std::size_t N>
auto unpack(std::array<T, N> &&arr) {
    return unpack_impl(std::forward<std::array<T, N>>(arr), std::make_index_sequence<N>{});
}
namespace debug_output
{
void print_bytes_in_int(int toprint);
void print_bytes_in_64int(uint64_t toprint);
/// printing debug macros

static std::ios_base::fmtflags f( std::cout.flags() ); 
//#define BOOL_PRINT
#ifdef BOOL_PRINT
#define MPRINT(X) \
    std::cout <<  std::left << std::setw(30) ;              \
    std::cout << #X;                              \
    debug_output::print_bytes_in_64int(X); \
    std::cout.flags( detail::debug_output::f );
#define MPRINT2(X) std::cout << #X << "\t" << X << '\n';
#else
#define MPRINT(X) ;
#define MPRINT2(X) ;
#endif
}
template<class pointer, class... Args>
[[gnu::target("avx2")]] std::array<uint64_t, sizeof...(Args)> createBitmask(pointer first, Args... args)
{
    __m256i _charvalues1 = _mm256_stream_load_si256((__m256i*)(first));
    __m256i _charvalues2 = _mm256_stream_load_si256((__m256i*)(first + 32));
    if (*first == 's')
        int i = 1+1;
    std::array<uint64_t, sizeof...(Args)> results{};
    size_t index = 0;
    for (auto&& value : { args... })
    {
        auto _compare = _mm256_set1_epi8(value);
        auto _Mask1 = _mm256_cmpeq_epi8(_charvalues1, _compare);
        auto _Mask2 = _mm256_cmpeq_epi8(_charvalues2, _compare);
        uint32_t Mask1 = _mm256_movemask_epi8(_Mask1);
        uint32_t Mask2 = _mm256_movemask_epi8(_Mask2);
        uint64_t res = (uint64_t)Mask1 + ((uint64_t)Mask2 << 32);
        results[index++] = res;
    }
    return results;
}

[[gnu::target("pclmul")]] uint64_t carry_less_multiplication(uint64_t a, uint64_t b) {
    __m128i _a = _mm_set_epi64x(0, a);
    __m128i _b = _mm_set_epi64x(0, b);
    __m128i _c = _mm_clmulepi64_si128(_a, _b, 0x00);
    uint64_t dst[2]{};
    _mm_store_si128((__m128i*)dst, _c);
    return dst[0];
}[[gnu::target("bmi")]] inline uint64_t CountTreailingZero(uint64_t a)
{
    return _tzcnt_u64(a);
}

[[gnu::target("default")]] uint64_t CountTreailingZero(uint64_t n) {
    if (!n)
    return 64;
    return __builtin_ctzl(n);
}

const char * First_32Bit_aligned_pointer(const char* first, const char* last, size_t LengthOfStringNeededToEnableSimDinBytes)
    { // callculate if between first and last there is a 32 bit aligned piece long enough for simd
        void* nonConstFirst = const_cast<void*>(static_cast<const void*>(first));
        /// Length of string passed in bits
        size_t length = std::distance(first, last) * CHAR_BIT;
        return  const_cast<const char*>(
            static_cast<char*>((std::align(32, LengthOfStringNeededToEnableSimDinBytes * CHAR_BIT, nonConstFirst, length))));
    }

/**
 *  Returns a pointer to the next occursens off delimter which is not surrounded by a odd sequence of quotetation charreters.
 *  Plus a state object that can be used to more efficiently find the next occurens after it.
 *  For example a string like: `hello """," world,`  will return the location of second occurrence of `,` assuming that was the delimter and
 * '"' was the quotetation character it uses a algorithm adapted from [this paper](https://arxiv.org/abs/1902.08318) if either state.last or
 * state.chunkBegin is null. It is assumed to be the first invocation
 *  @param first points to the first charater to consider for searching
 *  @param last points to one past the last charater in the sequence to search for.
 *  @param delimter  The charter to find the first none escaped occurrence off.
 *  @param state A state object returned by previous invocation of this function.
 *  @tparam quotetation The quotetation char to use for starting strings
 *  @returns Returns a tuple of a pointer to the first occurrence and a state struct which can be reused in future invocations.
 *
 */
template<typename Policy>
template<typename findMode, class... TS>
std::tuple<const char*, const char*> chunker<Policy>::find_I(Quotation_Policy::SIMD<TS...>, findMode, const char* first, const char* last)
{
    if (last == nullptr)
        throw last;
    using std::cout;
    using std::ptrdiff_t;
    #ifdef BOOL_PRINT
    cout << "first,last  " << std::hex << (ptrdiff_t)first << " : " << (ptrdiff_t)last << '\n' << std::dec;
    cout << "size is " << last - first << '\n';
    cout << std::endl;
    #endif
    if (first == last)
        return std::make_tuple(last, last);
    //cout << "find called with" << std::string_view(first, last - first) << '\n';
    // some aliasing for readability ;
    auto& state = *this;
    bool& carrybit1 = this->carrybit1;
    bool& IsQuotedBlock = this->IsQuotedBlock;
    constexpr char quotetation = Policy::quotationMark;
    constexpr char delimter = Policy::delimiter;
    constexpr char newline = Policy::line_ending;
    constexpr size_t LengthOfStringNeededToEnableSimDinBytes = 32; // todo make this part of the policy class
    static_assert(LengthOfStringNeededToEnableSimDinBytes % 32 == 0, "can only work in multiples of 32 chars aka 256 bits ");

    const char* first_32Bit_aligned_pointer = First_32Bit_aligned_pointer(first, last, LengthOfStringNeededToEnableSimDinBytes);
    if (first_32Bit_aligned_pointer == nullptr)
            return state.find_I(Quotation_Policy::Generic<TS...>{}, find_mode::find_both{}, first, last);
    
    /// chars to hold the result in
    auto first_deliminter = last;
    auto first_endline = last;
    
    //int loop_count = -1;
    unsigned int offset;
    /// values to hold the bitmasks in
    uint64_t quotetationMask, delimM, newLineMask;
    /// bool value to determine if can retrun.
    bool Break = false;

    const char* itter = first_32Bit_aligned_pointer;

    /// Lambda returning a std::optional that mayby contain the value to be returned
    auto ReturnLambda = [&](const char * itterator )
    {
        if constexpr (std::is_same<findMode, find_mode::find_delim>::value)
        {
            if (first_deliminter < itterator)
                Break = true; // end loop if we found a delimter 
        }
        else if constexpr (std::is_same<findMode, find_mode::find_lineEnd>::value)
        {
            if (first_endline < itterator)
                Break = true; // end loop if we found a endline 
        }
        else if constexpr (std::is_same<findMode, find_mode::find_both>::value)
        {
            Break = (first_endline < itterator);
            if (Break && first_deliminter >= itterator)
                first_deliminter = last; // did not find a delim so set it to the last
        }
        return Break ?  std::optional<std::tuple<const char*, const char*>>{
            std::make_tuple( first_deliminter, first_endline )}
             : std::nullopt;
    };



    // caling the genreic version on the non algined part
    std::tie(first_deliminter, first_endline) = (this->find_I(
        Quotation_Policy::Generic<TS...>{}, find_mode::find_both{}, first, first_32Bit_aligned_pointer));
    itter = first_32Bit_aligned_pointer;

    auto res = ReturnLambda(itter);
    if (res)
        return res.value();


    //
    
    do
    {
        MPRINT2(IsQuotedBlock);
        size_t distance = static_cast<int>(std::distance(itter, last));
        #ifdef BOOL_PRINT
        auto s = std::string_view(itter, 64) ;
        std::cout <<std::left << std::setw(30) << "input " << s << '\n';
        std::cout.flags( detail::debug_output::f );
        #endif
        // input 	"String, literal "", "of3 "2"   "chars """ mark. but longer  """
        auto evenBits = std::bitset<64>(0x5555555555555555);
        std::tie(quotetationMask, delimM, newLineMask) = unpack(createBitmask(itter, quotetation, delimter, newline));
        {
            MPRINT(quotetationMask);
            MPRINT(delimM);
            MPRINT(newLineMask);
            uint64_t B = quotetationMask;
            MPRINT(B); // B     	1________________11__1____1_1___1______111___________________111
            uint64_t E = (evenBits.to_ulong());
            MPRINT(E);                                 // E     	1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_
            uint64_t O = (evenBits.flip().to_ulong()); // odd bits
            MPRINT(O);                                 // O         _1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1
            uint64_t S = B & ~(B << 1);
            S -= carrybit1;
            MPRINT(S); // S	        1________________1___1____1_1___1______1_____________________1__
            uint64_t ES = S & E;
            MPRINT(ES); // ES	    1_________________________1_1___1_______________________________
            uint64_t EC = B + ES;
            MPRINT(EC); // EC        _1_______________11__1_____1_1___1_____111___________________111
            uint64_t ECE = EC & ~B;
            MPRINT(ECE);
            uint64_t OD1 = ECE & ~E;
            MPRINT(OD1);
            uint64_t OS = S & O;
            MPRINT(OS);
            uint64_t OC = (B + OS);
            bool overflow = (std::numeric_limits<uint64_t>::max() - B < OS) ? true : false;
            MPRINT(OC);
            uint64_t OCE = OC & ~B;
            MPRINT(OCE);
            uint64_t OD2 = OCE & E;
            MPRINT(OD2);
            uint64_t OD = OD1 | OD2;
            MPRINT(OD);
            carrybit1 = (OD >> 32) & 1U;
            MPRINT2(carrybit1);
            auto Q = (OD >> 1) + ((uint64_t)overflow << 63);
            MPRINT(Q);
            auto result = carry_less_multiplication(Q, ~0);
            if (IsQuotedBlock)
                result = ~result;
            IsQuotedBlock = (result >> 63);

            MPRINT2(IsQuotedBlock);

            MPRINT(result); // PCMull	1111111111111111111111____111___1111111111_____________________1
            delimM &= ~(result);
            newLineMask &= ~(result);
        }
        if (first_deliminter == itter)
            std::advance(first_deliminter , CountTreailingZero(delimM));
        if (first_endline == itter)
            std::advance(first_endline , CountTreailingZero(newLineMask));
        auto merged = delimM | newLineMask;
        MPRINT(delimM);
        offset = CountTreailingZero(merged);
        MPRINT(newLineMask);


        auto l = [&](uint64_t bitset, auto callback)
        {
            while (bitset != 0) {
                uint64_t t = bitset & -bitset;
                int r = __builtin_ctzl(bitset);
                callback(r);
                bitset ^= t;
        } ;};
        MPRINT(merged);
        std::bitset<64> set{newLineMask};
        MPRINT(newLineMask);
        l(merged, [=](int i){
            saveInBuffer(itter + i, set[i]);
        }); 
        res = ReturnLambda(itter);
        if (res)
            return res.value();
        //loop_count++;
    } while (std::advance(itter, 64), itter < last);

    return std::make_tuple(std::min(itter + offset, last), last);
}
namespace debug_output
{
void print_bytes_in_int(int toprint)
{
    auto bset = std::bitset<32>(toprint);
    auto out = bset.to_string('_', '1');
    std::cout << std::string(out.rbegin(), out.rend()) << std::endl;
}
void print_bytes_in_64int(uint64_t toprint)
{
    auto bset = std::bitset<64>(toprint);
    auto out = bset.to_string('_', '1');
    std::cout << std::string(out.rbegin(), out.rend()) << std::endl;
}
}
}
}