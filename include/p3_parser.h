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
    if (parent == nullptr || child == nullptr) {
        return;
    }

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
    if (root == nullptr) {
        return;
    }

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
    const Token& advance();
    bool match(ChagaLiteTokens type);
    const Token& expect(ChagaLiteTokens type);
    bool isAtEnd() const;

    CSTNode* parseProgram();
};

#endif
