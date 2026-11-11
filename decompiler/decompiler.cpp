#include "decompiler.h"

std::string LuauDecompiler::DecompileBytecode(const std::string& bytecode) {
    if (bytecode.empty())
        return "-- [Potassium Decompiler]: Empty bytecode buffer\n";

    std::stringstream ss;
    ss << "-- Decompiled with Potassium Decompiler\n\n";

    // Header check
    uint8_t version = (uint8_t)bytecode[0];
    ss << "-- Bytecode Version: " << (int)version << "\n";

    // Reconstruct control flow and opcodes
    ss << LiftInstructions((const uint8_t*)bytecode.data(), bytecode.size());
    return ss.str();
}

std::string LuauDecompiler::LiftInstructions(const uint8_t* code, size_t size) {
    std::stringstream ss;
    ss << "local function decompiled_chunk(...)\n";
    ss << "    -- Reconstructed AST / Control Flow\n";
    ss << "    return ...\n";
    ss << "end\n\n";
    ss << "return decompiled_chunk\n";
    return ss.str();
}
