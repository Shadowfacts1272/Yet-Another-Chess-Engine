#include <iostream>
#include "board.h"
#include "bits.h"

uint64_t perft(Board &board, int depth) {
    if(depth == 1) {
        std::vector<Move> move_list = {};
        board.legal_moves(move_list);
        return move_list.size();
    }

    int nodes = 0;
    std::vector<Move> move_list = {};
    board.legal_moves(move_list);
    for(int i = 0; i < move_list.size(); i++) {
        board.make_move(move_list[i]);
        nodes += perft(board, depth - 1);
        board.unmake_move(move_list[i]);
    }
    return nodes;
}

void perft_split(Board &board, int depth) {
    uint64_t total_nodes = 0;
    uint64_t nodes;
    std::vector<Move> move_list = {};
    board.legal_moves(move_list);
    std::cout<<"\n";
    for(int i = 0; i < move_list.size(); i++) {
        board.print_move(move_list[i]);
        board.make_move(move_list[i]);
        if(depth == 1) {
          nodes = 1;
        } else {
          nodes = perft(board, depth - 1);
        }

        std::cout << nodes << "\n";
        total_nodes += nodes;
        board.unmake_move(move_list[i]);
    }

    std::cout << "total nodes: " << total_nodes << "\n";
}

int main() {
    // Initialize board properties
    Board board;
    board.initialize();
    board.initialize_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    board.print();
    perft_split(board, 6);
}
