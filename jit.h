#ifndef JIT_H
#define JIT_H

#include "ast.h"
#include "interpreter.h"
#include <vector>
#include <map>
#include <cstdint>

class JITCompiler {
public:
    JITCompiler(Interpreter* interp);
    ~JITCompiler();

    void* compileFunction(FunctionDefNode* func);
    void executeJIT(BlockNode* root);

private:
    Interpreter* interpreter;
    std::map<std::string, void*> compiledFunctions;
    std::vector<void*> allocatedPages;

    void* allocateExecutableMemory(size_t size);
    void freeExecutableMemory(void* ptr, size_t size);

    std::vector<uint8_t> code;
    std::map<std::string, int> localVars;
    int stackOffset;

    void emit(uint8_t byte);
    void emit32(uint32_t value);
    void emit64(uint64_t value);

    void compileExpression(ASTNode* node);
    void compileStatement(ASTNode* stmt);
    void compileBlock(BlockNode* block);

    void emitPrologue();
    void emitEpilogue();
};

#endif
