#ifndef P3_PARSER_H
#define P3_PARSER_H

#include <string>
#include <vector>
#include <stdexcept>

#include "p2_tokenizer.h"

struct CSTNode {
    std::string label;
    CSTNode* leftChild;
    CSTNode* rightSibling;

    explicit CSTNode(const std::string& value)
        : label(value), leftChild(nullptr), rightSibling(nullptr) {}
};

inline void addChild(CSTNode* parent, CSTNode* child) {
    if (parent == nullptr || child == nullptr) return;

    if (parent->leftChild == nullptr) {
        parent->leftChild = child;
        return;
    }

    CSTNode* current = parent->leftChild;
    while (current->rightSibling != nullptr) {
        current = current->rightSibling;
    }
    current->rightSibling = child;
}

inline void deleteTree(CSTNode* root) {
    if (root == nullptr) return;
    deleteTree(root->leftChild);
    deleteTree(root->rightSibling);
    delete root;
}

class Parser {
private:
    const std::vector<Token>& tokens;
    size_t current;

public:
    explicit Parser(const std::vector<Token>& tokenStream)
        : tokens(tokenStream), current(0) {}

    const Token& peek() const;
    const Token& previous() const;
    const Token& advance();
    bool match(ChagaLiteTokens type);
    bool check(ChagaLiteTokens type) const;
    const Token& expect(ChagaLiteTokens type, const std::string& message);
    bool isAtEnd() const;

    CSTNode* parseProgram();

    CSTNode* parseFunctionDeclaration();
    CSTNode* parseMainProcedure();
    CSTNode* parseProcedureDeclaration();

    CSTNode* parseBlockStatement();
    CSTNode* parseCompoundStatement();
    CSTNode* parseStatement();

    CSTNode* parseDeclarationStatement();
    CSTNode* parseAssignmentStatement();
    CSTNode* parseSelectionStatement();
    CSTNode* parseIterationStatement();
    CSTNode* parseReturnStatement();
    CSTNode* parsePrintfStatement();
    CSTNode* parseProcedureCallStatement();

    CSTNode* parseDatatypeSpecifier();
    CSTNode* parseParameterList();
    CSTNode* parseParameterDeclaration();
    CSTNode* parseIdentifierList();
    CSTNode* parseIdentifierOrArrayDeclarationList();

    CSTNode* parseExpression();
    CSTNode* parseBooleanExpression();
    CSTNode* parseNumericalExpression();
    CSTNode* parsePrimary();
    CSTNode* parseFunctionCallExpression();
    CSTNode* parseArgumentList();

    CSTNode* makeLeaf(const Token& tok);
    CSTNode* makeLeaf(const std::string& text);

    bool isDatatypeKeyword() const;
    bool isStatementStart() const;
    bool isWord(const std::string& word) const;
    bool isDatatypeWord() const;
};

#endif
