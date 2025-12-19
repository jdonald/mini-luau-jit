#ifndef AST_H
#define AST_H

#include <string>
#include <vector>
#include <memory>
#include <map>

enum class ASTNodeType {
    INTEGER,
    BOOLEAN,
    STRING,
    VARIABLE,
    BINARY_OP,
    UNARY_OP,
    ASSIGNMENT,
    FUNCTION_DEF,
    FUNCTION_CALL,
    RETURN,
    IF_STMT,
    WHILE_STMT,
    BLOCK,
    PRINT,
    TYPE_ANNOTATION
};

enum class BinaryOpType {
    ADD, SUB, MUL, DIV, MOD,
    EQ, NE, LT, LE, GT, GE,
    AND, OR
};

enum class UnaryOpType {
    NOT, NEG
};

class ASTNode {
public:
    ASTNodeType type;
    virtual ~ASTNode() = default;

protected:
    ASTNode(ASTNodeType t) : type(t) {}
};

class IntegerNode : public ASTNode {
public:
    long long value;
    IntegerNode(long long v) : ASTNode(ASTNodeType::INTEGER), value(v) {}
};

class BooleanNode : public ASTNode {
public:
    bool value;
    BooleanNode(bool v) : ASTNode(ASTNodeType::BOOLEAN), value(v) {}
};

class StringNode : public ASTNode {
public:
    std::string value;
    StringNode(const std::string& v) : ASTNode(ASTNodeType::STRING), value(v) {}
};

class VariableNode : public ASTNode {
public:
    std::string name;
    VariableNode(const std::string& n) : ASTNode(ASTNodeType::VARIABLE), name(n) {}
};

class BinaryOpNode : public ASTNode {
public:
    BinaryOpType op;
    std::unique_ptr<ASTNode> left;
    std::unique_ptr<ASTNode> right;
    BinaryOpNode(BinaryOpType o, ASTNode* l, ASTNode* r)
        : ASTNode(ASTNodeType::BINARY_OP), op(o), left(l), right(r) {}
};

class UnaryOpNode : public ASTNode {
public:
    UnaryOpType op;
    std::unique_ptr<ASTNode> operand;
    UnaryOpNode(UnaryOpType o, ASTNode* opnd)
        : ASTNode(ASTNodeType::UNARY_OP), op(o), operand(opnd) {}
};

class AssignmentNode : public ASTNode {
public:
    std::string variable;
    std::string typeAnnotation;
    std::unique_ptr<ASTNode> value;
    AssignmentNode(const std::string& var, ASTNode* val, const std::string& type = "")
        : ASTNode(ASTNodeType::ASSIGNMENT), variable(var), typeAnnotation(type), value(val) {}
};

class FunctionDefNode : public ASTNode {
public:
    std::string name;
    std::vector<std::string> params;
    std::vector<std::string> paramTypes;
    std::string returnType;
    std::unique_ptr<ASTNode> body;
    FunctionDefNode(const std::string& n, const std::vector<std::string>& p, ASTNode* b,
                    const std::vector<std::string>& pt = {}, const std::string& rt = "")
        : ASTNode(ASTNodeType::FUNCTION_DEF), name(n), params(p), paramTypes(pt), returnType(rt), body(b) {}
};

class FunctionCallNode : public ASTNode {
public:
    std::string name;
    std::vector<std::unique_ptr<ASTNode>> args;
    FunctionCallNode(const std::string& n)
        : ASTNode(ASTNodeType::FUNCTION_CALL), name(n) {}
};

class ReturnNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> value;
    ReturnNode(ASTNode* v) : ASTNode(ASTNodeType::RETURN), value(v) {}
};

class IfNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<ASTNode> thenBlock;
    std::unique_ptr<ASTNode> elseBlock;
    IfNode(ASTNode* cond, ASTNode* thenB, ASTNode* elseB = nullptr)
        : ASTNode(ASTNodeType::IF_STMT), condition(cond), thenBlock(thenB), elseBlock(elseB) {}
};

class WhileNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<ASTNode> body;
    WhileNode(ASTNode* cond, ASTNode* b)
        : ASTNode(ASTNodeType::WHILE_STMT), condition(cond), body(b) {}
};

class BlockNode : public ASTNode {
public:
    std::vector<std::unique_ptr<ASTNode>> statements;
    BlockNode() : ASTNode(ASTNodeType::BLOCK) {}
    void addStatement(ASTNode* stmt) {
        statements.push_back(std::unique_ptr<ASTNode>(stmt));
    }
};

class PrintNode : public ASTNode {
public:
    std::vector<std::unique_ptr<ASTNode>> args;
    PrintNode() : ASTNode(ASTNodeType::PRINT) {}
};

extern BlockNode* programRoot;

#endif
