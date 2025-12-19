#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "ast.h"
#include <variant>
#include <map>
#include <functional>

enum class ValueType {
    INTEGER,
    BOOLEAN,
    STRING,
    NONE
};

class Value {
public:
    ValueType type;
    std::variant<long long, bool, std::string> data;

    Value() : type(ValueType::NONE) {}
    Value(long long i) : type(ValueType::INTEGER), data(i) {}
    Value(bool b) : type(ValueType::BOOLEAN), data(b) {}
    Value(const std::string& s) : type(ValueType::STRING), data(s) {}

    long long asInteger() const { return std::get<long long>(data); }
    bool asBoolean() const {
        if (type == ValueType::BOOLEAN) return std::get<bool>(data);
        if (type == ValueType::INTEGER) return std::get<long long>(data) != 0;
        return true;
    }
    std::string asString() const { return std::get<std::string>(data); }

    bool isNone() const { return type == ValueType::NONE; }
};

class ReturnException {
public:
    Value value;
    ReturnException(const Value& v) : value(v) {}
};

class Interpreter {
public:
    std::map<std::string, Value> variables;
    std::map<std::string, FunctionDefNode*> functions;

    void execute(BlockNode* root);
    Value evaluate(ASTNode* node);
    Value executeStatement(ASTNode* stmt);

private:
    Value evaluateBinaryOp(BinaryOpNode* node);
    Value evaluateUnaryOp(UnaryOpNode* node);
    Value evaluateFunctionCall(FunctionCallNode* node);
    void executePrint(PrintNode* node);
};

#endif
