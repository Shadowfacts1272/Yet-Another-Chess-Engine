#ifndef BOARD_H
#define BOARD_H

#include <cstdint>
#include <vector>
#include <cmath>
#include "move.h"


// Enumerate board properties
enum piece_types {WP, BP, WN, BN, WB, BB, WR, BR, WQ, BQ, WK, BK, E};

enum colors {WHITE, BLACK, BOTH};

// a1 = 0, h1 = 7, a8 = 56, h8 = 63
enum squares {
    a1, b1, c1, d1, e1, f1, g1, h1,
    a2, b2, c2, d2, e2, f2, g2, h2,
    a3, b3, c3, d3, e3, f3, g3, h3,
    a4, b4, c4, d4, e4, f4, g4, h4,
    a5, b5, c5, d5, e5, f5, g5, h5,
    a6, b6, c6, d6, e6, f6, g6, h6,
    a7, b7, c7, d7, e7, f7, g7, h7,
    a8, b8, c8, d8, e8, f8, g8, h8
};

// Enumerate ray directions
enum directions {NO, NE, EA, SE, SO, SW, WE, NW};

class Board {
private:
public:
    // Initialization functions
    void initialize_sliding_pieces();
    void initialize();
    void initialize_in_between();
    void initialize_fen(std::string fen);

    // Representing board state
    uint64_t bitboards [13];
    uint64_t occupancies [3];
    int piece_list [64] = {
        E, E, E, E, E, E, E, E,
        E, E, E, E, E, E, E, E,
        E, E, E, E, E, E, E, E,
        E, E, E, E, E, E, E, E,
        E, E, E, E, E, E, E, E,
        E, E, E, E, E, E, E, E,
        E, E, E, E, E, E, E, E,
        E, E, E, E, E, E, E, E,
    };
    std::vector<int> castling_rights;
    std::vector<int> en_passant_square;
    bool side;

    // Display purposes
    void print();
    void print_bits(uint64_t bitboard);
    void print_moves(std::vector<Move> move_list);
    void print_move(Move move);

    // Classical sliding piece attacks for magic bitboards
    uint64_t positive_ray_attacks (int square, int direction, uint64_t occupancy);
    uint64_t negative_ray_attacks (int square, int direction, uint64_t occupancy);
    uint64_t diagonal_attacks (int square, uint64_t occupancy);
    uint64_t anti_diagonal_attacks (int square, uint64_t occupancy);
    uint64_t file_attacks (int square, uint64_t occupancy);
    uint64_t rank_attacks (int square, uint64_t occupancy);
    uint64_t classical_rook_attacks (int square, uint64_t occupancy);
    uint64_t classical_bishop_attacks (int square, uint64_t occupancy);

    // Magic bitboard sliding piece attacks
    uint64_t rook_attacks (int square, uint64_t occupancy);
    uint64_t bishop_attacks (int square, uint64_t occupancy);
    uint64_t queen_attacks (int square, uint64_t occupancy);

    // Move generation
    void legal_moves(std::vector<Move> &move_list);
    void legal_captures(std::vector<Move> &move_list);

    // Making and unamking moves
    void make_move(Move move);
    void unmake_move(Move move);
    void set_square(int square, int piece);
    // To account for the adding of useless bits
    const uint64_t occupancy_modifier[13] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0};
    void remove_square(int square, int piece);

    // Other functions and arrays concerning move generation
    uint64_t in_between [64] [64]; //[from] [to]
    uint64_t x_ray_rook_attacks(int square, uint64_t occupancy, uint64_t blockers);
    uint64_t x_ray_bishop_attacks(int square, uint64_t occupancy, uint64_t blockers);
    uint64_t attacks_to_square(int square);
    uint64_t absolute_pins(int square);
    uint64_t attack_map(uint64_t occupancy);

    // For special move flags (promotion, double pawn push and  castling)
    const uint64_t promotion_ranks [2] = {
        0xff000000000000,
        0xff00
    };

    const uint64_t rank_2_7 [2] = {
        0xff00,
        0xff000000000000
    };

    const uint64_t rank_4_5 [2] = {
        0xff000000,
        0xff00000000
    };

    const int castling_locations [2] [5] = {
        {e1, a1, h1, g1, c1},
        {e8, a8, h8, g8, c8}
    };

    const uint64_t castling_check_mask [2] [2] = {
        {0x60, 0xc},
        {0x6000000000000000, 0xc00000000000000}
    };

    const uint64_t castling_occupancy_mask [2] [2] = {
        {0x60, 0xe},
        {0x6000000000000000, 0xe00000000000000}
    };
};

#endif
