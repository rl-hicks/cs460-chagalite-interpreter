#include "p3_parser.h"

// ============================================================
// Basic token navigation helpers
// ============================================================

const Token& Parser::peek() const {
    return tokens[current];
}

const Token& Parser::advance() {
    if (!isAtEnd()) current++;
    return tokens[current - 1];
}

bool Parser::isAtEnd() const {
    return tokens[current].type == TOKEN_END_OF_FILE;
}

bool Parser::match(ChagaLiteTokens type) {
    if (peek().type == type) {
        advance();
        return true;
    }
    return false;
}

// Strict version: must match or fail
const Token& Parser::expect(ChagaLiteTokens type) {
    if (peek().type == type) {
        return advance();
    }

    throw std::runtime_error(
        "Unexpected token on line " +
        std::to_string(peek().lineNum)
    );
}

// ============================================================
// parseProgram (ROOT of everything)
// ============================================================

CSTNode* Parser::parseProgram() {
    CSTNode* programNode = new CSTNode("PROGRAM");

    while (!isAtEnd()) {

        CSTNode* child = nullptr;

        // ----------------------------------------------------
        // Decide what kind of top-level construct this is
        // ----------------------------------------------------

        if (peek().type == KEYWORD && peek().content == "function") {
            // child = parseFunctionDeclaration();
        }
        else if (peek().type == KEYWORD && peek().content == "procedure") {
            // Need to check if it's "procedure main"
            // child = parseProcedureDeclaration() OR parseMainProcedure();
        }
        else if (peek().type == KEYWORD &&
                 (peek().content == "int" ||
                  peek().content == "char" ||
                  peek().content == "bool")) {
            // child = parseDeclarationStatement();
        }
        else {
            throw std::runtime_error(
                "Invalid start of program on line " +
                std::to_string(peek().lineNum)
            );
        }

        // For now (until functions exist), just break to avoid infinite loop
        // REMOVE THIS once you implement real parsing
        break;

        // addChild(programNode, child);
    }

    return programNode;
}
