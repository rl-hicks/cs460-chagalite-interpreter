#include "p4_symbol_table.h"

#include <cstddef>
#include <iostream>

// ============================================================
// Constructor / Destructor
// ============================================================

SymbolTableBuilder::SymbolTableBuilder()
    : head(nullptr),
      tail(nullptr),
      nextScopeId(1),
      errorMessage(),
      tokensRef(nullptr) {}

SymbolTableBuilder::~SymbolTableBuilder() {
    clear();
}

// ============================================================
// Cleanup
// ============================================================

void SymbolTableBuilder::clearParameters(ParameterNode* p) {
    while (p != nullptr) {
        ParameterNode* temp = p;
        p = p->next;
        delete temp;
    }
}

void SymbolTableBuilder::clear() {
    while (head != nullptr) {
        SymbolNode* temp = head;
        head = head->next;
        clearParameters(temp->parameterHead);
        delete temp;
    }

    tail = nullptr;
    errorMessage.clear();
    nextScopeId = 1;
    tokensRef = nullptr;
}

// ============================================================
// Public Build Entry
// ============================================================

bool SymbolTableBuilder::build(const std::vector<Token>& tokens,
                               CSTNode* root,
                               std::ostream& out)
{
    (void)root;

    clear();
    tokensRef = &tokens;

    if (!buildFromTokens()) {
        out << errorMessage << "\n";
        return false;
    }

    writeOutput(out);
    return true;
}

// ============================================================
// Token Helpers
// ============================================================

bool SymbolTableBuilder::isAtEnd(size_t index) const {
    return tokensRef == nullptr || index >= tokensRef->size();
}

bool SymbolTableBuilder::isKeyword(size_t index, const std::string& word) const {
    if (isAtEnd(index)) return false;
    const Token& tok = (*tokensRef)[index];
    return tok.type == KEYWORD && tok.content == word;
}

bool SymbolTableBuilder::isDatatypeToken(size_t index) const {
    if (isAtEnd(index)) return false;
    const Token& tok = (*tokensRef)[index];
    return tok.type == KEYWORD &&
           (tok.content == "int" || tok.content == "char" || tok.content == "bool");
}

bool SymbolTableBuilder::isIdentifierToken(size_t index) const {
    if (isAtEnd(index)) return false;
    return (*tokensRef)[index].type == IDENTIFIER;
}

bool SymbolTableBuilder::isIntegerToken(size_t index) const {
    if (isAtEnd(index)) return false;
    return (*tokensRef)[index].type == INTEGER;
}

// ============================================================
// Main Build From Tokens
// ============================================================

bool SymbolTableBuilder::buildFromTokens() {
    size_t index = 0;

    while (!isAtEnd(index)) {
        size_t nextIndex = parseTopLevel(index);
        if (!errorMessage.empty()) {
            return false;
        }

        if (nextIndex == index) {
            return false;
        }

        index = nextIndex;
    }

    return true;
}

size_t SymbolTableBuilder::parseTopLevel(size_t index) {
    if (isAtEnd(index)) return index;

    if (isKeyword(index, "function")) {
        return parseFunctionDeclaration(index);
    }

    if (isKeyword(index, "procedure")) {
        if (isKeyword(index + 1, "main")) {
            return parseMainProcedure(index);
        }
        return parseProcedureDeclaration(index);
    }

    if (isDatatypeToken(index)) {
        return parseGlobalDeclaration(index);
    }

    errorMessage = "Internal error: unexpected token at top level.";
    return index;
}

size_t SymbolTableBuilder::parseGlobalDeclaration(size_t index) {
    return parseDeclarationStatement(index, 0, nullptr);
}

// ============================================================
// Function / Procedure Parsing
// ============================================================

size_t SymbolTableBuilder::parseFunctionDeclaration(size_t index) {
    // function <datatype> <name> ( ... ) { ... }

    if (!isDatatypeToken(index + 1) || !isIdentifierToken(index + 2)) {
        errorMessage = "Internal error: malformed function declaration.";
        return index;
    }

    const std::string returnDatatype = (*tokensRef)[index + 1].content;
    const std::string functionName = (*tokensRef)[index + 2].content;
    const int scope = nextScopeId++;

    SymbolNode* owner = appendSymbol(functionName, "function", returnDatatype, false, 0, scope);

    size_t i = index + 3; // should be '('
    if (isAtEnd(i) || (*tokensRef)[i].type != LPAREN) {
        errorMessage = "Internal error: expected '(' in function declaration.";
        return index;
    }

    ++i; // move inside parameter list
    i = parseParameterList(i, owner, scope);
    if (!errorMessage.empty()) return index;

    if (isAtEnd(i) || (*tokensRef)[i].type != RPAREN) {
        errorMessage = "Internal error: expected ')' in function declaration.";
        return index;
    }

    ++i; // should be '{'
    i = parseBlockForLocalDeclarations(i, scope, owner);
    return i;
}

size_t SymbolTableBuilder::parseProcedureDeclaration(size_t index) {
    // procedure <name> ( ... ) { ... }

    if (!isIdentifierToken(index + 1)) {
        errorMessage = "Internal error: malformed procedure declaration.";
        return index;
    }

    const std::string procedureName = (*tokensRef)[index + 1].content;
    const int scope = nextScopeId++;

    SymbolNode* owner = appendSymbol(procedureName, "procedure", "NOT APPLICABLE", false, 0, scope);

    size_t i = index + 2; // should be '('
    if (isAtEnd(i) || (*tokensRef)[i].type != LPAREN) {
        errorMessage = "Internal error: expected '(' in procedure declaration.";
        return index;
    }

    ++i; // move inside parameter list
    i = parseParameterList(i, owner, scope);
    if (!errorMessage.empty()) return index;

    if (isAtEnd(i) || (*tokensRef)[i].type != RPAREN) {
        errorMessage = "Internal error: expected ')' in procedure declaration.";
        return index;
    }

    ++i; // should be '{'
    i = parseBlockForLocalDeclarations(i, scope, owner);
    return i;
}

size_t SymbolTableBuilder::parseMainProcedure(size_t index) {
    // procedure main ( void ) { ... }

    const int scope = nextScopeId++;
    SymbolNode* owner = appendSymbol("main", "procedure", "NOT APPLICABLE", false, 0, scope);

    size_t i = index + 2; // should be '('
    if (isAtEnd(i) || (*tokensRef)[i].type != LPAREN) {
        errorMessage = "Internal error: expected '(' in main procedure.";
        return index;
    }

    ++i; // should be void
    if (!isKeyword(i, "void")) {
        errorMessage = "Internal error: expected 'void' in main procedure.";
        return index;
    }

    ++i; // should be ')'
    if (isAtEnd(i) || (*tokensRef)[i].type != RPAREN) {
        errorMessage = "Internal error: expected ')' in main procedure.";
        return index;
    }

    ++i; // should be '{'
    i = parseBlockForLocalDeclarations(i, scope, owner);
    return i;
}

// ============================================================
// Parameter Parsing
// ============================================================

size_t SymbolTableBuilder::parseParameterList(size_t index,
                                              SymbolNode* ownerSymbol,
                                              int scope)
{
    if (isAtEnd(index)) {
        errorMessage = "Internal error: unexpected end of parameter list.";
        return index;
    }

    if (isKeyword(index, "void")) {
        return index + 1;
    }

    while (!isAtEnd(index) && (*tokensRef)[index].type != RPAREN) {
        if (!isDatatypeToken(index)) {
            errorMessage = "Internal error: expected datatype in parameter list.";
            return index;
        }

        std::string datatype = (*tokensRef)[index].content;
        ++index;

        if (!isIdentifierToken(index)) {
            errorMessage = "Internal error: expected identifier in parameter list.";
            return index;
        }

        std::string name = (*tokensRef)[index].content;
        int lineNumber = (*tokensRef)[index].lineNum;
        ++index;

        bool isArray = false;
        int arraySize = 0;

        if (!isAtEnd(index) && (*tokensRef)[index].type == LBRACKET) {
            isArray = true;
            ++index;

            if (!isIntegerToken(index)) {
                errorMessage = "Internal error: expected array size in parameter list.";
                return index;
            }

            arraySize = std::stoi((*tokensRef)[index].content);
            ++index;

            if (isAtEnd(index) || (*tokensRef)[index].type != RBRACKET) {
                errorMessage = "Internal error: expected ']' in parameter list.";
                return index;
            }

            ++index;
        }

        if (parameterExists(ownerSymbol, name)) {
            errorMessage = "Error on line " + std::to_string(lineNumber) +
                           ": variable \"" + name + "\" is already defined locally";
            return index;
        }

        appendParameter(ownerSymbol, name, datatype, isArray, arraySize, scope);

        if (!isAtEnd(index) && (*tokensRef)[index].type == COMMA) {
            ++index;
        }
    }

    return index;
}

// ============================================================
// Declaration Parsing
// ============================================================

size_t SymbolTableBuilder::parseDeclarationStatement(size_t index,
                                                     int scope,
                                                     SymbolNode* ownerSymbol)
{
    if (!isDatatypeToken(index)) {
        errorMessage = "Internal error: expected datatype in declaration.";
        return index;
    }

    const std::string datatype = (*tokensRef)[index].content;
    ++index;

    while (!isAtEnd(index)) {
        if (!isIdentifierToken(index)) {
            errorMessage = "Internal error: expected identifier in declaration.";
            return index;
        }

        std::string name = (*tokensRef)[index].content;
        int lineNumber = (*tokensRef)[index].lineNum;
        ++index;

        bool isArray = false;
        int arraySize = 0;

        if (!isAtEnd(index) && (*tokensRef)[index].type == LBRACKET) {
            isArray = true;
            ++index;

            if (!isIntegerToken(index)) {
                errorMessage = "Internal error: expected integer array size.";
                return index;
            }

            arraySize = std::stoi((*tokensRef)[index].content);
            ++index;

            if (isAtEnd(index) || (*tokensRef)[index].type != RBRACKET) {
                errorMessage = "Internal error: expected ']'.";
                return index;
            }

            ++index;
        }

        if (!canDeclareVariable(name, scope, lineNumber, ownerSymbol)) {
            return index;
        }

        appendSymbol(name, "datatype", datatype, isArray, arraySize, scope);

        if (isAtEnd(index)) {
            errorMessage = "Internal error: unexpected end of declaration.";
            return index;
        }

        if ((*tokensRef)[index].type == COMMA) {
            ++index;
            continue;
        }

        if ((*tokensRef)[index].type == SEMICOLON) {
            ++index;
            break;
        }

        errorMessage = "Internal error: expected ',' or ';' in declaration.";
        return index;
    }

    return index;
}

// ============================================================
// Block Parsing
// ============================================================

size_t SymbolTableBuilder::parseBlockForLocalDeclarations(size_t index,
                                                          int scope,
                                                          SymbolNode* ownerSymbol)
{
    if (isAtEnd(index) || (*tokensRef)[index].type != LBRACE) {
        errorMessage = "Internal error: expected '{' to start block.";
        return index;
    }

    int braceDepth = 1;
    ++index;

    while (!isAtEnd(index) && braceDepth > 0) {
        if ((*tokensRef)[index].type == LBRACE) {
            ++braceDepth;
            ++index;
            continue;
        }

        if ((*tokensRef)[index].type == RBRACE) {
            --braceDepth;
            ++index;
            continue;
        }

        if (braceDepth >= 1 && isDatatypeToken(index)) {
            index = parseDeclarationStatement(index, scope, ownerSymbol);
            if (!errorMessage.empty()) {
                return index;
            }
            continue;
        }

        ++index;
    }

    if (braceDepth != 0) {
        errorMessage = "Internal error: unmatched braces in block.";
    }

    return index;
}

// ============================================================
// Symbol Table Insert / Checks
// ============================================================

SymbolNode* SymbolTableBuilder::appendSymbol(const std::string& name,
                                             const std::string& identifierType,
                                             const std::string& datatype,
                                             bool isArray,
                                             int arraySize,
                                             int scope)
{
    SymbolNode* node = new SymbolNode(name, identifierType, datatype, isArray, arraySize, scope);

    if (head == nullptr) {
        head = node;
        tail = node;
    } else {
        tail->next = node;
        tail = node;
    }

    return node;
}

void SymbolTableBuilder::appendParameter(SymbolNode* ownerSymbol,
                                         const std::string& name,
                                         const std::string& datatype,
                                         bool isArray,
                                         int arraySize,
                                         int scope)
{
    ParameterNode* node = new ParameterNode(name, datatype, isArray, arraySize, scope);

    if (ownerSymbol->parameterHead == nullptr) {
        ownerSymbol->parameterHead = node;
        ownerSymbol->parameterTail = node;
    } else {
        ownerSymbol->parameterTail->next = node;
        ownerSymbol->parameterTail = node;
    }
}

SymbolNode* SymbolTableBuilder::findSymbolInScope(const std::string& name, int scope) const {
    SymbolNode* curr = head;
    while (curr != nullptr) {
        if (curr->name == name && curr->scope == scope && curr->identifierType == "datatype") {
            return curr;
        }
        curr = curr->next;
    }
    return nullptr;
}

SymbolNode* SymbolTableBuilder::findGlobalVariable(const std::string& name) const {
    SymbolNode* curr = head;
    while (curr != nullptr) {
        if (curr->name == name && curr->scope == 0 && curr->identifierType == "datatype") {
            return curr;
        }
        curr = curr->next;
    }
    return nullptr;
}

SymbolNode* SymbolTableBuilder::findScopeOwner(int scope) const {
    SymbolNode* curr = head;
    while (curr != nullptr) {
        if ((curr->identifierType == "function" || curr->identifierType == "procedure") &&
            curr->scope == scope) {
            return curr;
        }
        curr = curr->next;
    }
    return nullptr;
}

bool SymbolTableBuilder::parameterExists(SymbolNode* ownerSymbol, const std::string& name) const {
    if (ownerSymbol == nullptr) return false;

    ParameterNode* curr = ownerSymbol->parameterHead;
    while (curr != nullptr) {
        if (curr->name == name) {
            return true;
        }
        curr = curr->next;
    }

    return false;
}

bool SymbolTableBuilder::canDeclareVariable(const std::string& name,
                                            int scope,
                                            int lineNumber,
                                            SymbolNode* ownerSymbol)
{
    if (findSymbolInScope(name, scope) != nullptr) {
        errorMessage = "Error on line " + std::to_string(lineNumber) +
                       ": variable \"" + name + "\" is already defined locally";
        return false;
    }

    if (scope != 0) {
        if (ownerSymbol == nullptr) {
            ownerSymbol = findScopeOwner(scope);
        }

        if (parameterExists(ownerSymbol, name)) {
            errorMessage = "Error on line " + std::to_string(lineNumber) +
                           ": variable \"" + name + "\" is already defined locally";
            return false;
        }
    }

    if (scope != 0 && findGlobalVariable(name) != nullptr) {
        errorMessage = "Error on line " + std::to_string(lineNumber) +
                       ": variable \"" + name + "\" is already defined globally";
        return false;
    }

    if (scope == 0 && findGlobalVariable(name) != nullptr) {
        errorMessage = "Error on line " + std::to_string(lineNumber) +
                       ": variable \"" + name + "\" is already defined globally";
        return false;
    }

    return true;
}

// ============================================================
// Output
// ============================================================

void SymbolTableBuilder::writeOutput(std::ostream& out) const {
    writeSymbolTable(out);
    writeParameterLists(out);
}

void SymbolTableBuilder::writeSymbolTable(std::ostream& out) const {
    SymbolNode* curr = head;

    while (curr != nullptr) {
        writeSymbol(out, curr);
        curr = curr->next;
    }
}

void SymbolTableBuilder::writeSymbol(std::ostream& out, const SymbolNode* symbol) const {
    out << "      IDENTIFIER_NAME: " << symbol->name << "\n";
    out << "      IDENTIFIER_TYPE: " << symbol->identifierType << "\n";
    out << "             DATATYPE: " << symbol->datatype << "\n";
    out << "    DATATYPE_IS_ARRAY: " << (symbol->isArray ? "yes" : "no") << "\n";
    out << "  DATATYPE_ARRAY_SIZE: " << symbol->arraySize << "\n";
    out << "                SCOPE: " << symbol->scope << "\n\n";
}

void SymbolTableBuilder::writeParameterLists(std::ostream& out) const {
    SymbolNode* curr = head;

    while (curr != nullptr) {
        if ((curr->identifierType == "function" || curr->identifierType == "procedure") &&
            curr->parameterHead != nullptr) {
            out << "\n   PARAMETER LIST FOR: " << curr->name << "\n";
            writeParameterList(out, curr);
        }

        curr = curr->next;
    }
}

void SymbolTableBuilder::writeParameterList(std::ostream& out,
                                            const SymbolNode* symbol) const
{
    ParameterNode* curr = symbol->parameterHead;

    while (curr != nullptr) {
        out << "      IDENTIFIER_NAME: " << curr->name << "\n";
        out << "             DATATYPE: " << curr->datatype << "\n";
        out << "    DATATYPE_IS_ARRAY: " << (curr->isArray ? "yes" : "no") << "\n";
        out << "  DATATYPE_ARRAY_SIZE: " << curr->arraySize << "\n";
        out << "                SCOPE: " << curr->scope << "\n\n";
        curr = curr->next;
    }
}
