#include "codegen.h"

#if defined(__x86_64__) || defined(_M_X64)

// x86-64 Implementation (System V AMD64 ABI)
// Result register: RAX
// Secondary register: RBX (callee-saved, preserved across calls)
// Arg registers: RDI, RSI, RDX, RCX, R8, R9
// Stack grows downward
// Frame: [RBP+8]=return addr, [RBP]=old RBP, [RBP-8]=local0, [RBP-16]=local1, etc.

void X86_64CodeGen::emitPrologue(int localCount) {
    localSlots = localCount;

    // push rbp
    emit(0x55);

    // mov rbp, rsp
    emit(REX_W); emit(0x89); emit(0xE5);

    // push rbx (callee-saved, we use it)
    emit(0x53);

    // push r12 (callee-saved, for arg array pointer)
    emit(REX_B); emit(0x54);

    // Save arg pointer (rdi = args array) to r12
    // mov r12, rdi
    emit(REX_W | 0x01); emit(0x89); emit(0xFC);

    // Allocate space for locals (aligned to 16 bytes)
    frameSize = ((localCount * 8) + 15) & ~15;
    if (frameSize > 0) {
        if (frameSize <= 127) {
            // sub rsp, imm8
            emit(REX_W); emit(0x83); emit(0xEC); emit(frameSize);
        } else {
            // sub rsp, imm32
            emit(REX_W); emit(0x81); emit(0xEC); emit32(frameSize);
        }
    }

    // Initialize locals to 0
    for (int i = 0; i < localCount; i++) {
        // mov qword [rbp - offset], 0
        emit(REX_W); emit(0xC7); emit(0x45);
        emit((uint8_t)(-(16 + (i + 1) * 8))); // offset from rbp
        emit32(0);
    }
}

void X86_64CodeGen::emitEpilogue() {
    // mov rsp, rbp ; sub rsp, 16 (for saved rbx and r12)
    emit(REX_W); emit(0x8D); emit(0x65); emit(0xF0); // lea rsp, [rbp-16]

    // pop r12
    emit(REX_B); emit(0x5C);

    // pop rbx
    emit(0x5B);

    // pop rbp
    emit(0x5D);

    // ret
    emit(0xC3);
}

void X86_64CodeGen::emitLoadImmediate(long long value) {
    emitMovReg64Imm(RAX, value);
}

void X86_64CodeGen::emitLoadBool(bool value) {
    emitMovReg64Imm(RAX, value ? 1 : 0);
}

void X86_64CodeGen::emitLoadLocal(int offset) {
    // mov rax, [rbp - (16 + (offset+1)*8)]
    int stackOffset = -(16 + (offset + 1) * 8);
    emitMovFromStack(RAX, stackOffset);
}

void X86_64CodeGen::emitStoreLocal(int offset) {
    // mov [rbp - (16 + (offset+1)*8)], rax
    int stackOffset = -(16 + (offset + 1) * 8);
    emitMovToStack(stackOffset, RAX);
}

void X86_64CodeGen::emitLoadArg(int argIndex) {
    // Args passed as array: mov rax, [r12 + argIndex*8]
    // REX.W + 8B /r : mov r64, r/m64
    // ModRM: 01 000 100 (mod=01, reg=rax, rm=r12 via SIB)
    // SIB: 00 100 100 (scale=1, index=none, base=r12)
    emit(REX_W | 0x01); emit(0x8B); emit(0x44); emit(0x24);
    emit((uint8_t)(argIndex * 8));
}

void X86_64CodeGen::emitPush() {
    // push rax
    emit(0x50);
}

void X86_64CodeGen::emitPop() {
    // pop rbx (secondary register)
    emit(0x5B);
}

void X86_64CodeGen::emitAdd() {
    // add rax, rbx
    emit(REX_W); emit(0x01); emit(0xD8);
}

void X86_64CodeGen::emitSub() {
    // sub rbx, rax (left - right, left in rbx, right in rax)
    emit(REX_W); emit(0x29); emit(0xC3);
    // mov rax, rbx
    emit(REX_W); emit(0x89); emit(0xD8);
}

void X86_64CodeGen::emitMul() {
    // imul rax, rbx
    emit(REX_W); emit(0x0F); emit(0xAF); emit(0xC3);
}

void X86_64CodeGen::emitDiv() {
    // rbx / rax -> we want rbx in rax, then divide by old rax
    // mov rcx, rax
    emit(REX_W); emit(0x89); emit(0xC1);
    // mov rax, rbx
    emit(REX_W); emit(0x89); emit(0xD8);
    // cqo (sign extend rax into rdx:rax)
    emit(REX_W); emit(0x99);
    // idiv rcx
    emit(REX_W); emit(0xF7); emit(0xF9);
}

void X86_64CodeGen::emitMod() {
    // rbx % rax
    // mov rcx, rax
    emit(REX_W); emit(0x89); emit(0xC1);
    // mov rax, rbx
    emit(REX_W); emit(0x89); emit(0xD8);
    // cqo
    emit(REX_W); emit(0x99);
    // idiv rcx
    emit(REX_W); emit(0xF7); emit(0xF9);
    // mov rax, rdx (remainder is in rdx)
    emit(REX_W); emit(0x89); emit(0xD0);
}

void X86_64CodeGen::emitCompareEq() {
    // cmp rbx, rax
    emit(REX_W); emit(0x39); emit(0xC3);
    // sete al
    emit(0x0F); emit(0x94); emit(0xC0);
    // movzx rax, al
    emit(REX_W); emit(0x0F); emit(0xB6); emit(0xC0);
}

void X86_64CodeGen::emitCompareNe() {
    // cmp rbx, rax
    emit(REX_W); emit(0x39); emit(0xC3);
    // setne al
    emit(0x0F); emit(0x95); emit(0xC0);
    // movzx rax, al
    emit(REX_W); emit(0x0F); emit(0xB6); emit(0xC0);
}

void X86_64CodeGen::emitCompareLt() {
    // cmp rbx, rax
    emit(REX_W); emit(0x39); emit(0xC3);
    // setl al
    emit(0x0F); emit(0x9C); emit(0xC0);
    // movzx rax, al
    emit(REX_W); emit(0x0F); emit(0xB6); emit(0xC0);
}

void X86_64CodeGen::emitCompareLe() {
    // cmp rbx, rax
    emit(REX_W); emit(0x39); emit(0xC3);
    // setle al
    emit(0x0F); emit(0x9E); emit(0xC0);
    // movzx rax, al
    emit(REX_W); emit(0x0F); emit(0xB6); emit(0xC0);
}

void X86_64CodeGen::emitCompareGt() {
    // cmp rbx, rax
    emit(REX_W); emit(0x39); emit(0xC3);
    // setg al
    emit(0x0F); emit(0x9F); emit(0xC0);
    // movzx rax, al
    emit(REX_W); emit(0x0F); emit(0xB6); emit(0xC0);
}

void X86_64CodeGen::emitCompareGe() {
    // cmp rbx, rax
    emit(REX_W); emit(0x39); emit(0xC3);
    // setge al
    emit(0x0F); emit(0x9D); emit(0xC0);
    // movzx rax, al
    emit(REX_W); emit(0x0F); emit(0xB6); emit(0xC0);
}

void X86_64CodeGen::emitAnd() {
    // Convert both to boolean, then AND
    // test rbx, rbx; setne cl
    emit(REX_W); emit(0x85); emit(0xDB);
    emit(0x0F); emit(0x95); emit(0xC1);
    // test rax, rax; setne al
    emit(REX_W); emit(0x85); emit(0xC0);
    emit(0x0F); emit(0x95); emit(0xC0);
    // and al, cl
    emit(0x20); emit(0xC8);
    // movzx rax, al
    emit(REX_W); emit(0x0F); emit(0xB6); emit(0xC0);
}

void X86_64CodeGen::emitOr() {
    // Convert both to boolean, then OR
    // test rbx, rbx; setne cl
    emit(REX_W); emit(0x85); emit(0xDB);
    emit(0x0F); emit(0x95); emit(0xC1);
    // test rax, rax; setne al
    emit(REX_W); emit(0x85); emit(0xC0);
    emit(0x0F); emit(0x95); emit(0xC0);
    // or al, cl
    emit(0x08); emit(0xC8);
    // movzx rax, al
    emit(REX_W); emit(0x0F); emit(0xB6); emit(0xC0);
}

void X86_64CodeGen::emitNot() {
    // test rax, rax
    emit(REX_W); emit(0x85); emit(0xC0);
    // sete al
    emit(0x0F); emit(0x94); emit(0xC0);
    // movzx rax, al
    emit(REX_W); emit(0x0F); emit(0xB6); emit(0xC0);
}

void X86_64CodeGen::emitNeg() {
    // neg rax
    emit(REX_W); emit(0xF7); emit(0xD8);
}

Label X86_64CodeGen::createLabel() {
    Label l;
    l.bound = false;
    l.offset = 0;
    return l;
}

void X86_64CodeGen::bindLabel(Label& label) {
    label.offset = code.size();
    label.bound = true;

    // Fix up all pending references
    for (size_t fixupOffset : label.pendingFixups) {
        int32_t rel = (int32_t)(label.offset - (fixupOffset + 4));
        patch32(fixupOffset, rel);
    }
    label.pendingFixups.clear();
}

void X86_64CodeGen::emitJump(Label& label) {
    // jmp rel32
    emit(0xE9);
    if (label.bound) {
        int32_t rel = (int32_t)(label.offset - (code.size() + 4));
        emit32(rel);
    } else {
        label.pendingFixups.push_back(code.size());
        emit32(0); // placeholder
    }
}

void X86_64CodeGen::emitJumpIfFalse(Label& label) {
    // test rax, rax
    emit(REX_W); emit(0x85); emit(0xC0);
    // jz rel32
    emit(0x0F); emit(0x84);
    if (label.bound) {
        int32_t rel = (int32_t)(label.offset - (code.size() + 4));
        emit32(rel);
    } else {
        label.pendingFixups.push_back(code.size());
        emit32(0); // placeholder
    }
}

void X86_64CodeGen::emitJumpIfTrue(Label& label) {
    // test rax, rax
    emit(REX_W); emit(0x85); emit(0xC0);
    // jnz rel32
    emit(0x0F); emit(0x85);
    if (label.bound) {
        int32_t rel = (int32_t)(label.offset - (code.size() + 4));
        emit32(rel);
    } else {
        label.pendingFixups.push_back(code.size());
        emit32(0); // placeholder
    }
}

void X86_64CodeGen::emitCallRuntime(void* funcPtr, int argCount) {
    // Call function at absolute address
    // mov r11, funcPtr
    emit(REX_W | REX_B); emit(0xB8 + (R11 - 8)); emit64((uint64_t)funcPtr);
    // call r11
    emit(0x41); emit(0xFF); emit(0xD3);
}

void X86_64CodeGen::emitReturn() {
    emitEpilogue();
}

void X86_64CodeGen::emitLoadStringPtr(const char* str) {
    // Load string pointer directly (string is in static memory)
    emitMovReg64Imm(RAX, (uint64_t)str);
}

void X86_64CodeGen::emitPrepareCallArgs(int argCount) {
    // Args are already set via emitSetCallArg
}

void X86_64CodeGen::emitSetCallArg(int argIndex) {
    // Move rax to the appropriate argument register
    static const int argRegs[] = {RDI, RSI, RDX, RCX, R8, R9};
    if (argIndex < 6) {
        int reg = argRegs[argIndex];
        if (reg < 8) {
            // mov reg, rax
            emit(REX_W); emit(0x89); emit(0xC0 | reg);
        } else {
            // mov r8/r9, rax
            emit(REX_W | REX_B); emit(0x89); emit(0xC0 | (reg - 8));
        }
    }
}

// Helper methods
void X86_64CodeGen::emitMovReg64Imm(int reg, uint64_t imm) {
    if (imm == 0) {
        // xor eax, eax (shorter, also clears upper 32 bits)
        if (reg < 8) {
            emit(0x31); emit(0xC0 | (reg << 3) | reg);
        } else {
            emit(0x45); emit(0x31); emit(0xC0 | ((reg-8) << 3) | (reg-8));
        }
    } else if (imm <= 0xFFFFFFFF) {
        // mov eax, imm32 (zero-extends to 64-bit)
        if (reg < 8) {
            emit(0xB8 + reg);
        } else {
            emit(REX_B); emit(0xB8 + (reg - 8));
        }
        emit32((uint32_t)imm);
    } else {
        // mov rax, imm64
        if (reg < 8) {
            emit(REX_W); emit(0xB8 + reg);
        } else {
            emit(REX_W | REX_B); emit(0xB8 + (reg - 8));
        }
        emit64(imm);
    }
}

void X86_64CodeGen::emitMovRegReg(int dst, int src) {
    uint8_t rex = REX_W;
    if (dst >= 8) rex |= REX_B;
    if (src >= 8) rex |= REX_R;
    emit(rex);
    emit(0x89);
    emit(0xC0 | ((src & 7) << 3) | (dst & 7));
}

void X86_64CodeGen::emitMovFromStack(int reg, int offset) {
    // mov reg, [rbp + offset]
    uint8_t rex = REX_W;
    if (reg >= 8) rex |= REX_R;
    emit(rex);
    emit(0x8B);

    if (offset >= -128 && offset <= 127) {
        emit(0x45 | ((reg & 7) << 3)); // ModRM: [rbp + disp8]
        emit((int8_t)offset);
    } else {
        emit(0x85 | ((reg & 7) << 3)); // ModRM: [rbp + disp32]
        emit32(offset);
    }
}

void X86_64CodeGen::emitMovToStack(int offset, int reg) {
    // mov [rbp + offset], reg
    uint8_t rex = REX_W;
    if (reg >= 8) rex |= REX_R;
    emit(rex);
    emit(0x89);

    if (offset >= -128 && offset <= 127) {
        emit(0x45 | ((reg & 7) << 3)); // ModRM: [rbp + disp8]
        emit((int8_t)offset);
    } else {
        emit(0x85 | ((reg & 7) << 3)); // ModRM: [rbp + disp32]
        emit32(offset);
    }
}

#endif // x86_64
