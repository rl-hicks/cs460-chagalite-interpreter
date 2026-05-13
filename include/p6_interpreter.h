#ifndef P6_INTERPRETER_H
#define P6_INTERPRETER_H

#include <iosfwd>
#include <string>
#include <unordered_map>
#include <vector>

#include "p4_symbol_table.h"
#include "p5_ast.h"

enum class ValueType
{
    INT,
    BOOL,
    CHAR,
    STRING,
    NONE
};

struct Value
{
    ValueType type = ValueType::NONE;
    int intValue = 0;
    bool boolValue = false;
    char charValue = '\0';
    std::string stringValue;
};

class Interpreter
{
public:
    Interpreter();

    bool execute(ASTNode* astRoot,
                 SymbolTableBuilder& symbolTable,
                 std::ostream& out);

private:
    struct Routine
    {
        std::string name;
        bool isFunction = false;
        std::vector<std::string> parameterNames;
        ASTNode* declarationNode = nullptr;
        ASTNode* blockNode = nullptr;
    };

    std::ostream* outputStream = nullptr;
    std::unordered_map<std::string, Routine> routines;
    std::vector<std::unordered_map<std::string, Value>> frames;

    bool returnActive = false;
    Value returnValue;

    void buildRoutineTable(ASTNode* root);
    void executeProgram(ASTNode* root);
    void executeRoutine(const Routine& routine, const std::vector<Value>& args);
    Value callFunction(const std::string& name, const std::vector<Value>& args);
    void callProcedure(const std::string& name, const std::vector<Value>& args);

    ASTNode* executeBlock(ASTNode* beginBlockNode);
    ASTNode* executeStatement(ASTNode* stmtNode);

    ASTNode* executeDeclaration(ASTNode* node);
    ASTNode* executeAssignment(ASTNode* node);
    ASTNode* executePrintf(ASTNode* node);
    ASTNode* executeIf(ASTNode* node);
    ASTNode* executeWhile(ASTNode* node);
    ASTNode* executeFor(ASTNode* node);
    ASTNode* executeCall(ASTNode* node);
    ASTNode* executeReturn(ASTNode* node);

    void executeAssignmentPayload(const std::vector<std::string>& payload);
    void declareFromPayload(const std::vector<std::string>& payload);

    Value evaluatePayloadExpression(const std::vector<std::string>& tokens);
    Value applyOperator(const std::string& op, const Value& lhs, const Value& rhs);

    std::vector<Value> evaluateCallArguments(const std::vector<std::string>& tokens,
                                             size_t openParenIndex,
                                             size_t closeParenIndex);
    Value evaluateArrayRead(const std::vector<std::string>& tokens,
                            size_t identifierIndex,
                            size_t& consumedThrough);

    Value lookupVariable(const std::string& name) const;
    void assignVariable(const std::string& name, const Value& value);
    void declareVariable(const std::string& name, const Value& value);

    ASTNode* findMatchingEndBlock(ASTNode* beginBlockNode) const;
    ASTNode* nodeAfterBlock(ASTNode* beginBlockNode) const;

    bool isRoutineDeclaration(const ASTNode* node) const;
    Routine parseRoutineDeclaration(ASTNode* declarationNode) const;

    bool isInteger(const std::string& text) const;
    bool isOperator(const std::string& text) const;
    bool isIdentifierLike(const std::string& text) const;
    bool isDatatype(const std::string& text) const;

    int toInt(const Value& value) const;
    bool toBool(const Value& value) const;
    std::string toPrintableString(const Value& value) const;

    std::string decodeEscapes(const std::string& text) const;
    std::string stringUntilNull(const std::string& text) const;
};

#endif // P6_INTERPRETER_H
