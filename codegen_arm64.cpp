#include "codegen.h"

#if defined(__aarch64__) || defined(_M_ARM64)

// ARM64 Implementation (AAPCS64)
// Result register: X0
// Secondary register: X9 (caller-saved)
// Arg registers: X0-X7
// Callee-saved: X19-X28
// Frame pointer: X29
// Link register: X30
// Stack pointer: SP (X31 context-dependent)
//
// Stack frame layout (growing down):
//   [FP+8] = saved LR (X30)
//   [FP]   = saved FP (X29)
//   [FP-8] = local0
//   [FP-16] = local1
//   ...

void ARM64CodeGen::emitInstruction(uint32_t insn) {
    emit(insn & 0xFF);
    emit((insn >> 8) & 0xFF);
    emit((insn >> 16) & 0xFF);
    emit((insn >> 24) & 0xFF);
}

void ARM64CodeGen::emitPrologue(int localCount) {
    localSlots = localCount;

    // Calculate frame size (locals + saved registers, 16-byte aligned)
    frameSize = ((localCount * 8 + 32) + 15) & ~15;

    // stp x29, x30, [sp, #-frameSize]!  ; pre-index store pair, allocate frame
    uint32_t stp_offset = ((-frameSize) >> 3) & 0x7F;
    emitInstruction(0xA9800000 | (stp_offset << 15) | (30 << 10) | (31 << 5) | 29);

    // mov x29, sp  ; set frame pointer
    emitInstruction(0x910003FD);

    // Save x19 (we use it as temp)
    // str x19, [sp, #16]
    emitInstruction(0xF9000800 | (31 << 5) | 19);

    // Save arg pointer (x0 = args array) to x19
    // mov x19, x0
    emitInstruction(0xAA0003F3);

    // Initialize locals to 0
    for (int i = 0; i < localCount; i++) {
        // str xzr, [x29, #-(8*(i+1))]
        int offset = -(8 * (i + 1));
        // Need to use stur for negative offsets
        emitInstruction(0xF8000000 | ((offset & 0x1FF) << 12) | (29 << 5) | 31);
    }
}

void ARM64CodeGen::emitEpilogue() {
    // Restore x19
    // ldr x19, [sp, #16]
    emitInstruction(0xF9400800 | (31 << 5) | 19);

    // ldp x29, x30, [sp], #frameSize  ; post-index load pair, deallocate frame
    uint32_t ldp_offset = (frameSize >> 3) & 0x7F;
    emitInstruction(0xA8C00000 | (ldp_offset << 15) | (30 << 10) | (31 << 5) | 29);

    // ret
    emitInstruction(0xD65F03C0);
}

void ARM64CodeGen::emitMovImm64(int reg, uint64_t imm) {
    // movz reg, #imm16, lsl #0
    emitInstruction(0xD2800000 | ((imm & 0xFFFF) << 5) | reg);

    if (imm > 0xFFFF) {
        // movk reg, #imm16, lsl #16
        emitInstruction(0xF2A00000 | (((imm >> 16) & 0xFFFF) << 5) | reg);
    }
    if (imm > 0xFFFFFFFF) {
        // movk reg, #imm16, lsl #32
        emitInstruction(0xF2C00000 | (((imm >> 32) & 0xFFFF) << 5) | reg);
    }
    if (imm > 0xFFFFFFFFFFFF) {
        // movk reg, #imm16, lsl #48
        emitInstruction(0xF2E00000 | (((imm >> 48) & 0xFFFF) << 5) | reg);
    }
}

void ARM64CodeGen::emitLdrOffset(int rt, int rn, int offset) {
    if (offset >= 0 && offset < 32768 && (offset & 7) == 0) {
        // ldr rt, [rn, #offset] - scaled offset
        uint32_t imm12 = offset >> 3;
        emitInstruction(0xF9400000 | (imm12 << 10) | (rn << 5) | rt);
    } else {
        // ldur rt, [rn, #offset] - unscaled offset
        emitInstruction(0xF8400000 | ((offset & 0x1FF) << 12) | (rn << 5) | rt);
    }
}

void ARM64CodeGen::emitStrOffset(int rt, int rn, int offset) {
    if (offset >= 0 && offset < 32768 && (offset & 7) == 0) {
        // str rt, [rn, #offset] - scaled offset
        uint32_t imm12 = offset >> 3;
        emitInstruction(0xF9000000 | (imm12 << 10) | (rn << 5) | rt);
    } else {
        // stur rt, [rn, #offset] - unscaled offset
        emitInstruction(0xF8000000 | ((offset & 0x1FF) << 12) | (rn << 5) | rt);
    }
}

void ARM64CodeGen::emitLoadImmediate(long long value) {
    if (value >= 0 && value <= 0xFFFF) {
        // movz x0, #value
        emitInstruction(0xD2800000 | ((value & 0xFFFF) << 5) | X0);
    } else if (value < 0 && value >= -0x10000) {
        // movn x0, #~value
        uint16_t notVal = ~value;
        emitInstruction(0x92800000 | (notVal << 5) | X0);
    } else {
        emitMovImm64(X0, (uint64_t)value);
    }
}

void ARM64CodeGen::emitLoadBool(bool value) {
    // movz x0, #0 or #1
    emitInstruction(0xD2800000 | ((value ? 1 : 0) << 5) | X0);
}

void ARM64CodeGen::emitLoadLocal(int offset) {
    // ldr x0, [x29, #-(8*(offset+1))]
    int stackOffset = -(8 * (offset + 1));
    emitLdrOffset(X0, X29, stackOffset);
}

void ARM64CodeGen::emitStoreLocal(int offset) {
    // str x0, [x29, #-(8*(offset+1))]
    int stackOffset = -(8 * (offset + 1));
    emitStrOffset(X0, X29, stackOffset);
}

void ARM64CodeGen::emitLoadArg(int argIndex) {
    // Args passed as array in x19
    // ldr x0, [x19, #argIndex*8]
    int offset = argIndex * 8;
    emitLdrOffset(X0, X19, offset);
}

void ARM64CodeGen::emitPush() {
    // str x0, [sp, #-16]!
    emitInstruction(0xF81F0FE0);
}

void ARM64CodeGen::emitPop() {
    // ldr x9, [sp], #16
    emitInstruction(0xF8410FE9);
}

void ARM64CodeGen::emitAdd() {
    // add x0, x9, x0
    emitInstruction(0x8B000120);
}

void ARM64CodeGen::emitSub() {
    // sub x0, x9, x0
    emitInstruction(0xCB000120);
}

void ARM64CodeGen::emitMul() {
    // mul x0, x9, x0
    emitInstruction(0x9B007D20);
}

void ARM64CodeGen::emitDiv() {
    // sdiv x0, x9, x0
    emitInstruction(0x9AC00D20);
}

void ARM64CodeGen::emitMod() {
    // sdiv x10, x9, x0
    emitInstruction(0x9AC00D2A);
    // msub x0, x10, x0, x9  ; x0 = x9 - x10 * x0
    emitInstruction(0x9B008140);
}

void ARM64CodeGen::emitCompareEq() {
    // cmp x9, x0
    emitInstruction(0xEB00013F);
    // cset x0, eq
    emitInstruction(0x9A9F17E0);
}

void ARM64CodeGen::emitCompareNe() {
    // cmp x9, x0
    emitInstruction(0xEB00013F);
    // cset x0, ne
    emitInstruction(0x9A9F07E0);
}

void ARM64CodeGen::emitCompareLt() {
    // cmp x9, x0
    emitInstruction(0xEB00013F);
    // cset x0, lt
    emitInstruction(0x9A9FA7E0);
}

void ARM64CodeGen::emitCompareLe() {
    // cmp x9, x0
    emitInstruction(0xEB00013F);
    // cset x0, le
    emitInstruction(0x9A9FC7E0);
}

void ARM64CodeGen::emitCompareGt() {
    // cmp x9, x0
    emitInstruction(0xEB00013F);
    // cset x0, gt
    emitInstruction(0x9A9FC7E0 ^ 0x1000);  // Flip condition
}

void ARM64CodeGen::emitCompareGe() {
    // cmp x9, x0
    emitInstruction(0xEB00013F);
    // cset x0, ge
    emitInstruction(0x9A9FA7E0 ^ 0x1000);  // Flip condition
}

void ARM64CodeGen::emitAnd() {
    // cmp x9, #0
    emitInstruction(0xF100013F);
    // cset x10, ne
    emitInstruction(0x9A9F07EA);
    // cmp x0, #0
    emitInstruction(0xF100001F);
    // cset x0, ne
    emitInstruction(0x9A9F07E0);
    // and x0, x10, x0
    emitInstruction(0x8A000140);
}

void ARM64CodeGen::emitOr() {
    // cmp x9, #0
    emitInstruction(0xF100013F);
    // cset x10, ne
    emitInstruction(0x9A9F07EA);
    // cmp x0, #0
    emitInstruction(0xF100001F);
    // cset x0, ne
    emitInstruction(0x9A9F07E0);
    // orr x0, x10, x0
    emitInstruction(0xAA000140);
}

void ARM64CodeGen::emitNot() {
    // cmp x0, #0
    emitInstruction(0xF100001F);
    // cset x0, eq
    emitInstruction(0x9A9F17E0);
}

void ARM64CodeGen::emitNeg() {
    // neg x0, x0
    emitInstruction(0xCB0003E0);
}

Label ARM64CodeGen::createLabel() {
    Label l;
    l.bound = false;
    l.offset = 0;
    return l;
}

void ARM64CodeGen::bindLabel(Label& label) {
    label.offset = code.size();
    label.bound = true;

    // Fix up pending references
    for (size_t fixupOffset : label.pendingFixups) {
        int32_t rel = (int32_t)(label.offset - fixupOffset) >> 2;
        // Read existing instruction
        uint32_t insn = code[fixupOffset] |
                       (code[fixupOffset + 1] << 8) |
                       (code[fixupOffset + 2] << 16) |
                       (code[fixupOffset + 3] << 24);

        if ((insn & 0xFC000000) == 0x14000000) {
            // Unconditional branch (B)
            insn = (insn & 0xFC000000) | (rel & 0x3FFFFFF);
        } else if ((insn & 0xFF000010) == 0x54000000) {
            // Conditional branch (B.cond)
            insn = (insn & 0xFF00001F) | ((rel & 0x7FFFF) << 5);
        }

        code[fixupOffset] = insn & 0xFF;
        code[fixupOffset + 1] = (insn >> 8) & 0xFF;
        code[fixupOffset + 2] = (insn >> 16) & 0xFF;
        code[fixupOffset + 3] = (insn >> 24) & 0xFF;
    }
    label.pendingFixups.clear();
}

void ARM64CodeGen::emitJump(Label& label) {
    // b label
    if (label.bound) {
        int32_t rel = (int32_t)(label.offset - code.size()) >> 2;
        emitInstruction(0x14000000 | (rel & 0x3FFFFFF));
    } else {
        label.pendingFixups.push_back(code.size());
        emitInstruction(0x14000000); // placeholder
    }
}

void ARM64CodeGen::emitJumpIfFalse(Label& label) {
    // cbz x0, label  (branch if x0 == 0)
    if (label.bound) {
        int32_t rel = (int32_t)(label.offset - code.size()) >> 2;
        emitInstruction(0xB4000000 | ((rel & 0x7FFFF) << 5) | X0);
    } else {
        label.pendingFixups.push_back(code.size());
        emitInstruction(0xB4000000 | X0); // placeholder
    }
}

void ARM64CodeGen::emitJumpIfTrue(Label& label) {
    // cbnz x0, label  (branch if x0 != 0)
    if (label.bound) {
        int32_t rel = (int32_t)(label.offset - code.size()) >> 2;
        emitInstruction(0xB5000000 | ((rel & 0x7FFFF) << 5) | X0);
    } else {
        label.pendingFixups.push_back(code.size());
        emitInstruction(0xB5000000 | X0); // placeholder
    }
}

void ARM64CodeGen::emitCallRuntime(void* funcPtr, int argCount) {
    // Load function pointer into x10
    emitMovImm64(10, (uint64_t)funcPtr);
    // blr x10
    emitInstruction(0xD63F0140);
}

void ARM64CodeGen::emitReturn() {
    emitEpilogue();
}

void ARM64CodeGen::emitLoadStringPtr(const char* str) {
    emitMovImm64(X0, (uint64_t)str);
}

void ARM64CodeGen::emitPrepareCallArgs(int argCount) {
    // Args are set via emitSetCallArg
}

void ARM64CodeGen::emitSetCallArg(int argIndex) {
    if (argIndex < 8) {
        // mov xN, x0
        if (argIndex != 0) {
            emitInstruction(0xAA0003E0 | argIndex);
        }
    }
}

#endif // aarch64
