#ifndef MOVE_H
#define MOVE_H

// Representation of difference special move cases
enum move_flags {
    none,
    k_castling,
    q_castling,
    en_passant,
    double_push,
};

// Represent a move with a class
class Move {
public:
    int move;

    // Origin of moving piece
    int source() const {
        return move & 0x3f;
    }

    // Destination of moving piece
    int target() const {
        return (move >> 6) & 0x3f;
    }

    // Piece type
    int piece() const {
        return (move >> 12) & 0xf;
    }

    // Potential capture
    int capture() const {
        return (move >> 16) & 0xf;
    }

    // Potential promotion
    int promote() const {
        return (move >> 20) & 0xf;
    }

    // Record any special moves
    int flag() const {
        return (move >> 24) & 0x7;
    }

    // Constructor to encode move
    Move (int source, int target, int piece, int capture, int promote, int flag) {
        move = source | (target << 6) | (piece << 12) | (capture << 16) | (promote << 20) | (flag << 24);
    }
};

#endif
