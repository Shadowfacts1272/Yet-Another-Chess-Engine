#include <iostream>
#include <string>
#include "board.h"
#include "arrays.h"
#include "magic.h"
#include "bits.h"

// Some pseudocode taken and rewritten from the resource https://www.chessprogramming.org/Main_Page

// Positive rays are north, north east, east, and north west
uint64_t Board::positive_ray_attacks(int square, int direction, uint64_t occupancy) {
	uint64_t attacks = rays[direction][square];
	uint64_t blocker = attacks & occupancy;
	if(blocker) {
	   int blocker_sq = lsb(blocker);
	   attacks ^= rays[direction][blocker_sq];
	}
	return attacks;
}

// Negative rays are south, southeast, west, southwest
uint64_t Board::negative_ray_attacks(int square, int direction, uint64_t occupancy) {
	uint64_t attacks = rays[direction][square];
	uint64_t blocker = attacks & occupancy;
	if(blocker) {
		int blocker_sq = msb(blocker);
	   attacks ^= rays[direction][blocker_sq];
	}
	return attacks;
}

// Return union of ray attacks
uint64_t Board::diagonal_attacks(int square, uint64_t occupancy) {
	return positive_ray_attacks(square, NE, occupancy) | negative_ray_attacks(square, SW, occupancy);
}

uint64_t Board::anti_diagonal_attacks(int square, uint64_t occupancy) {
	return positive_ray_attacks(square, NW, occupancy) | negative_ray_attacks(square, SE, occupancy);
}

uint64_t Board::file_attacks(int square, uint64_t occupancy) {
	return positive_ray_attacks(square, NO, occupancy) | negative_ray_attacks(square, SO, occupancy);
}

uint64_t Board::rank_attacks(int square, uint64_t occupancy) {
	return positive_ray_attacks(square, EA, occupancy) | negative_ray_attacks(square, WE, occupancy);
}

// For eventual initialization of magic bitboard
uint64_t Board::classical_rook_attacks(int square, uint64_t occupancy) {
	return file_attacks(square, occupancy) | rank_attacks(square, occupancy);
}

uint64_t Board::classical_bishop_attacks(int square, uint64_t occupancy) {
	return diagonal_attacks(square, occupancy) | anti_diagonal_attacks(square, occupancy);
}

// Main method of generating sliding piece attacks
uint64_t Board::rook_attacks(int square, uint64_t occupancy) {
    return lookup_table[rook_magics[square].position + ((occupancy & rook_mask[square]) * rook_magics[square].factor >> 52)];
}
uint64_t Board::bishop_attacks(int square, uint64_t occupancy) {
	return lookup_table[bishop_magics[square].position + ((occupancy & bishop_mask[square]) * bishop_magics[square].factor >> 55)];
}
uint64_t Board::queen_attacks(int square, uint64_t occupancy) {
	return rook_attacks (square, occupancy) | bishop_attacks (square, occupancy);
}

// Generate pins along rook and bishop attacks
uint64_t Board::x_ray_rook_attacks(int square, uint64_t occupancy, uint64_t blockers) {
	uint64_t attacks = rook_attacks(square, occupancy);
	blockers &= attacks;
	return attacks ^ rook_attacks(square, occupancy ^ blockers);
}

uint64_t Board::x_ray_bishop_attacks(int square, uint64_t occupancy, uint64_t blockers) {
	uint64_t attacks = bishop_attacks(square, occupancy);
	blockers &= attacks;
	return attacks ^ bishop_attacks(square, occupancy ^ blockers);
}

// Find pieces that attack a specific square
uint64_t Board::attacks_to_square(int square) {
	uint64_t attackers = 0ULL;
	bool opp_side = !side;
	// Pawns
	uint64_t board = bitboards[WP + opp_side];
	attackers |= pawn_attacks[side][square] & board;

	// Knights
	board = bitboards[WN + opp_side];
	attackers |= knight_mask[square] & board;

	// Bishops and Queens
	board = bitboards[WB + opp_side] | bitboards[WQ + opp_side];
	attackers |= bishop_attacks(square, occupancies[BOTH]) & board;
	// Rooks and Queens
	board = bitboards[WR + opp_side] | bitboards[WQ + opp_side];
	attackers |= rook_attacks(square, occupancies[BOTH]) & board;
	// Kings
	board = bitboards[WK + opp_side];
	attackers |= king_mask[square] & board;
	return attackers;
}

// Generates a bitboard showing squares the opponent attacks
uint64_t Board::attack_map(uint64_t occupancy) {
	uint64_t mapped_attacks = 0ULL;
	bool opposite_side = !side;
	// Pawns
	uint64_t board = bitboards[WP + opposite_side];
	while(board) {
		mapped_attacks |= pawn_attacks[opposite_side][return_lsb(board)];
	}
	// Knights
	board = bitboards[WN + opposite_side];
	while(board) {
		mapped_attacks |= knight_mask[return_lsb(board)];
	}
	// Bishops and queens
	board = bitboards[WB + opposite_side] | bitboards[WQ + opposite_side];
	while(board) {
		mapped_attacks |= bishop_attacks(return_lsb(board), occupancy);
	}
	//Rooks and queens
	board = bitboards[WR + opposite_side] | bitboards[WQ + opposite_side];
	while(board) {
		mapped_attacks |= rook_attacks(return_lsb(board), occupancy);
	}
	// King
	board = bitboards[WK + opposite_side];
	mapped_attacks |= king_mask[lsb(board)];
	return mapped_attacks;
}

// Generate a bitboard of pieces that are pinned to the king
uint64_t Board::absolute_pins(int square) {
	uint64_t pinned = 0;
	uint64_t pinner = x_ray_rook_attacks(square, occupancies[BOTH], occupancies[side]) & (bitboards[WR + !side] | bitboards[WQ + !side]);
	while(pinner) {
		int pin_square = lsb(pinner);
		pinned |= in_between[square] [pin_square] & occupancies[side];
		pinner &= pinner - 1;
	}

	pinner = x_ray_bishop_attacks(square, occupancies[BOTH], occupancies[side]) & (bitboards[WB + !side] | bitboards[WQ + !side]);
	while(pinner) {
		int pin_square = lsb(pinner);
		pinned |= in_between[square] [pin_square] & occupancies[side];
		pinner &= pinner - 1;
	}

	return pinned;
}

// Populate a vector with legal moves
void Board::legal_moves(std::vector<Move> &move_list) {
	// Initialize some variables
	move_list.clear();
	int target_square, source_square;
	int king_square = lsb(bitboards[WK + side]);
	uint64_t board = 0ULL;
	uint64_t targets = 0ULL;
	uint64_t checkers = attacks_to_square(king_square);
	uint64_t check_mask;
	uint64_t pin_mask;

	// Generate pin masks
	uint64_t hv_pinmask = x_ray_rook_attacks(king_square, occupancies[BOTH], occupancies[side]) & (bitboards[WR + !side] | bitboards[WQ + !side]);
	uint64_t da_pinmask = x_ray_bishop_attacks(king_square, occupancies[BOTH], occupancies[side]) & (bitboards[WB + !side] | bitboards[WQ + !side]);
	board = hv_pinmask;
	for(; board; board &= board - 1) {
		hv_pinmask |= in_between[lsb(board)] [king_square];
	}
	board = da_pinmask;
	for(; board; board &= board - 1) {
		da_pinmask |= in_between[lsb(board)] [king_square];
	}

	// Get king moves
	board = bitboards[WK + side];
	uint64_t king_attack_map = attack_map(occupancies[BOTH] ^ board);
	targets = king_mask[king_square] & ~occupancies[side] & ~king_attack_map;
	for(; targets; targets &= targets - 1) {
		target_square = lsb(targets);
		move_list.push_back(Move(king_square, target_square, WK + side, piece_list[target_square], 0, none));
	}

	switch(pop_count(checkers)) {
		case 2: // No other piece moves possible
			break;
		case 1: // No castling and must block check. And pinned pieces cannot move
			check_mask = in_between[king_square][lsb(checkers)] | checkers;
			// Horizontal sliders
			board = (bitboards[WQ + side] | bitboards[WR + side]) & ~(hv_pinmask | da_pinmask);
			for(; board; board &= board - 1) {
				source_square = lsb(board);
				targets = rook_attacks(source_square, occupancies[BOTH]) & ~occupancies[side] & check_mask;
				for(; targets; targets &= targets - 1) {
					target_square = lsb(targets);
					move_list.push_back(Move(source_square, target_square, piece_list[source_square], piece_list[target_square], 0, none));
				}
			}

			// Diagonal sliders
			board = (bitboards[WQ + side] | bitboards[WB + side]) & ~(da_pinmask | hv_pinmask);
			for(; board; board &= board - 1) {
				source_square = lsb(board);
				targets = bishop_attacks(source_square, occupancies[BOTH]) & ~occupancies[side] & check_mask;
				for(; targets; targets &= targets - 1) {
					target_square = lsb(targets);
					move_list.push_back(Move(source_square, target_square, piece_list[source_square], piece_list[target_square], 0, none));
				}
			}

			// Knights
			board = bitboards[WN + side] & ~(hv_pinmask | da_pinmask);
			for(; board; board &= board - 1) {
				source_square = lsb(board);
				targets = knight_mask[source_square] & ~occupancies[side] & check_mask;
				for(; targets; targets &= targets - 1) {
					target_square = lsb(targets);
					move_list.push_back(Move(source_square, target_square, WN + side, piece_list[target_square], 0, none));
				}
			}

			// Pawns (not promoting)
			board = bitboards[WP + side] & ~(hv_pinmask | da_pinmask | promotion_ranks[side]);
			for(; board; board &= board - 1) {
				source_square = lsb(board);
				targets = ((pawn_attacks[side][source_square] & occupancies[!side]) | (pawn_pushes[side][source_square] & file_attacks(source_square, occupancies[BOTH]) & ~occupancies[BOTH])) & check_mask;
				for(; targets; targets &= targets - 1) {
					target_square = lsb(targets);
					move_list.push_back(Move(source_square, target_square, WP + side, piece_list[target_square], 0, ((1ULL << source_square & rank_2_7[side]) && (1ULL << target_square & rank_4_5[side])) << 2));
				}
			}

			// Pawns (promoting)
			board = bitboards[WP + side] & ~(hv_pinmask | da_pinmask) & promotion_ranks[side];
			for(; board; board &= board - 1) {
				//get source square
				source_square = lsb(board);
				//get targets for attacks
				targets = ((pawn_attacks[side][source_square] & occupancies[!side]) | (pawn_pushes[side][source_square] & ~occupancies[BOTH])) & check_mask;
				for(; targets; targets &= targets - 1) {
					target_square = lsb(targets);
					move_list.push_back(Move(source_square, target_square, WP + side, piece_list[target_square], WN, none));
					move_list.push_back(Move(source_square, target_square, WP + side, piece_list[target_square], WB, none));
					move_list.push_back(Move(source_square, target_square, WP + side, piece_list[target_square], WR, none));
					move_list.push_back(Move(source_square, target_square, WP + side, piece_list[target_square], WQ, none));

				}
			}

			// En passant
			board = pawn_attacks[!side] [en_passant_square.back()] & bitboards[WP + side];
			for(; board; board &= board - 1) {
				source_square = lsb(board);
				uint64_t updated_occupancies = occupancies[BOTH] ^ ((1ULL << source_square) | (1ULL << en_passant_square.back())) ^ pawn_pushes[!side][en_passant_square.back()];
				if(!((rook_attacks(king_square, updated_occupancies) & (bitboards[WR + !side] | bitboards[WQ + !side])) | (bishop_attacks(king_square, updated_occupancies) & (bitboards[WB + !side] | bitboards[WQ + !side])) | (knight_mask[king_square] & bitboards[WN + !side]) | (pawn_attacks[side][king_square] & (bitboards[!side] ^ pawn_pushes[!side][en_passant_square.back()])))) {
					move_list.push_back(Move(source_square, en_passant_square.back(), WP + side, WP + !side, 0, en_passant));
				}
			}

			break;
		case 0: // Account for pins
			// Horizontal sliders (not pinned)
			board = (bitboards[WQ + side] | bitboards[WR + side]) & ~(hv_pinmask | da_pinmask);
			for(; board; board &= board - 1) {
				source_square = lsb(board);
				targets = rook_attacks(source_square, occupancies[BOTH]) & ~occupancies[side];
				for(; targets; targets &= targets - 1) {
					target_square = lsb(targets);
					move_list.push_back(Move(source_square, target_square, piece_list[source_square], piece_list[target_square], 0, none));
				}
			}

			// Horizontal sliders (pinned)
			board = (bitboards[WQ + side] | bitboards[WR + side]) & hv_pinmask;
			for(; board; board &= board - 1) {
				source_square = lsb(board);
				targets = rook_attacks(source_square, occupancies[BOTH]) & ~occupancies[side] & hv_pinmask;
				for(; targets; targets &= targets - 1) {
					target_square = lsb(targets);
					move_list.push_back(Move(source_square, target_square, piece_list[source_square], piece_list[target_square], 0, none));
				}
			}

			// Diagonal sliders (not pinned)
			board = (bitboards[WQ + side] | bitboards[WB + side]) & ~(da_pinmask | hv_pinmask);
			for(; board; board &= board - 1) {
				source_square = lsb(board);
				targets = bishop_attacks(source_square, occupancies[BOTH]) & ~occupancies[side];
				for(; targets; targets &= targets - 1) {
					target_square = lsb(targets);
					move_list.push_back(Move(source_square, target_square, piece_list[source_square], piece_list[target_square], 0, none));
				}
			}

			// Diagonal sliders (pinned)
			board = (bitboards[WQ + side] | bitboards[WB + side]) & da_pinmask;
			for(; board; board &= board - 1) {
				source_square = lsb(board);
				targets = bishop_attacks(source_square, occupancies[BOTH]) & ~occupancies[side] & da_pinmask;
				for(; targets; targets &= targets - 1) {
					target_square = lsb(targets);
					move_list.push_back(Move(source_square, target_square, piece_list[source_square], piece_list[target_square], 0, none));
				}
			}

			// Knights (pinned ones cannot move)
			board = bitboards[WN + side] & ~(hv_pinmask | da_pinmask);
			for(; board; board &= board - 1) {
				source_square = lsb(board);
				targets = knight_mask[source_square] & ~occupancies[side];
				for(; targets; targets &= targets - 1) {
					target_square = lsb(targets);
					move_list.push_back(Move(source_square, target_square, WN + side, piece_list[target_square], 0, none));
				}
			}

			// Pawns (not pinned and not promoting)
			board = bitboards[WP + side] & ~(hv_pinmask | da_pinmask | promotion_ranks[side]);
			for(; board; board &= board - 1) {
				source_square = lsb(board);
				targets = (pawn_attacks[side][source_square] & occupancies[!side]) | (pawn_pushes[side][source_square] & file_attacks(source_square, occupancies[BOTH]) & ~occupancies[BOTH]);
				for(; targets; targets &= targets - 1) {
					target_square = lsb(targets);
					move_list.push_back(Move(source_square, target_square, WP + side, piece_list[target_square], 0, ((1ULL << source_square & rank_2_7[side]) && (1ULL << target_square & rank_4_5[side])) << 2));
				}
			}

			// Pawns (pinned and not promoting)
			board = bitboards[WP + side] & ~promotion_ranks[side] & (hv_pinmask | da_pinmask);
			for(; board; board &= board - 1) {
				source_square = lsb(board);
				targets = (pawn_attacks[side][source_square] & occupancies[!side] & da_pinmask) | (pawn_pushes[side][source_square] & file_attacks(source_square, occupancies[BOTH]) & ~occupancies[BOTH] & hv_pinmask);
				for(; targets; targets &= targets - 1) {
					target_square = lsb(targets);
					move_list.push_back(Move(source_square, target_square, WP + side, piece_list[target_square], 0, ((1ULL << source_square & rank_2_7[side]) && (1ULL << target_square & rank_4_5[side])) << 2));
				}
			}

			// Pawns (promoting and not pinned)
			board = bitboards[WP + side] & ~(hv_pinmask | da_pinmask) & promotion_ranks[side];
			for(; board; board &= board - 1) {
				//get source square
				source_square = lsb(board);
				//get targets for attacks
				targets = (pawn_attacks[side][source_square] & occupancies[!side]) | (pawn_pushes[side][source_square] & ~occupancies[BOTH]);
				for(; targets; targets &= targets - 1) {
					target_square = lsb(targets);
					move_list.push_back(Move(source_square, target_square, WP + side, piece_list[target_square], WN, none));
					move_list.push_back(Move(source_square, target_square, WP + side, piece_list[target_square], WB, none));
					move_list.push_back(Move(source_square, target_square, WP + side, piece_list[target_square], WR, none));
					move_list.push_back(Move(source_square, target_square, WP + side, piece_list[target_square], WQ, none));
				}
			}

			// Pawns (promoting and pinned)
			board = bitboards[WP + side] & promotion_ranks[side] & (hv_pinmask | da_pinmask);
			for(; board; board &= board - 1) {
				//get source square
				source_square = lsb(board);
				//get targets for attacks
				targets = (pawn_attacks[side][source_square] & occupancies[!side] & da_pinmask) | (pawn_pushes[side][source_square] & ~occupancies[BOTH] & hv_pinmask);
				for(; targets; targets &= targets - 1) {
					target_square = lsb(targets);
					move_list.push_back(Move(source_square, target_square, WP + side, piece_list[target_square], WN, none));
					move_list.push_back(Move(source_square, target_square, WP + side, piece_list[target_square], WB, none));
					move_list.push_back(Move(source_square, target_square, WP + side, piece_list[target_square], WR, none));
					move_list.push_back(Move(source_square, target_square, WP + side, piece_list[target_square], WQ, none));

				}
			}

			// En passant
			board = pawn_attacks[!side] [en_passant_square.back()] & bitboards[WP + side];
			for(; board; board &= board - 1) {
				source_square = lsb(board);
				uint64_t updated_occupancies = occupancies[BOTH] ^ ((1ULL << source_square) | (1ULL << en_passant_square.back())) ^ pawn_pushes[!side][en_passant_square.back()];
				if(!((rook_attacks(king_square, updated_occupancies) & (bitboards[WR + !side] | bitboards[WQ + !side])) | (bishop_attacks(king_square, updated_occupancies) & (bitboards[WB + !side] | bitboards[WQ + !side])) | (knight_mask[king_square] & bitboards[WN + !side]))) {
					move_list.push_back(Move(source_square, en_passant_square.back(), WP + side, WP + !side, 0, en_passant));
				}
			}

			// Castling
			int castle = castling_rights.back();
			// Handle king-side castling
			if((castle & (1 << side)) && !(castling_occupancy_mask[side][0] & occupancies[BOTH]) && !(castling_check_mask[side][0] & king_attack_map)) {
				move_list.push_back(Move(castling_locations[side][0], castling_locations[side][3], WK + side, E, 0, k_castling));
			}

			// Handle queen-side castling
			if((castle & (4 << side)) && !(castling_occupancy_mask[side][1] & occupancies[BOTH]) && !(castling_check_mask[side][1] & king_attack_map)) {
				move_list.push_back(Move(castling_locations[side][0], castling_locations[side][4], WK + side, E, 0, q_castling));
			}

			break;
	}
}

// Modifies all applicable occupancies
void Board::set_square(int square, int piece) {
	bitboards[piece] |= 1ULL << square;
	piece_list[square] = piece;
	occupancies[BOTH] |= occupancy_modifier[piece] << square;
	occupancies[side] = occupancies[!side] ^ occupancies[BOTH];
}

void Board::remove_square(int square, int piece) {
	bitboards[piece] &= ~(1ULL << square);
	piece_list[square] = E;
	occupancies[BOTH] &= ~(1ULL << square);
	occupancies[side] = occupancies[side] & occupancies[BOTH];
}

// Make a move
void Board::make_move(Move move) {
	int source_square = move.source();
	int target_square = move.target();
	int piece = move.piece();
	int capture = move.capture();
	en_passant_square.push_back(64);
	castling_rights.push_back(castling_rights.back());
	switch(move.flag()) {
		case none:
			// Remove moving piece, remove possible piece from target square, set piece down
			remove_square(source_square, piece);
			side ^= 1;
			remove_square(target_square, capture);
			side ^= 1;
			/*bitboards[capture] &= ~(1ULL << target_square);
			piece_list[target_square] = E;
			occupancies[BOTH] &= ~(1ULL << target_square);
			occupancies[!side] = occupancies[!side] & occupancies[BOTH];*/
			set_square(target_square, piece + move.promote());
			break;
		case k_castling:
			// Remove and place king
			remove_square(source_square, piece);
			set_square(target_square, piece);
			// Remove and place rook
			remove_square(target_square + 1, WR + side);
			set_square(source_square + 1, WR + side);
			break;
		case q_castling:
			// Remove and place king
			remove_square(source_square, piece);
			set_square(target_square, piece);
			// Remove and place rook
			remove_square(target_square - 2, WR + side);
			set_square(target_square + 1, WR + side);
			break;
		case en_passant:
			// Remove and place pawn
			remove_square(source_square, piece);
			set_square(target_square, piece);
			// Remove target pawn
			side ^= 1;
			remove_square((source_square & 56) + (target_square & 7), side);
			side ^= 1;
			break;
		case double_push:
			// Remove and place pawn
			remove_square(source_square, piece);
			set_square(target_square, piece);
			en_passant_square.back() = lsb(pawn_pushes[!side][target_square]);
			break;
	}

	// Check castling rights
	//Kingside (White)
	if (piece == WK || piece_list[h1] != WR) {
    	castling_rights.back() &= ~1;
  	}
	//Kingside (Black)
  	if (piece == BK || piece_list[h8] != BR) {
    	castling_rights.back() &= ~2;
  	}
	//Queenside (White)
  	if (piece == WK || piece_list[a1] != WR) {
    	castling_rights.back() &= ~4;
  	}
	//Queenside (Black)
  	if (piece == BK || piece_list[a8] != BR) {
    	castling_rights.back() &= ~8;
  	}

	side ^= 1;
}

// Unmake a move
void Board::unmake_move(Move move) {
	castling_rights.pop_back();
	en_passant_square.pop_back();
	int source_square = move.source();
	int target_square = move.target();
	int piece = move.piece();
	int capture = move.capture();
	side ^= 1;
	switch(move.flag()) {
		case none:
			// Restore moving piece, possible piece from target square, and remove target square
			remove_square(target_square, piece + move.promote());
			set_square(source_square, piece);
			side ^= 1;
			set_square(target_square, move.capture());
			side ^= 1;
			break;
		case k_castling:
			// Restore king
			set_square(source_square, piece);
			remove_square(target_square, piece);
			// Restore rook
			set_square(target_square + 1, WR + side);
			remove_square(source_square + 1, WR + side);
			break;
		case q_castling:
			// Restore king
			set_square(source_square, piece);
			remove_square(target_square, piece);
			// Restore rook
			set_square(target_square - 2, WR + side);
			remove_square(target_square + 1, WR + side);
			break;
		case en_passant:
			// Restore pawn
			set_square(source_square, piece);
			remove_square(target_square, piece);
			// Restore target pawn
			side ^= 1;
			set_square((source_square & 56) + (target_square & 7), side);
			side ^= 1;
			break;
		case double_push:
			// Restore pawn
			set_square(source_square, piece);
			remove_square(target_square, piece);
			break;
	}
}

void Board::print() {
    const std::string pieces[13] = {"♙", "♟", "♘", "♞", "♗", "♝", "♖", "♜", "♕", "♛", "♔", "♚", "."};
    int square;
	for(int i = 0; i < 64; i++) {
		square = 63 - (floor(i / 8) * 8 + (7 - (i % 8)));
		if(i % 8 == 0 && i != 64) {
			std::cout << "\n" << floor(square / 8) + 1 << " ";
		} else if (i % 8 == 0) {
			std::cout << "\n" << floor(square / 8) + 1;
		}
		if(i / 8 == 8) {
			std::cout << " ";
		}

		std::cout << pieces[piece_list[square]] << " ";
	}
	std::cout << "\n  a b c d e f g h\n";
}

void Board::print_bits(uint64_t bitboard) {
	int square;
	for(int i = 0; i < 64; i++) {
		square = 63 - (floor(i / 8) * 8 + (7 - (i % 8)));
		if(i % 8 == 0 && i != 64) {
			std::cout << "\n" << floor(square / 8) + 1 << " ";
		} else if (i % 8 == 0) {
			std::cout <<" \n" << floor(square / 8) + 1;
		}
		if(i / 8 == 8) {
			std::cout << " ";
		}

		std::cout << ((bitboard & (1ULL << square)) ? 1 : 0) << " ";
	}
	std::cout << "\n  a b c d e f g h\n";
}

char promoted_pieces[12] = {' ',' ','n','n','b','b','r','r','q','q',' ',' '};

//pretty printing stuff
std::string coordinates [65] = {
	"a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
	"a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
	"a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
	"a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
	"a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
	"a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
	"a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
	"a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
	"-"
};

void Board::print_moves(std::vector<Move> move_list) {
	// loop over moves within a move list
	std::cout << "\nMove:	Capture:\n";
  	for (int i = 0; i < move_list.size(); i++) {
		// init move
		Move move = move_list[i];

		std::cout << coordinates[move.source()];
		std::cout << coordinates[move.target()];
		std::cout << promoted_pieces[move.promote()];
		std::cout << "	 " << "\x1B[32m";
		bool captured = move.capture() != E ? 1 : 0;
		std::cout << captured << "\n";
    	std::cout << "\033[0m";
  	}
	// print total number of moves
  	std::cout << "\nTotal number of moves: ";
	std::cout << move_list.size() << "\n";
}

void Board::print_move(Move move) {
	std::cout << coordinates[move.source()];
	std::cout << coordinates[move.target()];
	std::cout << promoted_pieces[move.promote()] << " ";
}

void Board::initialize_sliding_pieces() {
	// Rook bitboards
	for(int sq = 0; sq < 64; sq++) {
		uint64_t rook_attacks = rook_mask[sq];

		int total_sets = 1ULL << pop_count(rook_attacks);
		uint64_t rook_copy;
		for(int permutation = 0; permutation < total_sets; permutation++) {
			rook_copy = rook_attacks;
			// Create possible blocker permutations
			uint64_t blockers = 0ULL;
			for(int shift = 0; shift < pop_count(rook_attacks); shift++) {
				int index = return_lsb(rook_copy);
				if(permutation & (1 << shift)) {
					blockers |= 1ULL << index;
				}
			}
			lookup_table[rook_magics[sq].position + (blockers * rook_magics[sq].factor >> 52)] = classical_rook_attacks(sq, blockers);
		}
	}

	// Bishop bitboards
	for(int sq = 0; sq < 64; sq++) {
		uint64_t bishop_attacks = bishop_mask[sq];

		int total_sets = 1ULL << pop_count(bishop_attacks);
		uint64_t bishop_copy;
		for(int permutation = 0; permutation < total_sets; permutation++) {
			bishop_copy = bishop_attacks;
			// Create possible blocker permutations
			uint64_t blockers = 0ULL;
			for(int shift = 0; shift < pop_count(bishop_attacks); shift++) {
				int index = return_lsb(bishop_copy);
				if(permutation & (1 << shift)) {
					blockers |= 1ULL << index;
				}
			}
			lookup_table[bishop_magics[sq].position + (blockers * bishop_magics[sq].factor >> 55)] = classical_bishop_attacks(sq, blockers);
		}
	}
}

// Populate the array in_between
void Board::initialize_in_between() {
	const uint64_t m1 = -1;
	const uint64_t a2a7 = 0x0001010101010100;
	const uint64_t b2g7 = 0x0040201008040200;
	const uint64_t h1b7 = 0x0002040810204080;
	for(int sq1 = 0; sq1 < 64; sq1++) {
		for(int sq2 = 0; sq2 < 64; sq2++) {
			uint64_t btwn, line, rank, file;
			btwn = (m1 << sq1) ^ (m1 << sq2);
			file = (sq2 & 7) - (sq1 & 7);
			rank = ((sq2 | 7) -  sq1) >> 3 ;
			line = ((file & 7) - 1) & a2a7;
			line += 2 * (((rank & 7) - 1) >> 58);
			line += (((rank - file) & 15) - 1) & b2g7;
			line += (((rank + file) & 15) - 1) & h1b7;
			line *= btwn & -btwn;
			in_between[sq1] [sq2] = line & btwn;
		}
	}
}

// Uses FEN string to initialize the board
void Board::initialize_fen(std::string FEN) {
	int i = 0;
	int square_counter = 0;
	int square;
	while(FEN[i] != ' ') {
		square = (square_counter ^ 63) ^ 7;
		switch(FEN[i]) {
			case 'P':
				bitboards[WP] |= 1ULL << square;
				piece_list[square] = WP;
				square_counter++;
				break;
			case 'p':
				bitboards[BP] |= 1ULL << square;
				piece_list[square] = BP;
				square_counter++;
				break;
			case 'N':
				bitboards[WN] |= 1ULL << square;
				piece_list[square] = WN;
				square_counter++;
				break;
			case 'n':
				bitboards[BN] |= 1ULL << square;
				piece_list[square] = BN;
				square_counter++;
				break;
			case 'B':
				bitboards[WB] |= 1ULL << square;
				piece_list[square] = WB;
				square_counter++;

				break;
			case 'b':
				bitboards[BB] |= 1ULL << square;
				piece_list[square] = BB;
				square_counter++;
				break;
			case 'R':
				bitboards[WR] |= 1ULL << square;
				piece_list[square] = WR;
				square_counter++;
				break;
			case 'r':
				bitboards[BR] |= 1ULL << square;
				piece_list[square] = BR;
				square_counter++;
				break;
			case 'Q':
				bitboards[WQ] |= 1ULL << square;
				piece_list[square] = WQ;
				square_counter++;
				break;
			case 'q':
				bitboards[BQ] |= 1ULL << square;
				piece_list[square] = BQ;
				square_counter++;
				break;
			case 'K':
				bitboards[WK] |= 1ULL << square;
				piece_list[square] = WK;
				square_counter++;
				break;
			case 'k':
				bitboards[BK] |= 1ULL << square;
				piece_list[square] = BK;
				square_counter++;
				break;
			case '1':
				square_counter++;
				break;
			case '2':
				square_counter += 2;
				break;
			case '3':
				square_counter += 3;
				break;
			case '4':
				square_counter += 4;
				break;
			case '5':
				square_counter += 5;
				break;
			case '6':
				square_counter += 6;
				break;
			case '7':
				square_counter += 7;
				break;
			case '8':
				square_counter += 8;
				break;
			case '/':
				break;
		}
		i++;
	}

	// Side to move
	i++;
	side = 0;
	if(FEN[i] == 'b') {
		side = 1;
	}
	i += 2;

	// Castling
	int castling = 0;
	while(FEN[i] != ' ') {
		switch(FEN[i]) {
			case 'K':
				castling |= 1;
				break;
			case 'k':
				castling |= 2;
				break;
			case 'Q':
				castling |= 4;
				break;
			case 'q':
				castling |= 8;
				break;
			case '-':
				break;
		}
		i++;
	}
	castling_rights.push_back(castling);
	i++;

	// En Passant
	int en_passant = 0;
	if(FEN[i] == '-') {
		en_passant = 64;
	} else {
		switch(FEN[i]) {
			case 'a':
				break;
			case 'b':
				en_passant += 1;
				break;
			case 'c':
				en_passant += 2;
				break;
			case 'd':
				en_passant += 3;
				break;
			case 'e':
				en_passant += 4;
				break;
			case 'f':
				en_passant += 5;
				break;
			case 'g':
				en_passant += 6;
				break;
			case 'h':
				en_passant += 7;
				break;
		}

		switch(FEN[i + 1]) {
			case '3':
				en_passant += 16;
				break;
			case '6':
				en_passant += 40;
				break;
			default:
				break;
		}
	}
	en_passant_square.push_back(en_passant);

	// Initialize the occupancy bitboards
	occupancies[WHITE] = bitboards[WP] | bitboards[WN] | bitboards[WB] | bitboards[WR] | bitboards[WQ] | bitboards[WK];
	occupancies[BLACK] = bitboards[BP] | bitboards[BN] | bitboards[BB] | bitboards[BR] | bitboards[BQ] | bitboards[BK];
	occupancies[BOTH] = occupancies[WHITE] | occupancies[BLACK];
}

void Board::initialize() {
	initialize_sliding_pieces();
	initialize_in_between();
}
