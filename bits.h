#include <iostream>
#include <bitset>
#include <cstdint>

inline int lsb(uint64_t bitboard) {
    return __builtin_ctzll(bitboard);
}

const int index64[64] = {
    0, 47,  1, 56, 48, 27,  2, 60,
   57, 49, 41, 37, 28, 16,  3, 61,
   54, 58, 35, 52, 50, 42, 21, 44,
   38, 32, 29, 23, 17, 11,  4, 62,
   46, 55, 26, 59, 40, 36, 15, 53,
   34, 51, 20, 43, 31, 22, 10, 45,
   25, 39, 14, 33, 19, 30,  9, 24,
   13, 18,  8, 12,  7,  6,  5, 63
};

inline int msb(uint64_t bitboard) {
    const uint64_t debruijn64 = 0x03f79d71b4cb0a89;
    bitboard |= bitboard >> 32;
    bitboard |= bitboard >> 16;
    bitboard |= bitboard >> 8;
    bitboard |= bitboard >> 4;
    bitboard |= bitboard >> 2;
    bitboard |= bitboard >> 1;
    return index64[(bitboard * debruijn64) >> 58];
}

inline void pop_lsb(uint64_t &bitboard) {
    bitboard &= bitboard - 1;
}

inline int return_lsb(uint64_t &bitboard) {
    int least_significant_bit = lsb(bitboard);
    pop_lsb(bitboard);
    return least_significant_bit;
}

inline int pop_count(uint64_t bitboard) {
    return __builtin_popcountll(bitboard);
}

inline void pop_bit(uint64_t &bitboard, int square) {
    bitboard &= ~(1ULL << square);
}

inline void set_bit(uint64_t &bitboard, int square) {
    bitboard |= 1ULL << square;
}

inline uint64_t get_bit(uint64_t bitboard, int square) {
    return bitboard & (1ULL << square);
}
