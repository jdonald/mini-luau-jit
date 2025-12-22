#include "native_jit.h"
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <stdexcept>

// Static member for runtime callbacks
NativeJIT* NativeJIT::currentJIT = nullptr;

NativeJIT::NativeJIT(Interpreter* interp)
    : interpreter(interp), localVarCount(0) {
    codegen.reset(createCodeGenerator());
    currentJIT = this;
}

NativeJIT::~NativeJIT() {
    // Free all allocated executable memory
    for (auto& page : allocatedPages) {
        munmap(page.first, page.second);
    }
    if (currentJIT == this) {
        currentJIT = nullptr;
    }
}

void* NativeJIT::allocateExecutableMemory(size_t size) {
    // Round up to page size
    size_t pageSize = sysconf(_SC_PAGESIZE);
    size_t allocSize = (size + pageSize - 1) & ~(pageSize - 1);

    void* ptr = mmap(nullptr, allocSize,
                     PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (ptr == MAP_FAILED) {
        throw std::runtime_error("Failed to allocate executable memory");
    }

    allocatedPages.push_back({ptr, allocSize});
    return ptr;
}

void NativeJIT::collectLocals(ASTNode* node, std::set<std::string>& locals) {
    if (!node) return;

    switch (node->type) {
        case ASTNodeType::ASSIGNMENT: {
            AssignmentNode* assign = static_cast<AssignmentNode*>(node);
            if (functionParams.find(assign->variable) == functionParams.end()) {
                locals.insert(assign->variable);
            }
            collectLocals(assign->value.get(), locals);
            break;
        }
        case ASTNodeType::VARIABLE: {
            VariableNode* var = static_cast<VariableNode*>(node);
            if (functionParams.find(var->name) == functionParams.end()) {
                locals.insert(var->name);
            }
            break;
        }
        case ASTNodeType::BINARY_OP: {
            BinaryOpNode* binOp = static_cast<BinaryOpNode*>(node);
            collectLocals(binOp->left.get(), locals);
            collectLocals(binOp->right.get(), locals);
            break;
        }
        case ASTNodeType::UNARY_OP: {
            UnaryOpNode* unOp = static_cast<UnaryOpNode*>(node);
            collectLocals(unOp->operand.get(), locals);
            break;
        }
        case ASTNodeType::IF_STMT: {
            IfNode* ifNode = static_cast<IfNode*>(node);
            collectLocals(ifNode->condition.get(), locals);
            collectLocals(ifNode->thenBlock.get(), locals);
            if (ifNode->elseBlock) {
                collectLocals(ifNode->elseBlock.get(), locals);
            }
            break;
        }
        case ASTNodeType::WHILE_STMT: {
            WhileNode* whileNode = static_cast<WhileNode*>(node);
            collectLocals(whileNode->condition.get(), locals);
            collectLocals(whileNode->body.get(), locals);
            break;
        }
        case ASTNodeType::BLOCK: {
            BlockNode* block = static_cast<BlockNode*>(node);
            for (auto& stmt : block->statements) {
                collectLocals(stmt.get(), locals);
            }
            break;
        }
        case ASTNodeType::RETURN: {
            ReturnNode* retNode = static_cast<ReturnNode*>(node);
            if (retNode->value) {
                collectLocals(retNode->value.get(), locals);
            }
            break;
        }
        case ASTNodeType::FUNCTION_CALL: {
            FunctionCallNode* call = static_cast<FunctionCallNode*>(node);
            for (auto& arg : call->args) {
                collectLocals(arg.get(), locals);
            }
            break;
        }
        case ASTNodeType::PRINT: {
            PrintNode* print = static_cast<PrintNode*>(node);
            for (auto& arg : print->args) {
                collectLocals(arg.get(), locals);
            }
            break;
        }
        default:
            break;
    }
}

void NativeJIT::compileExpression(ASTNode* node) {
    if (!node) {
        codegen->emitLoadImmediate(0);
        return;
    }

    switch (node->type) {
        case ASTNodeType::INTEGER: {
            IntegerNode* intNode = static_cast<IntegerNode*>(node);
            codegen->emitLoadImmediate(intNode->value);
            break;
        }

        case ASTNodeType::BOOLEAN: {
            BooleanNode* boolNode = static_cast<BooleanNode*>(node);
            codegen->emitLoadBool(boolNode->value);
            break;
        }

        case ASTNodeType::VARIABLE: {
            VariableNode* varNode = static_cast<VariableNode*>(node);
            auto it = localVarMap.find(varNode->name);
            if (it != localVarMap.end()) {
                codegen->emitLoadLocal(it->second);
            } else {
                throw std::runtime_error("Undefined variable in JIT: " + varNode->name);
            }
            break;
        }

        case ASTNodeType::BINARY_OP: {
            BinaryOpNode* binOp = static_cast<BinaryOpNode*>(node);

            // Compile left operand
            compileExpression(binOp->left.get());
            codegen->emitPush();

            // Compile right operand
            compileExpression(binOp->right.get());

            // Pop left into secondary register
            codegen->emitPop();

            // Emit binary operation
            switch (binOp->op) {
                case BinaryOpType::ADD: codegen->emitAdd(); break;
                case BinaryOpType::SUB: codegen->emitSub(); break;
                case BinaryOpType::MUL: codegen->emitMul(); break;
                case BinaryOpType::DIV: codegen->emitDiv(); break;
                case BinaryOpType::MOD: codegen->emitMod(); break;
                case BinaryOpType::EQ:  codegen->emitCompareEq(); break;
                case BinaryOpType::NE:  codegen->emitCompareNe(); break;
                case BinaryOpType::LT:  codegen->emitCompareLt(); break;
                case BinaryOpType::LE:  codegen->emitCompareLe(); break;
                case BinaryOpType::GT:  codegen->emitCompareGt(); break;
                case BinaryOpType::GE:  codegen->emitCompareGe(); break;
                case BinaryOpType::AND: codegen->emitAnd(); break;
                case BinaryOpType::OR:  codegen->emitOr(); break;
            }
            break;
        }

        case ASTNodeType::UNARY_OP: {
            UnaryOpNode* unOp = static_cast<UnaryOpNode*>(node);
            compileExpression(unOp->operand.get());
            switch (unOp->op) {
                case UnaryOpType::NOT: codegen->emitNot(); break;
                case UnaryOpType::NEG: codegen->emitNeg(); break;
            }
            break;
        }

        case ASTNodeType::FUNCTION_CALL: {
            FunctionCallNode* call = static_cast<FunctionCallNode*>(node);
            int argCount = call->args.size();

            // Push each argument using jit_push_arg helper
            for (int i = 0; i < argCount; i++) {
                compileExpression(call->args[i].get());
                codegen->emitSetCallArg(0);  // Pass value as first arg
                codegen->emitCallRuntime((void*)&jit_push_arg, 1);
            }

            // Call the function using jit_call_func helper
            codegen->emitLoadStringPtr(call->name.c_str());
            codegen->emitSetCallArg(0);
            codegen->emitCallRuntime((void*)&jit_call_func, 1);
            // Result is in return register (rax/x0)
            break;
        }

        default:
            throw std::runtime_error("Unsupported expression type in JIT");
    }
}

void NativeJIT::compileStatement(ASTNode* node) {
    if (!node) return;

    switch (node->type) {
        case ASTNodeType::ASSIGNMENT: {
            AssignmentNode* assign = static_cast<AssignmentNode*>(node);
            compileExpression(assign->value.get());
            auto it = localVarMap.find(assign->variable);
            if (it != localVarMap.end()) {
                codegen->emitStoreLocal(it->second);
            } else {
                throw std::runtime_error("Undefined variable in JIT assignment: " + assign->variable);
            }
            break;
        }

        case ASTNodeType::IF_STMT: {
            IfNode* ifNode = static_cast<IfNode*>(node);

            Label elseLabel = codegen->createLabel();
            Label endLabel = codegen->createLabel();

            // Compile condition
            compileExpression(ifNode->condition.get());

            // Jump to else if false
            codegen->emitJumpIfFalse(elseLabel);

            // Compile then block
            compileStatement(ifNode->thenBlock.get());

            if (ifNode->elseBlock) {
                codegen->emitJump(endLabel);
            }

            // Else label
            codegen->bindLabel(elseLabel);

            // Compile else block
            if (ifNode->elseBlock) {
                compileStatement(ifNode->elseBlock.get());
                codegen->bindLabel(endLabel);
            }
            break;
        }

        case ASTNodeType::WHILE_STMT: {
            WhileNode* whileNode = static_cast<WhileNode*>(node);

            Label loopStart = codegen->createLabel();
            Label loopEnd = codegen->createLabel();

            codegen->bindLabel(loopStart);

            // Compile condition
            compileExpression(whileNode->condition.get());
            codegen->emitJumpIfFalse(loopEnd);

            // Compile body
            compileStatement(whileNode->body.get());

            // Jump back to start
            codegen->emitJump(loopStart);

            codegen->bindLabel(loopEnd);
            break;
        }

        case ASTNodeType::BLOCK: {
            BlockNode* block = static_cast<BlockNode*>(node);
            for (auto& stmt : block->statements) {
                compileStatement(stmt.get());
            }
            break;
        }

        case ASTNodeType::RETURN: {
            ReturnNode* retNode = static_cast<ReturnNode*>(node);
            if (retNode->value) {
                compileExpression(retNode->value.get());
            } else {
                codegen->emitLoadImmediate(0);
            }
            codegen->emitReturn();
            break;
        }

        case ASTNodeType::PRINT: {
            PrintNode* print = static_cast<PrintNode*>(node);

            for (size_t i = 0; i < print->args.size(); i++) {
                if (i > 0) {
                    // Print tab separator
                    codegen->emitCallRuntime((void*)&runtimePrintTab, 0);
                }

                // For simplicity, assume all print args are integers
                // A full implementation would track types
                compileExpression(print->args[i].get());
                codegen->emitSetCallArg(0);
                codegen->emitCallRuntime((void*)&runtimePrintInt, 1);
            }

            // Print newline
            codegen->emitCallRuntime((void*)&runtimePrintNewline, 0);
            break;
        }

        case ASTNodeType::FUNCTION_DEF: {
            // Function definitions are handled at the top level
            break;
        }

        case ASTNodeType::FUNCTION_CALL: {
            // Expression statement (call for side effects)
            compileExpression(node);
            break;
        }

        default:
            // Try as expression
            compileExpression(node);
            break;
    }
}

CompiledFunc NativeJIT::compileFunction(FunctionDefNode* func) {
    codegen->clear();
    localVarMap.clear();
    functionParams.clear();
    currentFunction = func->name;

    // Map parameters to local slots
    int slot = 0;
    for (const auto& param : func->params) {
        localVarMap[param] = slot++;
        functionParams.insert(param);
    }

    // Collect all local variables used in the function body
    std::set<std::string> locals;
    collectLocals(func->body.get(), locals);

    // Add locals that aren't parameters
    for (const auto& local : locals) {
        if (localVarMap.find(local) == localVarMap.end()) {
            localVarMap[local] = slot++;
        }
    }

    localVarCount = slot;

    // Generate prologue
    codegen->emitPrologue(localVarCount);

    // Copy arguments from arg array to local slots
    for (size_t i = 0; i < func->params.size(); i++) {
        codegen->emitLoadArg(i);
        codegen->emitStoreLocal(localVarMap[func->params[i]]);
    }

    // Compile function body
    compileStatement(func->body.get());

    // Default return 0 if no explicit return
    codegen->emitLoadImmediate(0);
    codegen->emitEpilogue();

    // Allocate executable memory and copy code
    const auto& code = codegen->getCode();
    void* execMem = allocateExecutableMemory(code.size());
    memcpy(execMem, code.data(), code.size());

    CompiledFuncInfo info;
    info.code = execMem;
    info.codeSize = code.size();
    info.func = (CompiledFunc)execMem;

    compiledFunctions[func->name] = info;

    return info.func;
}

bool NativeJIT::isCompiled(const std::string& name) const {
    return compiledFunctions.find(name) != compiledFunctions.end();
}

long long NativeJIT::callCompiled(const std::string& name, long long* args, int argCount) {
    auto it = compiledFunctions.find(name);
    if (it != compiledFunctions.end()) {
        return it->second.func(args, argCount);
    }
    throw std::runtime_error("Function not compiled: " + name);
}

void NativeJIT::execute(BlockNode* root) {
    if (!root) return;

    // First pass: compile all functions
    for (auto& stmt : root->statements) {
        if (stmt->type == ASTNodeType::FUNCTION_DEF) {
            FunctionDefNode* funcDef = static_cast<FunctionDefNode*>(stmt.get());
            try {
                compileFunction(funcDef);
                // Also register with interpreter for fallback
                interpreter->functions[funcDef->name] = funcDef;
            } catch (const std::exception& e) {
                // Fallback to interpreter for this function
                std::cerr << "JIT compilation failed for " << funcDef->name
                         << ": " << e.what() << ", using interpreter" << std::endl;
                interpreter->functions[funcDef->name] = funcDef;
            }
        }
    }

    // Second pass: execute top-level statements
    for (auto& stmt : root->statements) {
        if (stmt->type != ASTNodeType::FUNCTION_DEF) {
            executeStatement(stmt.get());
        }
    }
}

// Helper to evaluate expression, using JIT for function calls
Value NativeJIT::evaluateWithJIT(ASTNode* node) {
    if (!node) return Value();

    if (node->type == ASTNodeType::FUNCTION_CALL) {
        FunctionCallNode* call = static_cast<FunctionCallNode*>(node);
        if (isCompiled(call->name)) {
            // Evaluate arguments recursively
            std::vector<long long> args;
            for (auto& arg : call->args) {
                Value val = evaluateWithJIT(arg.get());
                args.push_back(val.asInteger());
            }
            long long result = callCompiled(call->name, args.data(), args.size());
            return Value(result);
        }
    }

    // For other expressions, use interpreter
    return interpreter->evaluate(node);
}

void NativeJIT::executeStatement(ASTNode* stmt) {
    if (!stmt) return;

    // Handle assignments specially to use JIT for function calls
    if (stmt->type == ASTNodeType::ASSIGNMENT) {
        AssignmentNode* assign = static_cast<AssignmentNode*>(stmt);
        Value val = evaluateWithJIT(assign->value.get());
        interpreter->variables[assign->variable] = val;
        return;
    }

    // For direct function calls, use JIT if available
    if (stmt->type == ASTNodeType::FUNCTION_CALL) {
        FunctionCallNode* call = static_cast<FunctionCallNode*>(stmt);
        if (isCompiled(call->name)) {
            std::vector<long long> args;
            for (auto& arg : call->args) {
                Value val = evaluateWithJIT(arg.get());
                args.push_back(val.asInteger());
            }
            callCompiled(call->name, args.data(), args.size());
            return;
        }
    }

    // Handle print specially
    if (stmt->type == ASTNodeType::PRINT) {
        PrintNode* print = static_cast<PrintNode*>(stmt);
        for (size_t i = 0; i < print->args.size(); ++i) {
            if (i > 0) std::cout << "\t";
            Value val = evaluateWithJIT(print->args[i].get());
            switch (val.type) {
                case ValueType::INTEGER:
                    std::cout << val.asInteger();
                    break;
                case ValueType::BOOLEAN:
                    std::cout << (val.asBoolean() ? "true" : "false");
                    break;
                case ValueType::STRING:
                    std::cout << val.asString();
                    break;
                case ValueType::NONE:
                    std::cout << "nil";
                    break;
            }
        }
        std::cout << std::endl;
        return;
    }

    // Fallback to interpreter
    interpreter->executeStatement(stmt);
}

// Static buffer for passing arguments to JIT-compiled functions
static long long jit_call_args[16];
static int jit_call_arg_index = 0;

// Helper to push an arg for the next function call
extern "C" void jit_push_arg(long long value) {
    if (jit_call_arg_index < 16) {
        jit_call_args[jit_call_arg_index++] = value;
    }
}

// Helper to call a function with accumulated args
extern "C" long long jit_call_func(const char* name) {
    if (!NativeJIT::currentJIT) {
        return 0;
    }

    int argCount = jit_call_arg_index;
    jit_call_arg_index = 0;  // Reset for next call

    std::string funcName(name);

    // Try JIT first
    if (NativeJIT::currentJIT->isCompiled(funcName)) {
        return NativeJIT::currentJIT->callCompiled(funcName, jit_call_args, argCount);
    }

    // Fallback to interpreter
    return NativeJIT::runtimeCallUserFunc(name, jit_call_args, argCount);
}

// Runtime callbacks
void NativeJIT::runtimePrintInt(long long value) {
    std::cout << value;
}

void NativeJIT::runtimePrintBool(int value) {
    std::cout << (value ? "true" : "false");
}

void NativeJIT::runtimePrintString(const char* value) {
    std::cout << value;
}

void NativeJIT::runtimePrintTab() {
    std::cout << "\t";
}

void NativeJIT::runtimePrintNewline() {
    std::cout << std::endl;
}

long long NativeJIT::runtimeCallUserFunc(const char* name, long long* args, int argCount) {
    if (!currentJIT) {
        throw std::runtime_error("No JIT context for runtime call");
    }

    std::string funcName(name);

    // Try JIT first
    if (currentJIT->isCompiled(funcName)) {
        return currentJIT->callCompiled(funcName, args, argCount);
    }

    // Fallback to interpreter
    auto it = currentJIT->interpreter->functions.find(funcName);
    if (it == currentJIT->interpreter->functions.end()) {
        throw std::runtime_error("Undefined function: " + funcName);
    }

    FunctionDefNode* funcDef = it->second;

    // Save interpreter state
    std::map<std::string, Value> savedVars = currentJIT->interpreter->variables;

    // Set up parameters
    for (size_t i = 0; i < funcDef->params.size(); i++) {
        if ((int)i < argCount) {
            currentJIT->interpreter->variables[funcDef->params[i]] = Value(args[i]);
        } else {
            currentJIT->interpreter->variables[funcDef->params[i]] = Value();
        }
    }

    // Execute function body
    Value result;
    try {
        currentJIT->interpreter->executeStatement(funcDef->body.get());
    } catch (const ReturnException& e) {
        result = e.value;
    }

    // Restore interpreter state
    currentJIT->interpreter->variables = savedVars;

    return result.asInteger();
}
