#include "interpreter.h"
#include <iostream>
#include <stdexcept>

void Interpreter::execute(BlockNode* root) {
    if (!root) return;

    for (auto& stmt : root->statements) {
        try {
            executeStatement(stmt.get());
        } catch (const ReturnException& e) {
            throw;
        }
    }
}

Value Interpreter::executeStatement(ASTNode* stmt) {
    if (!stmt) return Value();

    switch (stmt->type) {
        case ASTNodeType::ASSIGNMENT: {
            AssignmentNode* assign = static_cast<AssignmentNode*>(stmt);
            Value val = evaluate(assign->value.get());
            variables[assign->variable] = val;
            return val;
        }
        case ASTNodeType::FUNCTION_DEF: {
            FunctionDefNode* funcDef = static_cast<FunctionDefNode*>(stmt);
            functions[funcDef->name] = funcDef;
            return Value();
        }
        case ASTNodeType::FUNCTION_CALL: {
            return evaluateFunctionCall(static_cast<FunctionCallNode*>(stmt));
        }
        case ASTNodeType::IF_STMT: {
            IfNode* ifNode = static_cast<IfNode*>(stmt);
            Value cond = evaluate(ifNode->condition.get());
            if (cond.asBoolean()) {
                executeStatement(ifNode->thenBlock.get());
            } else if (ifNode->elseBlock) {
                executeStatement(ifNode->elseBlock.get());
            }
            return Value();
        }
        case ASTNodeType::WHILE_STMT: {
            WhileNode* whileNode = static_cast<WhileNode*>(stmt);
            while (evaluate(whileNode->condition.get()).asBoolean()) {
                executeStatement(whileNode->body.get());
            }
            return Value();
        }
        case ASTNodeType::BLOCK: {
            BlockNode* block = static_cast<BlockNode*>(stmt);
            execute(block);
            return Value();
        }
        case ASTNodeType::RETURN: {
            ReturnNode* retNode = static_cast<ReturnNode*>(stmt);
            if (retNode->value) {
                throw ReturnException(evaluate(retNode->value.get()));
            }
            throw ReturnException(Value());
        }
        case ASTNodeType::PRINT: {
            PrintNode* printNode = static_cast<PrintNode*>(stmt);
            executePrint(printNode);
            return Value();
        }
        default:
            return evaluate(stmt);
    }
}

Value Interpreter::evaluate(ASTNode* node) {
    if (!node) return Value();

    switch (node->type) {
        case ASTNodeType::INTEGER: {
            IntegerNode* intNode = static_cast<IntegerNode*>(node);
            return Value(intNode->value);
        }
        case ASTNodeType::BOOLEAN: {
            BooleanNode* boolNode = static_cast<BooleanNode*>(node);
            return Value(boolNode->value);
        }
        case ASTNodeType::STRING: {
            StringNode* strNode = static_cast<StringNode*>(node);
            return Value(strNode->value);
        }
        case ASTNodeType::VARIABLE: {
            VariableNode* varNode = static_cast<VariableNode*>(node);
            auto it = variables.find(varNode->name);
            if (it != variables.end()) {
                return it->second;
            }
            throw std::runtime_error("Undefined variable: " + varNode->name);
        }
        case ASTNodeType::BINARY_OP: {
            return evaluateBinaryOp(static_cast<BinaryOpNode*>(node));
        }
        case ASTNodeType::UNARY_OP: {
            return evaluateUnaryOp(static_cast<UnaryOpNode*>(node));
        }
        case ASTNodeType::FUNCTION_CALL: {
            return evaluateFunctionCall(static_cast<FunctionCallNode*>(node));
        }
        default:
            return Value();
    }
}

Value Interpreter::evaluateBinaryOp(BinaryOpNode* node) {
    Value left = evaluate(node->left.get());
    Value right = evaluate(node->right.get());

    switch (node->op) {
        case BinaryOpType::ADD:
            if (left.type == ValueType::STRING || right.type == ValueType::STRING) {
                std::string leftStr = (left.type == ValueType::STRING) ? left.asString() : std::to_string(left.asInteger());
                std::string rightStr = (right.type == ValueType::STRING) ? right.asString() : std::to_string(right.asInteger());
                return Value(leftStr + rightStr);
            }
            return Value(left.asInteger() + right.asInteger());
        case BinaryOpType::SUB:
            return Value(left.asInteger() - right.asInteger());
        case BinaryOpType::MUL:
            return Value(left.asInteger() * right.asInteger());
        case BinaryOpType::DIV:
            if (right.asInteger() == 0) throw std::runtime_error("Division by zero");
            return Value(left.asInteger() / right.asInteger());
        case BinaryOpType::MOD:
            if (right.asInteger() == 0) throw std::runtime_error("Modulo by zero");
            return Value(left.asInteger() % right.asInteger());
        case BinaryOpType::EQ:
            if (left.type == ValueType::INTEGER && right.type == ValueType::INTEGER)
                return Value(left.asInteger() == right.asInteger());
            if (left.type == ValueType::BOOLEAN && right.type == ValueType::BOOLEAN)
                return Value(left.asBoolean() == right.asBoolean());
            if (left.type == ValueType::STRING && right.type == ValueType::STRING)
                return Value(left.asString() == right.asString());
            return Value(false);
        case BinaryOpType::NE:
            if (left.type == ValueType::INTEGER && right.type == ValueType::INTEGER)
                return Value(left.asInteger() != right.asInteger());
            if (left.type == ValueType::BOOLEAN && right.type == ValueType::BOOLEAN)
                return Value(left.asBoolean() != right.asBoolean());
            if (left.type == ValueType::STRING && right.type == ValueType::STRING)
                return Value(left.asString() != right.asString());
            return Value(true);
        case BinaryOpType::LT:
            return Value(left.asInteger() < right.asInteger());
        case BinaryOpType::LE:
            return Value(left.asInteger() <= right.asInteger());
        case BinaryOpType::GT:
            return Value(left.asInteger() > right.asInteger());
        case BinaryOpType::GE:
            return Value(left.asInteger() >= right.asInteger());
        case BinaryOpType::AND:
            return Value(left.asBoolean() && right.asBoolean());
        case BinaryOpType::OR:
            return Value(left.asBoolean() || right.asBoolean());
        default:
            return Value();
    }
}

Value Interpreter::evaluateUnaryOp(UnaryOpNode* node) {
    Value operand = evaluate(node->operand.get());

    switch (node->op) {
        case UnaryOpType::NOT:
            return Value(!operand.asBoolean());
        case UnaryOpType::NEG:
            return Value(-operand.asInteger());
        default:
            return Value();
    }
}

Value Interpreter::evaluateFunctionCall(FunctionCallNode* node) {
    auto it = functions.find(node->name);
    if (it == functions.end()) {
        throw std::runtime_error("Undefined function: " + node->name);
    }

    FunctionDefNode* funcDef = it->second;

    std::map<std::string, Value> savedVars = variables;

    for (size_t i = 0; i < funcDef->params.size(); ++i) {
        if (i < node->args.size()) {
            variables[funcDef->params[i]] = evaluate(node->args[i].get());
        } else {
            variables[funcDef->params[i]] = Value();
        }
    }

    Value result;
    try {
        executeStatement(funcDef->body.get());
    } catch (const ReturnException& e) {
        result = e.value;
    }

    variables = savedVars;

    return result;
}

void Interpreter::executePrint(PrintNode* node) {
    for (size_t i = 0; i < node->args.size(); ++i) {
        if (i > 0) std::cout << "\t";

        Value val = evaluate(node->args[i].get());
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
}
