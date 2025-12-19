#include "jit.h"
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>
#include <iostream>

JITCompiler::JITCompiler(Interpreter* interp) : interpreter(interp), stackOffset(0) {}

JITCompiler::~JITCompiler() {
    for (void* page : allocatedPages) {
        munmap(page, 4096);
    }
}

void* JITCompiler::allocateExecutableMemory(size_t size) {
    size_t pageSize = (size + 4095) & ~4095;
    void* ptr = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
        throw std::runtime_error("Failed to allocate executable memory");
    }
    allocatedPages.push_back(ptr);
    return ptr;
}

void JITCompiler::freeExecutableMemory(void* ptr, size_t size) {
    munmap(ptr, size);
}

void JITCompiler::emit(uint8_t byte) {
    code.push_back(byte);
}

void JITCompiler::emit32(uint32_t value) {
    emit(value & 0xFF);
    emit((value >> 8) & 0xFF);
    emit((value >> 16) & 0xFF);
    emit((value >> 24) & 0xFF);
}

void JITCompiler::emit64(uint64_t value) {
    emit32(value & 0xFFFFFFFF);
    emit32((value >> 32) & 0xFFFFFFFF);
}

void JITCompiler::emitPrologue() {
    emit(0x55);
    emit(0x48); emit(0x89); emit(0xE5);
}

void JITCompiler::emitEpilogue() {
    emit(0x5D);
    emit(0xC3);
}

void JITCompiler::compileExpression(ASTNode* node) {
    if (!node) return;

    switch (node->type) {
        case ASTNodeType::INTEGER: {
            IntegerNode* intNode = static_cast<IntegerNode*>(node);
            emit(0x48); emit(0xB8);
            emit64(intNode->value);
            break;
        }
        case ASTNodeType::BINARY_OP: {
            BinaryOpNode* binOp = static_cast<BinaryOpNode*>(node);
            compileExpression(binOp->left.get());
            emit(0x50);
            compileExpression(binOp->right.get());
            emit(0x48); emit(0x89); emit(0xC3);
            emit(0x58);

            switch (binOp->op) {
                case BinaryOpType::ADD:
                    emit(0x48); emit(0x01); emit(0xD8);
                    break;
                case BinaryOpType::SUB:
                    emit(0x48); emit(0x29); emit(0xD8);
                    break;
                case BinaryOpType::MUL:
                    emit(0x48); emit(0x0F); emit(0xAF); emit(0xC3);
                    break;
                case BinaryOpType::DIV:
                    emit(0x48); emit(0x99);
                    emit(0x48); emit(0xF7); emit(0xFB);
                    break;
                case BinaryOpType::MOD:
                    emit(0x48); emit(0x99);
                    emit(0x48); emit(0xF7); emit(0xFB);
                    emit(0x48); emit(0x89); emit(0xD0);
                    break;
                default:
                    break;
            }
            break;
        }
        default:
            break;
    }
}

void JITCompiler::compileStatement(ASTNode* stmt) {
}

void JITCompiler::compileBlock(BlockNode* block) {
    if (!block) return;
    for (auto& stmt : block->statements) {
        compileStatement(stmt.get());
    }
}

void* JITCompiler::compileFunction(FunctionDefNode* func) {
    code.clear();
    localVars.clear();
    stackOffset = 0;

    emitPrologue();

    if (func->body) {
        compileExpression(func->body.get());
    } else {
        emit(0x48); emit(0x31); emit(0xC0);
    }

    emitEpilogue();

    void* execMem = allocateExecutableMemory(code.size());
    memcpy(execMem, code.data(), code.size());

    return execMem;
}

void JITCompiler::executeJIT(BlockNode* root) {
    if (!root) return;

    for (auto& stmt : root->statements) {
        if (stmt->type == ASTNodeType::FUNCTION_DEF) {
            FunctionDefNode* funcDef = static_cast<FunctionDefNode*>(stmt.get());
            void* compiledFunc = compileFunction(funcDef);
            compiledFunctions[funcDef->name] = compiledFunc;
            interpreter->functions[funcDef->name] = funcDef;
        } else {
            interpreter->executeStatement(stmt.get());
        }
    }
}
