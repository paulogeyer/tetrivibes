#pragma once

// Shared TetriNET constants and cell/special encodings.
// Classic fields are 12x22. Specials are letters on locked blocks (a,c,b,r,o,q,g,s,n).

#include <array>
#include <cstdint>

namespace tnet {

constexpr int kFieldWidth = 12;
constexpr int kFieldHeight = 22;
constexpr int kMaxPlayers = 6;
constexpr int kMaxInventory = 18;
constexpr int kPieceSize = 4;

enum class Cell : uint8_t {
    Empty = 0,
    C1,
    C2,
    C3,
    C4,
    C5,
    AddLine,
    ClearLine,
    ClearSpecial,
    RandomClear,
    Bomb,
    Quake,
    Gravity,
    SwitchField,
    Nuke,
    LeftGravity,
    PieceChange,
    Zebra
};

enum class PieceKind : uint8_t { I, O, T, S, Z, J, L, Count };

enum class Special : uint8_t {
    AddLine,
    ClearLine,
    ClearSpecial,
    RandomClear,
    Bomb,
    Quake,
    Gravity,
    SwitchField,
    Nuke,
    LeftGravity,
    PieceChange,
    Zebra
};

enum class JoinProtocol : uint8_t { Auto, Native, Tetrinet113, TetriFast };

inline bool isSpecial(Cell c)
{
    return c >= Cell::AddLine;
}

inline bool isFilled(Cell c)
{
    return c != Cell::Empty;
}

inline Cell specialToCell(Special s)
{
    return static_cast<Cell>(static_cast<uint8_t>(Cell::AddLine) + static_cast<uint8_t>(s));
}

inline Special cellToSpecial(Cell c)
{
    return static_cast<Special>(static_cast<uint8_t>(c) - static_cast<uint8_t>(Cell::AddLine));
}

inline char cellToChar(Cell c)
{
    switch (c) {
    case Cell::Empty: return '0';
    case Cell::C1: return '1';
    case Cell::C2: return '2';
    case Cell::C3: return '3';
    case Cell::C4: return '4';
    case Cell::C5: return '5';
    case Cell::AddLine: return 'a';
    case Cell::ClearLine: return 'c';
    case Cell::ClearSpecial: return 'b';
    case Cell::RandomClear: return 'r';
    case Cell::Bomb: return 'o';
    case Cell::Quake: return 'q';
    case Cell::Gravity: return 'g';
    case Cell::SwitchField: return 's';
    case Cell::Nuke: return 'n';
    case Cell::LeftGravity: return 'l';
    case Cell::PieceChange: return 'p';
    case Cell::Zebra: return 'z';
    }
    return '0';
}

inline Cell charToCell(char ch)
{
    switch (ch) {
    case '1': return Cell::C1;
    case '2': return Cell::C2;
    case '3': return Cell::C3;
    case '4': return Cell::C4;
    case '5': return Cell::C5;
    case 'a': return Cell::AddLine;
    case 'c': return Cell::ClearLine;
    case 'b': return Cell::ClearSpecial;
    case 'r': return Cell::RandomClear;
    case 'o': return Cell::Bomb;
    case 'q': return Cell::Quake;
    case 'g': return Cell::Gravity;
    case 's': return Cell::SwitchField;
    case 'n': return Cell::Nuke;
    case 'l': return Cell::LeftGravity;
    case 'p': return Cell::PieceChange;
    case 'z': return Cell::Zebra;
    default: return Cell::Empty;
    }
}

inline char specialLetter(Special s)
{
    return cellToChar(specialToCell(s));
}

inline const char *specialName(Special s)
{
    switch (s) {
    case Special::AddLine: return "Add Line";
    case Special::ClearLine: return "Clear Line";
    case Special::ClearSpecial: return "Clear Specials";
    case Special::RandomClear: return "Random Clear";
    case Special::Bomb: return "Block Bomb";
    case Special::Quake: return "Blockquake";
    case Special::Gravity: return "Gravity";
    case Special::SwitchField: return "Switch Field";
    case Special::Nuke: return "Nuke Field";
    case Special::LeftGravity: return "Left Gravity";
    case Special::PieceChange: return "Piece Change";
    case Special::Zebra: return "Zebra Field";
    }
    return "?";
}

} // namespace tnet
