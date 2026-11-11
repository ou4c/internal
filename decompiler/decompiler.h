#pragma once
#include "common.h"

class LuauDecompiler {
public:
    static std::string DecompileBytecode(const std::string& bytecode);

private:
    static std::string LiftInstructions(const uint8_t* code, size_t size);
};
