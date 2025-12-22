#ifndef NATIVE_JIT_H
#define NATIVE_JIT_H

#include "ast.h"
#include "interpreter.h"
#include "codegen.h"
#include <vector>
#include <map>
#include <set>
#include <string>
#include <memory>

// Compiled function signature: takes args array and count, returns result
typedef long long (*CompiledFunc)(long long* args, int argCount);

// Forward declarations for JIT runtime helpers (extern "C" for name mangling)
extern "C" void jit_push_arg(long long value);
extern "C" long long jit_call_func(const char* name);

class NativeJIT {
public:
    NativeJIT(Interpreter* interp);
    ~NativeJIT();

    // Execute the program with JIT compilation
    void execute(BlockNode* root);

    // Compile a single function to native code
    CompiledFunc compileFunction(FunctionDefNode* func);

    // Call a compiled function
    long long callCompiled(const std::string& name, long long* args, int argCount);

    // Check if function is compiled
    bool isCompiled(const std::string& name) const;

    // Current JIT instance for runtime callbacks
    static NativeJIT* currentJIT;

    // Runtime helper for calling user functions from JIT code
    static long long runtimeCallUserFunc(const char* name, long long* args, int argCount);

private:
    Interpreter* interpreter;
    std::unique_ptr<CodeGenerator> codegen;

    // Compiled functions
    struct CompiledFuncInfo {
        void* code;
        size_t codeSize;
        CompiledFunc func;
    };
    std::map<std::string, CompiledFuncInfo> compiledFunctions;

    // Memory pages for executable code
    std::vector<std::pair<void*, size_t>> allocatedPages;

    // Current function being compiled
    std::string currentFunction;
    std::map<std::string, int> localVarMap;
    int localVarCount;
    std::set<std::string> functionParams;

    // Allocate executable memory
    void* allocateExecutableMemory(size_t size);

    // Collect all local variables used in a function
    void collectLocals(ASTNode* node, std::set<std::string>& locals);

    // Compile AST nodes to native code
    void compileExpression(ASTNode* node);
    void compileStatement(ASTNode* node);
    void compileBlock(BlockNode* block);

    // Execute a statement (interpreter fallback or JIT)
    void executeStatement(ASTNode* stmt);

    // Evaluate expression using JIT for function calls
    Value evaluateWithJIT(ASTNode* node);

    // Runtime helpers (called from generated code)
    static void runtimePrintInt(long long value);
    static void runtimePrintBool(int value);
    static void runtimePrintString(const char* value);
    static void runtimePrintTab();
    static void runtimePrintNewline();
};

#endif // NATIVE_JIT_H
