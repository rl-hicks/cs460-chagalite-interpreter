#ifndef P5_AST_H
#define P5_AST_H

#include <iosfwd>
#include <string>
#include <vector>
#include <cstddef>

#include "p2_tokenizer.h"
#include "p3_parser.h"


struct ASTNode {
    std::string label;
    std::vector<std::string> payload;

    ASTNode* leftChild;
    ASTNode* rightSibling;

    explicit ASTNode(const std::string& nodeLabel);
    ASTNode(const std::string& nodeLabel,
            const std::vector<std::string>& nodePayload);
};

// Adds child as the last child of parent.
// Uses leftChild for the first child and rightSibling for additional children.
void addASTChild(ASTNode* parent, ASTNode* child);

// Recursively deletes the AST.
void deleteAST(ASTNode* root);

// Prints the AST in breadth-first order.
// Each node prints as:
//   LABEL
// or:
//   LABEL   payload0   payload1   payload2
void printASTBreadthFirst(ASTNode* root, std::ostream& out);

class ASTBuilder {
public:
    ASTBuilder() = default;
    ~ASTBuilder() = default;

    // Builds the Assignment 5 AST from the token stream.
    //
    // cstRoot is included so main.cpp can pass the CST from Assignment 3 and so
    // this builder remains explicitly "based on the CST" for the assignment.
    // The implementation may use the token stream as the primary source because
    // the required output is a flattened statement/expression representation.
    ASTNode* build(const std::vector<Token>& tokens, CSTNode* cstRoot);

private:
    const std::vector<Token>* tokensRef = nullptr;
    size_t current = 0;

    // -------------------------------------------------------------------------
    // Top-level parsing / AST construction
    // -------------------------------------------------------------------------
    ASTNode* buildProgram();

    void parseTopLevel(ASTNode* root);
    void parseDeclaration(ASTNode* parent);
    void parseBlock(ASTNode* parent);

    void parseStatement(ASTNode* parent);
    void parseAssignmentStatement(ASTNode* parent);
    void parseIfStatement(ASTNode* parent);
    void parseWhileStatement(ASTNode* parent);
    void parseForStatement(ASTNode* parent);
    void parseReturnStatement(ASTNode* parent);
    void parsePrintfStatement(ASTNode* parent);
    void parseCallStatement(ASTNode* parent);

    // -------------------------------------------------------------------------
    // Expression helpers
    // -------------------------------------------------------------------------
    std::vector<Token> collectExpressionUntil(ChagaLiteTokens stopType);
    std::vector<Token> collectExpressionUntilSemicolon();
    std::vector<Token> collectExpressionUntilRightParen();

    std::vector<std::string> infixToPostfix(const std::vector<Token>& expr) const;
    std::vector<std::string> tokensToStrings(const std::vector<Token>& expr) const;

    // Used when function calls and array indexing should remain visually
    // infix-like in output, matching the professor's expected output files.
    std::vector<std::string> expressionToOutputTokens(const std::vector<Token>& expr) const;

    // -------------------------------------------------------------------------
    // Token classification
    // -------------------------------------------------------------------------
    bool isAtEnd() const;
    bool check(ChagaLiteTokens type) const;
    bool checkKeyword(const std::string& word) const;
    bool match(ChagaLiteTokens type);
    bool matchKeyword(const std::string& word);

    const Token& peek() const;
    const Token& previous() const;
    const Token& advance();

    bool isDatatypeKeyword(const Token& tok) const;
    bool isDeclarationStart() const;
    bool isAssignmentStart() const;
    bool isCallStart() const;

    bool isOperand(const Token& tok) const;
    bool isOperator(const Token& tok) const;
    bool isUnaryOperator(const Token& tok) const;
    int precedence(const Token& tok) const;

    // -------------------------------------------------------------------------
    // Formatting helpers
    // -------------------------------------------------------------------------
    std::string tokenText(const Token& tok) const;

    ASTNode* makeNode(const std::string& label);
    ASTNode* makeNode(const std::string& label,
                      const std::vector<std::string>& payload);
};

#endif