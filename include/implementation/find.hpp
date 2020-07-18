#pragma once 
#include <type_traits>
#include <immintrin.h>
#include  <algorithm>
#include  <array>
#include  <iostream>
#include  <string_view>
#include <wmmintrin.h>
#include  <bitset>
#include  <limits>
namespace lazycsv
{
    namespace Quotation_Policy
    {
        class None {};
        template<char Qoute>
        class SIMD{        }; 
        // use 
    }
    namespace detail
    {
        namespace debug_output {
    void print_bytes_in_int(int toprint);
    void print_bytes_in_64int(uint64_t toprint);
    #define MPRINT(X)  std::cout << #X <<"\t";   debug_output::print_bytes_in_64int(X );
    #define MPRINT2(X)  std::cout << #X <<"\t" << X << '\n';
    }

    template< class InputIt, class T , class U = Quotation_Policy::None>
     InputIt find( InputIt first, InputIt last, const T& value, U )
    {
        //static_assert(!std::is_same<std::remove_reference<T>,char>::value, "A overload for char exists");
        return std::find(first,last,value);
    }
    template< class InputIt , class U = Quotation_Policy::None >
     InputIt find( InputIt first, InputIt last, const char value, U)
    {
        return std::find(first,last,value);
    }

    [[gnu::target("avx2")]]
    uint64_t  createBitmask(const std::array<char ,64> &array, char val)
    {
        __m256i  _charvalues1 = _mm256_lddqu_si256 ((__m256i*)&array);
        __m256i  _charvalues2 = _mm256_lddqu_si256 ((__m256i*)&(array[32]));
        auto _compare = _mm256_set1_epi8(val);
        auto _Mask1 = _mm256_cmpeq_epi8(_charvalues1, _compare);
        auto _Mask2 = _mm256_cmpeq_epi8(_charvalues2, _compare);
        uint64_t Mask1 = _mm256_movemask_epi8(_Mask1);
        uint64_t Mask2 = _mm256_movemask_epi8(_Mask2);
        return Mask1 + (Mask2 << 32);
    }
    [[gnu::target("pclmul")]]
    uint64_t carry_less_multiplication(uint64_t a, uint64_t b)
    {
        __m128i _a = _mm_set_epi64x(0,a);
        __m128i _b = _mm_set_epi64x(0,b);
        __m128i _c  = _mm_clmulepi64_si128(_a,_b,0x00);
        uint64_t dst[2] {};
        _mm_store_si128( (__m128i*) dst, _c);
        return dst[0];
    }
    const char* find__simd(  const char*  first, const char* last, const char delimter , const char quotetation);
    template <char quotetation>
    [[gnu::target("avx2,pclmul")]]
    const char* find(  const char*  first, const char* last, const char delimter , Quotation_Policy::SIMD<quotetation>) 
    {
        return find__simd(first,last,delimter,quotetation);
    }
    const char* find__simd(  const char*  first, const char* last, const char delimter , const char quotetation)
    {
    if (first == last)
        return last;
    const char * itter = first;
    bool carrybit1 = false;
    bool carrybit2 = false;
    int loop_count = -1;
    unsigned int offset;
    do
    {
    [[gnu::aligned(32)]] std::array<char,64> array{};
    std::fill(array.begin(),array.end(), 0);
    size_t distance = static_cast<int>(std::distance(itter,last ));
    size_t length = std::min((size_t)64, distance );
    std::memcpy(&array, itter, std::min(length, distance));
    std::cout << "input \t" <<std::string_view{array.begin(),64} << '\n';
                                                                //input 	"String, literal "", "of3 "2"   "chars """ mark. but longer  """
    auto evenBits = std::bitset<64>(0x5555555555555555);        
    uint64_t B = createBitmask(array, quotetation);                     
    MPRINT(B);                                                  //B     	1________________11__1____1_1___1______111___________________111
    uint64_t E = (evenBits.to_ulong());                         
    MPRINT(E);                                                  //E     	1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_
    uint64_t O = (evenBits.flip().to_ulong()); // odd bits
    MPRINT(O);                                                  //O         _1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1_1
    uint64_t S = B &~( B << 1) ;
    S   -= carrybit1;      
    MPRINT(S);                                                  //S	        1________________1___1____1_1___1______1_____________________1__
    uint64_t ES = S & E;
    MPRINT(ES);                                                 //ES	    1_________________________1_1___1_______________________________
    uint64_t EC = B + ES;
    MPRINT(EC);                                                 //EC        _1_______________11__1_____1_1___1_____111___________________111
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
    auto Q = (OD >> 1) + ((uint64_t)overflow << 63) ;
     MPRINT(Q);
    auto result = carry_less_multiplication(Q,~0);
    if (carrybit2)
        result = ~result;
    carrybit2 = (result >> 63);

    MPRINT2(carrybit2);

     MPRINT(result);                                        //PCMull	1111111111111111111111____111___1111111111_____________________1
    auto delimterMask = createBitmask(array, delimter);
    auto ret = delimterMask & ~(result | Q);
    MPRINT(ret);
    offset = std::bitset<64>(ret)._Find_first(); 
    if (offset < 64)
        break;
    MPRINT2(offset);
    loop_count++; 
    }
    while (std::advance(itter, 64), itter < last);
    
    return std::min(itter + offset, last);
}
    namespace debug_output {
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

}}