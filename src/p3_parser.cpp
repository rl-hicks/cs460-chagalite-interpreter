#include "p3_parser.h"

// ============================================================
// Basic helpers
// ============================================================

const Token& Parser::peek() const {
    static Token eofToken{TOKEN_END_OF_FILE, "", 0};

    if (current >= tokens.size()) {
        return eofToken;
    }

    return tokens[current];
}

const Token& Parser::previous() const {
    static Token eofToken{TOKEN_END_OF_FILE, "", 0};

    if (current == 0 || current - 1 >= tokens.size()) {
        return eofToken;
    }

    return tokens[current - 1];
}

const Token& Parser::advance() {
    if (!isAtEnd()) {
        current++;
    }
    return previous();
}

bool Parser::isAtEnd() const {
    return current >= tokens.size() ||
           tokens[current].type == TOKEN_END_OF_FILE;
}

bool Parser::check(ChagaLiteTokens type) const {
    if (isAtEnd()) return false;
    return peek().type == type;
}

bool Parser::match(ChagaLiteTokens type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

const Token& Parser::expect(ChagaLiteTokens type, const std::string& message) {
    if (check(type)) {
        return advance();
    }

    throw std::runtime_error(
        message + " on line " + std::to_string(peek().lineNum)
    );
}

CSTNode* Parser::makeLeaf(const Token& tok) {
    return new CSTNode(tok.content);
}

CSTNode* Parser::makeLeaf(const std::string& text) {
    return new CSTNode(text);
}

bool Parser::isWord(const std::string& word) const {
    return !isAtEnd() &&
           (peek().type == KEYWORD || peek().type == IDENTIFIER) &&
           peek().content == word;
}

bool Parser::isDatatypeWord() const {
    return isWord("int") || isWord("char") || isWord("bool");
}

bool Parser::isDatatypeKeyword() const {
    return isDatatypeWord();
}

bool Parser::isStatementStart() const {
    if (isDatatypeWord()) return true;
    if (peek().type == IDENTIFIER) return true;

    return isWord("if") ||
           isWord("while") ||
           isWord("for") ||
           isWord("return") ||
           isWord("printf");
}

// ============================================================
// PROGRAM
// ============================================================

CSTNode* Parser::parseProgram() {
    CSTNode* node = new CSTNode("PROGRAM");

    while (!isAtEnd()) {
        if (isWord("function")) {
            addChild(node, parseFunctionDeclaration());
        }
        else if (isWord("procedure")) {
            if (current + 1 < tokens.size() &&
                tokens[current + 1].content == "main") {
                addChild(node, parseMainProcedure());
            } else {
                addChild(node, parseProcedureDeclaration());
            }
        }
        else if (isDatatypeWord()) {
            addChild(node, parseDeclarationStatement());
        }
        else {
            throw std::runtime_error(
                "Invalid start of program on line " +
                std::to_string(peek().lineNum) +
                " near token '" + peek().content + "'"
            );
        }
    }

    return node;
}

// ============================================================
// Top level declarations
// ============================================================

CSTNode* Parser::parseFunctionDeclaration() {
    CSTNode* node = new CSTNode("FUNCTION_DECLARATION");

    if (!isWord("function")) {
        throw std::runtime_error(
            "Expected 'function' on line " +
            std::to_string(peek().lineNum)
        );
    }
    addChild(node, makeLeaf(advance()));

    addChild(node, parseDatatypeSpecifier());
    addChild(node, makeLeaf(expect(IDENTIFIER, "Expected function name")));
    addChild(node, makeLeaf(expect(LPAREN, "Expected '(' after function name")));

    if (isWord("void")) {
        addChild(node, makeLeaf(advance()));
    } else {
        addChild(node, parseParameterList());
    }

    addChild(node, makeLeaf(expect(RPAREN, "Expected ')' after parameter list")));
    addChild(node, parseBlockStatement());

    return node;
}

CSTNode* Parser::parseProcedureDeclaration() {
    CSTNode* node = new CSTNode("PROCEDURE_DECLARATION");

    if (!isWord("procedure")) {
        throw std::runtime_error(
            "Expected 'procedure' on line " +
            std::to_string(peek().lineNum)
        );
    }
    addChild(node, makeLeaf(advance()));

    addChild(node, makeLeaf(expect(IDENTIFIER, "Expected procedure name")));
    addChild(node, makeLeaf(expect(LPAREN, "Expected '(' after procedure name")));

    if (isWord("void")) {
        addChild(node, makeLeaf(advance()));
    } else {
        addChild(node, parseParameterList());
    }

    addChild(node, makeLeaf(expect(RPAREN, "Expected ')' after parameter list")));
    addChild(node, parseBlockStatement());

    return node;
}

CSTNode* Parser::parseMainProcedure() {
    CSTNode* node = new CSTNode("MAIN_PROCEDURE");

    if (!isWord("procedure")) {
        throw std::runtime_error(
            "Expected 'procedure' for main procedure on line " +
            std::to_string(peek().lineNum)
        );
    }
    addChild(node, makeLeaf(advance()));

    if (!isWord("main")) {
        throw std::runtime_error(
            "Expected 'main' on line " +
            std::to_string(peek().lineNum)
        );
    }
    addChild(node, makeLeaf(advance()));

    addChild(node, makeLeaf(expect(LPAREN, "Expected '(' after main")));

    if (!isWord("void")) {
        throw std::runtime_error(
            "Expected 'void' in main parameter list on line " +
            std::to_string(peek().lineNum)
        );
    }
    addChild(node, makeLeaf(advance()));

    addChild(node, makeLeaf(expect(RPAREN, "Expected ')' after void")));
    addChild(node, parseBlockStatement());

    return node;
}

// ============================================================
// Block / compound / statements
// ============================================================

CSTNode* Parser::parseBlockStatement() {
    CSTNode* node = new CSTNode("BLOCK_STATEMENT");

    addChild(node, makeLeaf(expect(LBRACE, "Expected '{'")));

    if (!check(RBRACE)) {
        addChild(node, parseCompoundStatement());
    }

    addChild(node, makeLeaf(expect(RBRACE, "Expected '}'")));

    return node;
}

CSTNode* Parser::parseCompoundStatement() {
    CSTNode* node = new CSTNode("COMPOUND_STATEMENT");

    while (!check(RBRACE) && !isAtEnd()) {
        addChild(node, parseStatement());
    }

    return node;
}

CSTNode* Parser::parseStatement() {
    CSTNode* node = new CSTNode("STATEMENT");

    if (isDatatypeWord()) {
        addChild(node, parseDeclarationStatement());
    }
    else if (isWord("if")) {
        addChild(node, parseSelectionStatement());
    }
    else if (isWord("while") || isWord("for")) {
        addChild(node, parseIterationStatement());
    }
    else if (isWord("return")) {
        addChild(node, parseReturnStatement());
    }
    else if (isWord("printf")) {
        addChild(node, parsePrintfStatement());
    }
    else if (peek().type == IDENTIFIER) {
        if (current + 1 < tokens.size() && tokens[current + 1].type == LPAREN) {
            addChild(node, parseProcedureCallStatement());
        } else {
            addChild(node, parseAssignmentStatement());
        }
    }
    else {
        throw std::runtime_error(
            "Invalid statement start on line " +
            std::to_string(peek().lineNum) +
            " near token '" + peek().content + "'"
        );
    }

    return node;
}

// ============================================================
// Declarations / parameters
// ============================================================

CSTNode* Parser::parseDatatypeSpecifier() {
    CSTNode* node = new CSTNode("DATATYPE_SPECIFIER");

    if (!isDatatypeWord()) {
        throw std::runtime_error(
            "Expected datatype specifier on line " +
            std::to_string(peek().lineNum) +
            " near token '" + peek().content + "'"
        );
    }

    addChild(node, makeLeaf(advance()));
    return node;
}

CSTNode* Parser::parseParameterDeclaration() {
    CSTNode* node = new CSTNode("PARAMETER_DECLARATION");

    addChild(node, parseDatatypeSpecifier());
    addChild(node, makeLeaf(expect(IDENTIFIER, "Expected parameter name")));

    if (match(LBRACKET)) {
        addChild(node, makeLeaf("["));
        addChild(node, makeLeaf(expect(INTEGER, "Expected array size")));
        addChild(node, makeLeaf(expect(RBRACKET, "Expected ']' after array size")));
    }

    return node;
}

CSTNode* Parser::parseParameterList() {
    CSTNode* node = new CSTNode("PARAMETER_LIST");

    addChild(node, parseParameterDeclaration());

    while (match(COMMA)) {
        addChild(node, makeLeaf(","));
        addChild(node, parseParameterDeclaration());
    }

    return node;
}

CSTNode* Parser::parseIdentifierList() {
    CSTNode* node = new CSTNode("IDENTIFIER_LIST");

    addChild(node, makeLeaf(expect(IDENTIFIER, "Expected identifier")));

    while (match(COMMA)) {
        addChild(node, makeLeaf(","));
        addChild(node, makeLeaf(expect(IDENTIFIER, "Expected identifier after comma")));
    }

    return node;
}

CSTNode* Parser::parseIdentifierOrArrayDeclarationList() {
    CSTNode* node = new CSTNode("IDENTIFIER_AND_IDENTIFIER_ARRAY_LIST");

    addChild(node, makeLeaf(expect(IDENTIFIER, "Expected identifier")));

    if (match(LBRACKET)) {
        addChild(node, makeLeaf("["));
        addChild(node, makeLeaf(expect(INTEGER, "Expected array size")));
        addChild(node, makeLeaf(expect(RBRACKET, "Expected ']' after array size")));
    }

    while (match(COMMA)) {
        addChild(node, makeLeaf(","));
        addChild(node, makeLeaf(expect(IDENTIFIER, "Expected identifier after comma")));

        if (match(LBRACKET)) {
            addChild(node, makeLeaf("["));
            addChild(node, makeLeaf(expect(INTEGER, "Expected array size")));
            addChild(node, makeLeaf(expect(RBRACKET, "Expected ']' after array size")));
        }
    }

    return node;
}

CSTNode* Parser::parseDeclarationStatement() {
    CSTNode* node = new CSTNode("DECLARATION_STATEMENT");

    addChild(node, parseDatatypeSpecifier());
    addChild(node, parseIdentifierOrArrayDeclarationList());
    addChild(node, makeLeaf(expect(SEMICOLON, "Expected ';' after declaration")));

    return node;
}

// ============================================================
// Expressions
// Pragmatic CST-friendly expression consumer
// ============================================================

CSTNode* Parser::parseExpression() {
    CSTNode* node = new CSTNode("EXPRESSION");

    int parenDepth = 0;
    int bracketDepth = 0;

    while (!isAtEnd()) {
        if (peek().type == SEMICOLON && parenDepth == 0 && bracketDepth == 0) break;
        if (peek().type == COMMA && parenDepth == 0 && bracketDepth == 0) break;
        if (peek().type == RPAREN && parenDepth == 0 && bracketDepth == 0) break;
        if (peek().type == RBRACKET && parenDepth == 0 && bracketDepth == 0) break;

        if (peek().type == LPAREN) {
            addChild(node, makeLeaf(advance()));
            parenDepth++;
        }
        else if (peek().type == RPAREN) {
            addChild(node, makeLeaf(advance()));
            parenDepth--;
        }
        else if (peek().type == LBRACKET) {
            addChild(node, makeLeaf(advance()));
            bracketDepth++;
        }
        else if (peek().type == RBRACKET) {
            addChild(node, makeLeaf(advance()));
            bracketDepth--;
        }
        else {
            addChild(node, makeLeaf(advance()));
        }
    }

    return node;
}

CSTNode* Parser::parseBooleanExpression() {
    CSTNode* node = new CSTNode("BOOLEAN_EXPRESSION");
    addChild(node, parseExpression());
    return node;
}

CSTNode* Parser::parseNumericalExpression() {
    CSTNode* node = new CSTNode("NUMERICAL_EXPRESSION");
    addChild(node, parseExpression());
    return node;
}

CSTNode* Parser::parsePrimary() {
    CSTNode* node = new CSTNode("PRIMARY");
    addChild(node, parseExpression());
    return node;
}

CSTNode* Parser::parseFunctionCallExpression() {
    CSTNode* node = new CSTNode("USER_DEFINED_FUNCTION");

    addChild(node, makeLeaf(expect(IDENTIFIER, "Expected function name")));
    addChild(node, makeLeaf(expect(LPAREN, "Expected '(' after function name")));

    if (!check(RPAREN)) {
        addChild(node, parseArgumentList());
    }

    addChild(node, makeLeaf(expect(RPAREN, "Expected ')' after arguments")));
    return node;
}

CSTNode* Parser::parseArgumentList() {
    CSTNode* node = new CSTNode("ARGUMENT_LIST");

    addChild(node, parseExpression());

    while (match(COMMA)) {
        addChild(node, makeLeaf(","));
        addChild(node, parseExpression());
    }

    return node;
}

// ============================================================
// Statements
// ============================================================

CSTNode* Parser::parseAssignmentStatement() {
    CSTNode* node = new CSTNode("ASSIGNMENT_STATEMENT");

    addChild(node, makeLeaf(expect(IDENTIFIER, "Expected identifier in assignment")));

    if (match(LBRACKET)) {
        addChild(node, makeLeaf("["));
        addChild(node, parseExpression());
        addChild(node, makeLeaf(expect(RBRACKET, "Expected ']' after index")));
    }

    addChild(node, makeLeaf(expect(ASSIGN, "Expected '=' in assignment")));
    addChild(node, parseExpression());
    addChild(node, makeLeaf(expect(SEMICOLON, "Expected ';' after assignment")));

    return node;
}

CSTNode* Parser::parseProcedureCallStatement() {
    CSTNode* node = new CSTNode("USER_DEFINED_PROCEDURE_CALL_STATEMENT");

    addChild(node, makeLeaf(expect(IDENTIFIER, "Expected procedure name")));
    addChild(node, makeLeaf(expect(LPAREN, "Expected '(' after procedure name")));

    if (!check(RPAREN)) {
        addChild(node, parseArgumentList());
    }

    addChild(node, makeLeaf(expect(RPAREN, "Expected ')' after arguments")));
    addChild(node, makeLeaf(expect(SEMICOLON, "Expected ';' after procedure call")));

    return node;
}

CSTNode* Parser::parseSelectionStatement() {
    CSTNode* node = new CSTNode("SELECTION_STATEMENT");

    if (!isWord("if")) {
        throw std::runtime_error(
            "Expected 'if' on line " + std::to_string(peek().lineNum)
        );
    }
    addChild(node, makeLeaf(advance()));

    addChild(node, makeLeaf(expect(LPAREN, "Expected '(' after if")));
    addChild(node, parseExpression());
    addChild(node, makeLeaf(expect(RPAREN, "Expected ')' after condition")));

    if (check(LBRACE)) {
        addChild(node, parseBlockStatement());
    } else {
        addChild(node, parseStatement());
    }

    if (isWord("else")) {
        addChild(node, makeLeaf(advance()));
        if (check(LBRACE)) {
            addChild(node, parseBlockStatement());
        } else {
            addChild(node, parseStatement());
        }
    }

    return node;
}

CSTNode* Parser::parseIterationStatement() {
    CSTNode* node = new CSTNode("ITERATION_STATEMENT");

    if (isWord("while")) {
        addChild(node, makeLeaf(advance()));
        addChild(node, makeLeaf(expect(LPAREN, "Expected '(' after while")));
        addChild(node, parseExpression());
        addChild(node, makeLeaf(expect(RPAREN, "Expected ')' after while condition")));

        if (check(LBRACE)) {
            addChild(node, parseBlockStatement());
        } else {
            addChild(node, parseStatement());
        }

        return node;
    }

    if (isWord("for")) {
        addChild(node, makeLeaf(advance()));
        addChild(node, makeLeaf(expect(LPAREN, "Expected '(' after for")));

        CSTNode* initNode = new CSTNode("FOR_INIT");
        addChild(initNode, parseExpression());
        addChild(node, initNode);
        addChild(node, makeLeaf(expect(SEMICOLON, "Expected ';' after for init")));

        CSTNode* condNode = new CSTNode("FOR_CONDITION");
        addChild(condNode, parseExpression());
        addChild(node, condNode);
        addChild(node, makeLeaf(expect(SEMICOLON, "Expected ';' after for condition")));

        CSTNode* updateNode = new CSTNode("FOR_UPDATE");
        addChild(updateNode, parseExpression());
        addChild(node, updateNode);

        addChild(node, makeLeaf(expect(RPAREN, "Expected ')' after for clauses")));

        if (check(LBRACE)) {
            addChild(node, parseBlockStatement());
        } else {
            addChild(node, parseStatement());
        }

        return node;
    }

    throw std::runtime_error(
        "Expected iteration statement on line " +
        std::to_string(peek().lineNum)
    );
}

CSTNode* Parser::parseReturnStatement() {
    CSTNode* node = new CSTNode("RETURN_STATEMENT");

    if (!isWord("return")) {
        throw std::runtime_error(
            "Expected 'return' on line " + std::to_string(peek().lineNum)
        );
    }

    addChild(node, makeLeaf(advance()));
    addChild(node, parseExpression());
    addChild(node, makeLeaf(expect(SEMICOLON, "Expected ';' after return")));

    return node;
}

CSTNode* Parser::parsePrintfStatement() {
    CSTNode* node = new CSTNode("PRINTF_STATEMENT");

    if (!isWord("printf")) {
        throw std::runtime_error(
            "Expected 'printf' on line " + std::to_string(peek().lineNum)
        );
    }

    addChild(node, makeLeaf(advance()));
    addChild(node, makeLeaf(expect(LPAREN, "Expected '(' after printf")));

    if (!check(RPAREN)) {
        addChild(node, parseArgumentList());
    }

    addChild(node, makeLeaf(expect(RPAREN, "Expected ')' after printf arguments")));
    addChild(node, makeLeaf(expect(SEMICOLON, "Expected ';' after printf")));

    return node;
}
