#include "roblox_encoder.h"

// Reconstructed from sub_1801208A0 in Potassium.dll:
// Returns the number of 4-byte instruction slots consumed by the given Luau opcode.
int RobloxEncoder::getInstructionSlotCount(uint8_t op) {
    switch (op) {
        case 7:  // LOP_LOADKX
        case 8:  // LOP_JUMPX
        case 12: // LOP_FASTCALL
        case 15: // LOP_GETGLOBAL
        case 16: // LOP_SETGLOBAL
        case 20: // LOP_GETIMPORT
        case 27: // LOP_GETTABLEKS
        case 28: // LOP_SETTABLEKS
        case 29: // LOP_GETTABLER
        case 30: // LOP_SETTABLER
        case 31: // LOP_NEWCLOSURE
        case 32: // LOP_NAMECALL
        case 53: // LOP_JUMPIFEQ
        case 55: // LOP_JUMPIFNOTEQ
        case 58: // LOP_JUMPIFLE
        case 60: // LOP_JUMPIFNOTLE
        case 66: // LOP_JUMPIFNOT
        case 74: // LOP_JUMPIFEQK
        case 75: // LOP_JUMPIFNOTEQK
        case 77: // LOP_FASTCALL1
        case 78: // LOP_FASTCALL2
        case 79: // LOP_FASTCALL2K
        case 80: // LOP_FORGPREP_NEXT
        case 83: // LOP_FORGPREP_INEXT
        case 84: // LOP_FORGPREP_SLOT
        case 85: // LOP_CAPTURE
        case 86: // LOP_SUBRK
        case 87: // LOP_MULRK
        case 88: // LOP_DIVRK
        case 90: // LOP_MODRK
        case 91: // LOP_POWRK
        case 93: // LOP_ANDRK
        case 94: // LOP_ORRK
            return 2; // 2 slots (8 bytes)
        default:
            return 1; // 1 slot (4 bytes)
    }
}
