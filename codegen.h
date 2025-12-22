#ifndef CODEGEN_H
#define CODEGEN_H

#include <vector>
#include <cstdint>
#include <map>
#include <string>
#include <functional>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>

// Forward declarations for runtime function types
typedef long long (*JITFunction)(long long* args, int argCount);
typedef void (*PrintIntFunc)(long long value);
typedef void (*PrintBoolFunc)(int value);
typedef void (*PrintStringFunc)(const char* value);
typedef void (*PrintTabFunc)();
typedef void (*PrintNewlineFunc)();
typedef long long (*CallUserFuncFunc)(const char* name, long long* args, int argCount);

// Runtime function pointers - set by JIT compiler
struct RuntimeFunctions {
    PrintIntFunc printInt;
    PrintBoolFunc printBool;
    PrintStringFunc printString;
    PrintTabFunc printTab;
    PrintNewlineFunc printNewline;
    CallUserFuncFunc callUserFunc;
};

// Label for forward references (branches)
struct Label {
    size_t offset;
    bool bound;
    std::vector<size_t> pendingFixups;

    Label() : offset(0), bound(false) {}
};

// Abstract code generator base class
class CodeGenerator {
public:
    virtual ~CodeGenerator() = default;

    // Get generated code
    const std::vector<uint8_t>& getCode() const { return code; }
    size_t size() const { return code.size(); }
    void clear() { code.clear(); labelCounter = 0; }

    // Architecture detection
    static bool isX86_64();
    static bool isARM64();

    // Function prologue/epilogue
    virtual void emitPrologue(int localCount) = 0;
    virtual void emitEpilogue() = 0;

    // Load immediate value into result register
    virtual void emitLoadImmediate(long long value) = 0;

    // Load boolean into result register (0 or 1)
    virtual void emitLoadBool(bool value) = 0;

    // Variable operations (offset is stack slot index)
    virtual void emitLoadLocal(int offset) = 0;
    virtual void emitStoreLocal(int offset) = 0;

    // Load function argument into result register
    virtual void emitLoadArg(int argIndex) = 0;

    // Push/pop result register to stack (for expression evaluation)
    virtual void emitPush() = 0;
    virtual void emitPop() = 0; // Pop into secondary register

    // Binary operations (left in secondary, right in result -> result)
    virtual void emitAdd() = 0;
    virtual void emitSub() = 0;
    virtual void emitMul() = 0;
    virtual void emitDiv() = 0;
    virtual void emitMod() = 0;

    // Comparison operations (result is 0 or 1)
    virtual void emitCompareEq() = 0;
    virtual void emitCompareNe() = 0;
    virtual void emitCompareLt() = 0;
    virtual void emitCompareLe() = 0;
    virtual void emitCompareGt() = 0;
    virtual void emitCompareGe() = 0;

    // Logical operations
    virtual void emitAnd() = 0;
    virtual void emitOr() = 0;
    virtual void emitNot() = 0;
    virtual void emitNeg() = 0;

    // Control flow
    virtual Label createLabel() = 0;
    virtual void bindLabel(Label& label) = 0;
    virtual void emitJump(Label& label) = 0;
    virtual void emitJumpIfFalse(Label& label) = 0;
    virtual void emitJumpIfTrue(Label& label) = 0;

    // Function calls
    virtual void emitCallRuntime(void* funcPtr, int argCount) = 0;
    virtual void emitReturn() = 0;

    // String literal - returns the address where string is stored
    virtual void emitLoadStringPtr(const char* str) = 0;

    // Prepare args for function call
    virtual void emitPrepareCallArgs(int argCount) = 0;
    virtual void emitSetCallArg(int argIndex) = 0;

protected:
    std::vector<uint8_t> code;
    int labelCounter = 0;

    void emit(uint8_t byte) { code.push_back(byte); }
    void emit16(uint16_t value) {
        emit(value & 0xFF);
        emit((value >> 8) & 0xFF);
    }
    void emit32(uint32_t value) {
        emit(value & 0xFF);
        emit((value >> 8) & 0xFF);
        emit((value >> 16) & 0xFF);
        emit((value >> 24) & 0xFF);
    }
    void emit64(uint64_t value) {
        emit32(value & 0xFFFFFFFF);
        emit32((value >> 32) & 0xFFFFFFFF);
    }

    void patch32(size_t offset, int32_t value) {
        code[offset] = value & 0xFF;
        code[offset + 1] = (value >> 8) & 0xFF;
        code[offset + 2] = (value >> 16) & 0xFF;
        code[offset + 3] = (value >> 24) & 0xFF;
    }
};

// x86-64 code generator (System V AMD64 ABI)
class X86_64CodeGen : public CodeGenerator {
public:
    void emitPrologue(int localCount) override;
    void emitEpilogue() override;

    void emitLoadImmediate(long long value) override;
    void emitLoadBool(bool value) override;

    void emitLoadLocal(int offset) override;
    void emitStoreLocal(int offset) override;
    void emitLoadArg(int argIndex) override;

    void emitPush() override;
    void emitPop() override;

    void emitAdd() override;
    void emitSub() override;
    void emitMul() override;
    void emitDiv() override;
    void emitMod() override;

    void emitCompareEq() override;
    void emitCompareNe() override;
    void emitCompareLt() override;
    void emitCompareLe() override;
    void emitCompareGt() override;
    void emitCompareGe() override;

    void emitAnd() override;
    void emitOr() override;
    void emitNot() override;
    void emitNeg() override;

    Label createLabel() override;
    void bindLabel(Label& label) override;
    void emitJump(Label& label) override;
    void emitJumpIfFalse(Label& label) override;
    void emitJumpIfTrue(Label& label) override;

    void emitCallRuntime(void* funcPtr, int argCount) override;
    void emitReturn() override;

    void emitLoadStringPtr(const char* str) override;
    void emitPrepareCallArgs(int argCount) override;
    void emitSetCallArg(int argIndex) override;

private:
    int frameSize = 0;
    int localSlots = 0;

    // REX prefixes
    static constexpr uint8_t REX_W = 0x48;      // 64-bit operand size
    static constexpr uint8_t REX_R = 0x44;      // Extension of ModR/M reg field
    static constexpr uint8_t REX_X = 0x42;      // Extension of SIB index field
    static constexpr uint8_t REX_B = 0x41;      // Extension of ModR/M r/m or SIB base

    void emitMovReg64Imm(int reg, uint64_t imm);
    void emitMovRegReg(int dst, int src);
    void emitMovFromStack(int reg, int offset);
    void emitMovToStack(int offset, int reg);

    // Register encoding
    static constexpr int RAX = 0;
    static constexpr int RCX = 1;
    static constexpr int RDX = 2;
    static constexpr int RBX = 3;
    static constexpr int RSP = 4;
    static constexpr int RBP = 5;
    static constexpr int RSI = 6;
    static constexpr int RDI = 7;
    static constexpr int R8 = 8;
    static constexpr int R9 = 9;
    static constexpr int R10 = 10;
    static constexpr int R11 = 11;
};

// ARM64 code generator (AAPCS64)
class ARM64CodeGen : public CodeGenerator {
public:
    void emitPrologue(int localCount) override;
    void emitEpilogue() override;

    void emitLoadImmediate(long long value) override;
    void emitLoadBool(bool value) override;

    void emitLoadLocal(int offset) override;
    void emitStoreLocal(int offset) override;
    void emitLoadArg(int argIndex) override;

    void emitPush() override;
    void emitPop() override;

    void emitAdd() override;
    void emitSub() override;
    void emitMul() override;
    void emitDiv() override;
    void emitMod() override;

    void emitCompareEq() override;
    void emitCompareNe() override;
    void emitCompareLt() override;
    void emitCompareLe() override;
    void emitCompareGt() override;
    void emitCompareGe() override;

    void emitAnd() override;
    void emitOr() override;
    void emitNot() override;
    void emitNeg() override;

    Label createLabel() override;
    void bindLabel(Label& label) override;
    void emitJump(Label& label) override;
    void emitJumpIfFalse(Label& label) override;
    void emitJumpIfTrue(Label& label) override;

    void emitCallRuntime(void* funcPtr, int argCount) override;
    void emitReturn() override;

    void emitLoadStringPtr(const char* str) override;
    void emitPrepareCallArgs(int argCount) override;
    void emitSetCallArg(int argIndex) override;

private:
    int frameSize = 0;
    int localSlots = 0;

    // ARM64 instruction encoding helpers
    void emitInstruction(uint32_t insn);
    void emitMovImm64(int reg, uint64_t imm);
    void emitLdrOffset(int rt, int rn, int offset);
    void emitStrOffset(int rt, int rn, int offset);

    // Register usage:
    // x0-x7: arguments / return value
    // x9: secondary register for binary ops
    // x19-x28: callee-saved (we use x19 for our temp)
    // x29: frame pointer
    // x30: link register
    static constexpr int X0 = 0;
    static constexpr int X9 = 9;
    static constexpr int X19 = 19;
    static constexpr int X29 = 29;
    static constexpr int X30 = 30;
    static constexpr int SP = 31;
};

// Factory function to create appropriate code generator
inline CodeGenerator* createCodeGenerator() {
#if defined(__x86_64__) || defined(_M_X64)
    return new X86_64CodeGen();
#elif defined(__aarch64__) || defined(_M_ARM64)
    return new ARM64CodeGen();
#else
    #error "Unsupported architecture"
#endif
}

// Inline implementations for architecture detection
inline bool CodeGenerator::isX86_64() {
#if defined(__x86_64__) || defined(_M_X64)
    return true;
#else
    return false;
#endif
}

inline bool CodeGenerator::isARM64() {
#if defined(__aarch64__) || defined(_M_ARM64)
    return true;
#else
    return false;
#endif
}

#endif // CODEGEN_H
