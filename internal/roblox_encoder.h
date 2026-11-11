#pragma once
#include <cstdint>
#include <cstddef>

namespace Luau {
    class BytecodeEncoder {
    public:
        virtual ~BytecodeEncoder() = default;
        virtual uint8_t encodeOp(uint8_t op) = 0;
    };
}

class RobloxEncoder : public Luau::BytecodeEncoder {
public:
    RobloxEncoder() = default;
    virtual ~RobloxEncoder() override = default;

    // Roblox Luau opcode multiplication: op * 227 (which is -29 & 0xFF)
    virtual uint8_t encodeOp(uint8_t op) override {
        return static_cast<uint8_t>(op * 227);
    }

    // Helper for multislot instructions identified in sub_1801208A0
    static int getInstructionSlotCount(uint8_t op);
};
