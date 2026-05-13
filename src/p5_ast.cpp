#include "p5_ast.h"

#include <algorithm>
#include <cctype>
#include <queue>
#include <stack>

// -----------------------------------------------------------------------------
// ASTNode
// -----------------------------------------------------------------------------

ASTNode::ASTNode(const std::string& nodeLabel)
    : label(nodeLabel),
      payload(),
      leftChild(nullptr),
      rightSibling(nullptr) {}

ASTNode::ASTNode(const std::string& nodeLabel,
                 const std::vector<std::string>& nodePayload)
    : label(nodeLabel),
      payload(nodePayload),
      leftChild(nullptr),
      rightSibling(nullptr) {}

// -----------------------------------------------------------------------------
// Basic AST helpers
// -----------------------------------------------------------------------------

void addASTChild(ASTNode* parent, ASTNode* child) {
    if (parent == nullptr || child == nullptr) return;

    if (parent->leftChild == nullptr) {
        parent->leftChild = child;
        return;
    }

    ASTNode* current = parent->leftChild;
    while (current->rightSibling != nullptr) {
        current = current->rightSibling;
    }

    current->rightSibling = child;
}

void deleteAST(ASTNode* root) {
    if (root == nullptr) return;

    deleteAST(root->leftChild);
    deleteAST(root->rightSibling);

    delete root;
}

void printASTBreadthFirst(ASTNode* root, std::ostream& out) {
    if (root == nullptr) return;

    std::queue<ASTNode*> q;
    std::string lastPrintedLabel;

    ASTNode* child = root->leftChild;
    while (child != nullptr) {
        q.push(child);
        child = child->rightSibling;
    }

    while (!q.empty()) {
        ASTNode* node = q.front();
        q.pop();

        lastPrintedLabel = node->label;

        out << node->label;

        for (const std::string& item : node->payload) {
            out << "   " << item;
        }

        out << '\n';

        ASTNode* currentChild = node->leftChild;
        while (currentChild != nullptr) {
            q.push(currentChild);
            currentChild = currentChild->rightSibling;
        }
    }

    if (lastPrintedLabel == "END BLOCK") {
        out << '\n';
    }
}
// -----------------------------------------------------------------------------
// Local helpers
// -----------------------------------------------------------------------------

namespace {

std::vector<Token> sliceTokens(const std::vector<Token>& tokens,
                               size_t begin,
                               size_t end) {
    if (begin >= end || begin >= tokens.size()) return {};

    end = std::min(end, tokens.size());

    std::vector<Token> result;
    for (size_t i = begin; i < end; ++i) {
        if (tokens[i].type == TOKEN_END_OF_FILE) break;
        result.push_back(tokens[i]);
    }

    return result;
}

std::string trimOuterSpaces(const std::string& text) {
    size_t first = 0;
    while (first < text.size() &&
           std::isspace(static_cast<unsigned char>(text[first]))) {
        ++first;
    }

    size_t last = text.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(text[last - 1]))) {
        --last;
    }

    return text.substr(first, last - first);
}

bool isQuotedLiteral(const std::string& text, char quote) {
    return text.size() >= 2 && text.front() == quote && text.back() == quote;
}

std::vector<std::string> expandLiteralToken(const std::string& text) {
    if (isQuotedLiteral(text, '\'')) {
        return {"'", text.substr(1, text.size() - 2), "'"};
    }

    if (isQuotedLiteral(text, '"')) {
        return {"\"", text.substr(1, text.size() - 2), "\""};
    }

    return {text};
}

std::string stripPrintfLiteral(const std::string& text) {
    if (isQuotedLiteral(text, '\'') || isQuotedLiteral(text, '"')) {
        // Preserve leading/trailing spaces inside string literals.
        // This matters for printf(", ") in Assignment 6.
        return text.substr(1, text.size() - 2);
    }

    return text;
}

} // namespace

// -----------------------------------------------------------------------------
// ASTBuilder public entry point
// -----------------------------------------------------------------------------

ASTNode* ASTBuilder::build(const std::vector<Token>& tokens, CSTNode* cstRoot) {
    (void)cstRoot;

    tokensRef = &tokens;
    current = 0;

    return buildProgram();
}

// -----------------------------------------------------------------------------
// Top-level AST construction
// -----------------------------------------------------------------------------

ASTNode* ASTBuilder::buildProgram() {
    ASTNode* root = makeNode("PROGRAM");
    parseTopLevel(root);
    return root;
}

void ASTBuilder::parseTopLevel(ASTNode* root) {
    while (!isAtEnd()) {
        if (isDeclarationStart()) {
            parseDeclaration(root);
        } else {
            advance();
        }
    }
}

void ASTBuilder::parseDeclaration(ASTNode* parent) {
    std::vector<Token> declarationTokens;

    while (!isAtEnd() &&
           !check(SEMICOLON) &&
           !check(LBRACE)) {
        declarationTokens.push_back(advance());
    }

    addASTChild(parent, makeNode("DECLARATION", tokensToStrings(declarationTokens)));

    if (match(SEMICOLON)) {
        return;
    }

    if (check(LBRACE)) {
        parseBlock(parent);
    }
}

void ASTBuilder::parseBlock(ASTNode* parent) {
    if (!match(LBRACE)) {
        return;
    }

    addASTChild(parent, makeNode("BEGIN BLOCK"));

    while (!isAtEnd() && !check(RBRACE)) {
        parseStatement(parent);
    }

    if (match(RBRACE)) {
        addASTChild(parent, makeNode("END BLOCK"));
    }
}

void ASTBuilder::parseStatement(ASTNode* parent) {
    if (isAtEnd()) return;

    if (check(LBRACE)) {
        parseBlock(parent);
        return;
    }

    if (isDeclarationStart()) {
        parseDeclaration(parent);
        return;
    }

    if (checkKeyword("if")) {
        parseIfStatement(parent);
        return;
    }

    if (checkKeyword("while")) {
        parseWhileStatement(parent);
        return;
    }

    if (checkKeyword("for")) {
        parseForStatement(parent);
        return;
    }

    if (checkKeyword("return")) {
        parseReturnStatement(parent);
        return;
    }

    if (checkKeyword("printf")) {
        parsePrintfStatement(parent);
        return;
    }

    if (isAssignmentStart()) {
        parseAssignmentStatement(parent);
        return;
    }

    if (isCallStart()) {
        parseCallStatement(parent);
        return;
    }

    advance();
}

// -----------------------------------------------------------------------------
// Statement parsers
// -----------------------------------------------------------------------------

void ASTBuilder::parseAssignmentStatement(ASTNode* parent) {
    std::vector<Token> lhsTokens;

    while (!isAtEnd() && !check(ASSIGN) && !check(SEMICOLON)) {
        lhsTokens.push_back(advance());
    }

    if (!match(ASSIGN)) {
        while (!isAtEnd() && !match(SEMICOLON)) {}
        return;
    }

    std::vector<Token> rhsTokens = collectExpressionUntilSemicolon();
    match(SEMICOLON);

    std::vector<std::string> payload = tokensToStrings(lhsTokens);
    std::vector<std::string> rhs = expressionToOutputTokens(rhsTokens);

    payload.insert(payload.end(), rhs.begin(), rhs.end());
    payload.push_back("=");

    addASTChild(parent, makeNode("ASSIGNMENT", payload));
}

void ASTBuilder::parseIfStatement(ASTNode* parent) {
    matchKeyword("if");

    std::vector<Token> condition;

    if (match(LPAREN)) {
        condition = collectExpressionUntilRightParen();
        match(RPAREN);
    }

    addASTChild(parent, makeNode("IF", expressionToOutputTokens(condition)));

    parseStatement(parent);

    if (matchKeyword("else")) {
        addASTChild(parent, makeNode("ELSE"));
        parseStatement(parent);
    }
}

void ASTBuilder::parseWhileStatement(ASTNode* parent) {
    matchKeyword("while");

    std::vector<Token> condition;

    if (match(LPAREN)) {
        condition = collectExpressionUntilRightParen();
        match(RPAREN);
    }

    addASTChild(parent, makeNode("WHILE", expressionToOutputTokens(condition)));

    parseStatement(parent);
}

void ASTBuilder::parseForStatement(ASTNode* parent) {
    matchKeyword("for");

    if (!match(LPAREN)) {
        return;
    }

    std::vector<Token> expr1 = collectExpressionUntil(SEMICOLON);
    match(SEMICOLON);

    std::vector<Token> expr2 = collectExpressionUntil(SEMICOLON);
    match(SEMICOLON);

    std::vector<Token> expr3 = collectExpressionUntilRightParen();
    match(RPAREN);

    auto makeForPayload = [this](const std::vector<Token>& expr) {
        std::vector<std::string> payload;

        size_t assignPos = expr.size();
        for (size_t i = 0; i < expr.size(); ++i) {
            if (expr[i].type == ASSIGN) {
                assignPos = i;
                break;
            }
        }

        if (assignPos < expr.size()) {
            std::vector<Token> lhs = sliceTokens(expr, 0, assignPos);
            std::vector<Token> rhs = sliceTokens(expr, assignPos + 1, expr.size());

            payload = tokensToStrings(lhs);

            std::vector<std::string> postfix = expressionToOutputTokens(rhs);
            payload.insert(payload.end(), postfix.begin(), postfix.end());

            payload.push_back("=");
        } else {
            payload = expressionToOutputTokens(expr);
        }

        return payload;
    };

    addASTChild(parent, makeNode("FOR EXPRESSION 1", makeForPayload(expr1)));
    addASTChild(parent, makeNode("FOR EXPRESSION 2", expressionToOutputTokens(expr2)));
    addASTChild(parent, makeNode("FOR EXPRESSION 3", makeForPayload(expr3)));

    parseStatement(parent);
}

void ASTBuilder::parseReturnStatement(ASTNode* parent) {
    matchKeyword("return");

    std::vector<Token> expr = collectExpressionUntilSemicolon();

    match(SEMICOLON);

    addASTChild(parent, makeNode("RETURN", expressionToOutputTokens(expr)));
}

void ASTBuilder::parsePrintfStatement(ASTNode* parent) {
    matchKeyword("printf");

    std::vector<std::string> payload;

    if (match(LPAREN)) {
        int depth = 1;

        while (!isAtEnd() && depth > 0) {
            Token tok = advance();

            if (tok.type == LPAREN) {
                ++depth;
                payload.push_back(tokenText(tok));
            } else if (tok.type == RPAREN) {
                --depth;
                if (depth > 0) {
                    payload.push_back(tokenText(tok));
                }
            } else if (tok.type == COMMA) {
                continue;
            } else if (tok.type == DQUOTE || tok.type == SQUOTE) {
                continue;
            } else if (tok.type == TOKEN_STRING) {
                payload.push_back(stripPrintfLiteral(tokenText(tok)));
            } else {
                payload.push_back(tokenText(tok));
            }
        }
    }

    match(SEMICOLON);

    addASTChild(parent, makeNode("PRINTF", payload));
}

void ASTBuilder::parseCallStatement(ASTNode* parent) {
    std::vector<std::string> payload;

    while (!isAtEnd() && !check(SEMICOLON)) {
        Token tok = advance();

        if (tok.type == COMMA) {
            continue;
        }

        std::vector<std::string> expanded = expandLiteralToken(tokenText(tok));
        payload.insert(payload.end(), expanded.begin(), expanded.end());
    }

    match(SEMICOLON);

    addASTChild(parent, makeNode("CALL", payload));
}

// -----------------------------------------------------------------------------
// Expression collection
// -----------------------------------------------------------------------------

std::vector<Token> ASTBuilder::collectExpressionUntil(ChagaLiteTokens stopType) {
    std::vector<Token> expr;

    int parenDepth = 0;
    int bracketDepth = 0;

    while (!isAtEnd()) {
        const Token& tok = peek();

        if (parenDepth == 0 &&
            bracketDepth == 0 &&
            tok.type == stopType) {
            break;
        }

        if (tok.type == LPAREN) {
            ++parenDepth;
        } else if (tok.type == RPAREN) {
            if (parenDepth == 0 && stopType == RPAREN) {
                break;
            }
            --parenDepth;
        } else if (tok.type == LBRACKET) {
            ++bracketDepth;
        } else if (tok.type == RBRACKET) {
            --bracketDepth;
        }

        expr.push_back(advance());
    }

    return expr;
}

std::vector<Token> ASTBuilder::collectExpressionUntilSemicolon() {
    return collectExpressionUntil(SEMICOLON);
}

std::vector<Token> ASTBuilder::collectExpressionUntilRightParen() {
    std::vector<Token> expr;

    int parenDepth = 0;
    int bracketDepth = 0;

    while (!isAtEnd()) {
        const Token& tok = peek();

        if (tok.type == RPAREN && parenDepth == 0 && bracketDepth == 0) {
            break;
        }

        if (tok.type == LPAREN) {
            ++parenDepth;
        } else if (tok.type == RPAREN) {
            --parenDepth;
        } else if (tok.type == LBRACKET) {
            ++bracketDepth;
        } else if (tok.type == RBRACKET) {
            --bracketDepth;
        }

        expr.push_back(advance());
    }

    return expr;
}

// -----------------------------------------------------------------------------
// Expression conversion
// -----------------------------------------------------------------------------

std::vector<std::string> ASTBuilder::expressionToOutputTokens(const std::vector<Token>& expr) const {
    if (expr.empty()) return {};
    return infixToPostfix(expr);
}

std::vector<std::string> ASTBuilder::tokensToStrings(const std::vector<Token>& expr) const {
    std::vector<std::string> result;

    for (const Token& tok : expr) {
        if (tok.type == COMMA) continue;
        if (tok.type == TOKEN_END_OF_FILE) continue;

        std::vector<std::string> expanded = expandLiteralToken(tokenText(tok));
        result.insert(result.end(), expanded.begin(), expanded.end());
    }

    return result;
}

std::vector<std::string> ASTBuilder::infixToPostfix(const std::vector<Token>& expr) const {
    std::vector<std::string> output;
    std::stack<Token> operators;

    auto appendTokenExpanded = [&output, this](const Token& tok) {
        std::vector<std::string> expanded = expandLiteralToken(tokenText(tok));
        output.insert(output.end(), expanded.begin(), expanded.end());
    };

    auto appendOperandGroup = [&output, this](const std::vector<Token>& group) {
        std::vector<std::string> text = tokensToStrings(group);
        output.insert(output.end(), text.begin(), text.end());
    };

    auto popOperator = [&output, &operators, this]() {
        if (operators.empty()) return;
        output.push_back(tokenText(operators.top()));
        operators.pop();
    };

    for (size_t i = 0; i < expr.size(); ++i) {
        const Token& tok = expr[i];

        if (tok.type == COMMA || tok.type == TOKEN_END_OF_FILE) {
            continue;
        }

        if (tok.type == SQUOTE) {
            output.push_back("'");
            ++i;
            while (i < expr.size()) {
                if (expr[i].type == SQUOTE) {
                    output.push_back("'");
                    break;
                }
                output.push_back(tokenText(expr[i]));
                ++i;
            }
            continue;
        }

        if (tok.type == DQUOTE) {
            output.push_back("\"");
            ++i;
            while (i < expr.size()) {
                if (expr[i].type == DQUOTE) {
                    output.push_back("\"");
                    break;
                }
                output.push_back(tokenText(expr[i]));
                ++i;
            }
            continue;
        }

        if ((tok.type == IDENTIFIER || tok.type == KEYWORD) &&
            i + 1 < expr.size() &&
            expr[i + 1].type == LPAREN) {
            std::vector<Token> group;
            group.push_back(tok);

            ++i;
            int depth = 0;

            while (i < expr.size()) {
                group.push_back(expr[i]);

                if (expr[i].type == LPAREN) {
                    ++depth;
                } else if (expr[i].type == RPAREN) {
                    --depth;
                    if (depth == 0) break;
                }

                ++i;
            }

            appendOperandGroup(group);
            continue;
        }

        if ((tok.type == IDENTIFIER || tok.type == KEYWORD) &&
            i + 1 < expr.size() &&
            expr[i + 1].type == LBRACKET) {
            std::vector<Token> group;
            group.push_back(tok);

            ++i;
            int depth = 0;

            while (i < expr.size()) {
                group.push_back(expr[i]);

                if (expr[i].type == LBRACKET) {
                    ++depth;
                } else if (expr[i].type == RBRACKET) {
                    --depth;
                    if (depth == 0) break;
                }

                ++i;
            }

            appendOperandGroup(group);
            continue;
        }

        if (isOperand(tok)) {
            appendTokenExpanded(tok);
            continue;
        }

        if (tok.type == LPAREN) {
            operators.push(tok);
            continue;
        }

        if (tok.type == RPAREN) {
            while (!operators.empty() && operators.top().type != LPAREN) {
                popOperator();
            }

            if (!operators.empty() && operators.top().type == LPAREN) {
                operators.pop();
            }

            continue;
        }

        if (isOperator(tok)) {
            while (!operators.empty() &&
                   operators.top().type != LPAREN &&
                   isOperator(operators.top()) &&
                   precedence(operators.top()) >= precedence(tok)) {
                popOperator();
            }

            operators.push(tok);
            continue;
        }

        appendTokenExpanded(tok);
    }

    while (!operators.empty()) {
        if (operators.top().type != LPAREN) {
            output.push_back(tokenText(operators.top()));
        }
        operators.pop();
    }

    return output;
}

// -----------------------------------------------------------------------------
// Token navigation
// -----------------------------------------------------------------------------

bool ASTBuilder::isAtEnd() const {
    if (tokensRef == nullptr) return true;
    if (current >= tokensRef->size()) return true;

    return (*tokensRef)[current].type == TOKEN_END_OF_FILE;
}

bool ASTBuilder::check(ChagaLiteTokens type) const {
    if (isAtEnd()) return false;
    return peek().type == type;
}

bool ASTBuilder::checkKeyword(const std::string& word) const {
    if (isAtEnd()) return false;
    return peek().type == KEYWORD && peek().content == word;
}

bool ASTBuilder::match(ChagaLiteTokens type) {
    if (!check(type)) return false;
    advance();
    return true;
}

bool ASTBuilder::matchKeyword(const std::string& word) {
    if (!checkKeyword(word)) return false;
    advance();
    return true;
}

const Token& ASTBuilder::peek() const {
    return (*tokensRef)[current];
}

const Token& ASTBuilder::previous() const {
    return (*tokensRef)[current - 1];
}

const Token& ASTBuilder::advance() {
    if (!isAtEnd()) {
        ++current;
    }

    return (*tokensRef)[current - 1];
}

// -----------------------------------------------------------------------------
// Token classification
// -----------------------------------------------------------------------------

bool ASTBuilder::isDatatypeKeyword(const Token& tok) const {
    if (tok.type != KEYWORD) return false;

    return tok.content == "int" ||
           tok.content == "char" ||
           tok.content == "bool" ||
           tok.content == "void" ||
           tok.content == "string";
}

bool ASTBuilder::isDeclarationStart() const {
    if (isAtEnd()) return false;

    const Token& tok = peek();

    if (isDatatypeKeyword(tok)) return true;

    if (tok.type == KEYWORD &&
        (tok.content == "function" ||
         tok.content == "procedure" ||
         tok.content == "main")) {
        return true;
    }

    return false;
}

bool ASTBuilder::isAssignmentStart() const {
    if (isAtEnd()) return false;

    if (peek().type != IDENTIFIER) return false;

    size_t i = current + 1;
    int bracketDepth = 0;

    while (tokensRef != nullptr && i < tokensRef->size()) {
        const Token& tok = (*tokensRef)[i];

        if (tok.type == TOKEN_END_OF_FILE || tok.type == SEMICOLON) {
            return false;
        }

        if (tok.type == LBRACKET) {
            ++bracketDepth;
        } else if (tok.type == RBRACKET) {
            --bracketDepth;
        } else if (tok.type == ASSIGN && bracketDepth == 0) {
            return true;
        } else if (tok.type == LPAREN && bracketDepth == 0) {
            return false;
        }

        ++i;
    }

    return false;
}

bool ASTBuilder::isCallStart() const {
    if (isAtEnd()) return false;

    if (peek().type != IDENTIFIER) return false;

    return current + 1 < tokensRef->size() &&
           (*tokensRef)[current + 1].type == LPAREN;
}

bool ASTBuilder::isOperand(const Token& tok) const {
    if (tok.type == IDENTIFIER) return true;
    if (tok.type == INTEGER) return true;
    if (tok.type == TOKEN_STRING) return true;

    if (tok.type == KEYWORD &&
        (tok.content == "TRUE" ||
         tok.content == "FALSE" ||
         tok.content == "true" ||
         tok.content == "false")) {
        return true;
    }

    return false;
}

bool ASTBuilder::isOperator(const Token& tok) const {
    switch (tok.type) {
        case ASSIGN:
        case PLUS:
        case MINUS:
        case ASTERISK:
        case DIVIDE:
        case MODULO:
        case CARET:
        case LT:
        case GT:
        case LE:
        case GE:
        case EQUALS:
        case NOT_EQUALS:
        case AND_AND:
        case OR_OR:
        case NOT:
            return true;

        default:
            return false;
    }
}

bool ASTBuilder::isUnaryOperator(const Token& tok) const {
    return tok.type == NOT;
}

int ASTBuilder::precedence(const Token& tok) const {
    switch (tok.type) {
        case NOT:
            return 7;

        case CARET:
            return 6;

        case ASTERISK:
        case DIVIDE:
        case MODULO:
            return 5;

        case PLUS:
        case MINUS:
            return 4;

        case LT:
        case GT:
        case LE:
        case GE:
            return 3;

        case EQUALS:
        case NOT_EQUALS:
            return 2;

        case AND_AND:
            return 1;

        case OR_OR:
            return 0;

        default:
            return -1;
    }
}

// -----------------------------------------------------------------------------
// Formatting helpers
// -----------------------------------------------------------------------------

std::string ASTBuilder::tokenText(const Token& tok) const {
    if (tok.type == SQUOTE) return "'";
    if (tok.type == DQUOTE) return "\"";
    return tok.content;
}

ASTNode* ASTBuilder::makeNode(const std::string& label) {
    return new ASTNode(label);
}

ASTNode* ASTBuilder::makeNode(const std::string& label,
                              const std::vector<std::string>& payload) {
    return new ASTNode(label, payload);
}