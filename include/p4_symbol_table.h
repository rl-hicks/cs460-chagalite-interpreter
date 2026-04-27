#ifndef P4_SYMBOL_TABLE_H
#define P4_SYMBOL_TABLE_H

#include <iosfwd>
#include <string>
#include <vector>

#include "p2_tokenizer.h"
#include "p3_parser.h"

struct ParameterNode {
    std::string name;
    std::string datatype;
    bool isArray;
    int arraySize;
    int scope;
    ParameterNode* next;

    ParameterNode(const std::string& paramName,
                  const std::string& paramDatatype,
                  bool paramIsArray,
                  int paramArraySize,
                  int paramScope)
        : name(paramName),
          datatype(paramDatatype),
          isArray(paramIsArray),
          arraySize(paramArraySize),
          scope(paramScope),
          next(nullptr) {}
};

struct SymbolNode {
    std::string name;
    std::string identifierType;   // datatype / function / procedure
    std::string datatype;         // int / char / bool / NOT APPLICABLE
    bool isArray;
    int arraySize;
    int scope;

    ParameterNode* parameterHead;
    ParameterNode* parameterTail;

    SymbolNode* next;

    SymbolNode(const std::string& symbolName,
               const std::string& symbolIdentifierType,
               const std::string& symbolDatatype,
               bool symbolIsArray,
               int symbolArraySize,
               int symbolScope)
        : name(symbolName),
          identifierType(symbolIdentifierType),
          datatype(symbolDatatype),
          isArray(symbolIsArray),
          arraySize(symbolArraySize),
          scope(symbolScope),
          parameterHead(nullptr),
          parameterTail(nullptr),
          next(nullptr) {}
};

class SymbolTableBuilder {
public:
    SymbolTableBuilder();
    ~SymbolTableBuilder();

    bool build(const std::vector<Token>& tokens, CSTNode* root, std::ostream& out);

private:
    SymbolNode* head;
    SymbolNode* tail;

    int nextScopeId;
    std::string errorMessage;

    const std::vector<Token>* tokensRef;

    // cleanup
    void clear();
    void clearParameters(ParameterNode* paramHead);

    // main parse/build
    bool buildFromTokens();

    size_t parseTopLevel(size_t index);
    size_t parseGlobalDeclaration(size_t index);
    size_t parseFunctionDeclaration(size_t index);
    size_t parseProcedureDeclaration(size_t index);
    size_t parseMainProcedure(size_t index);

    size_t parseDeclarationStatement(size_t index, int scope, SymbolNode* ownerSymbol);
    size_t parseParameterList(size_t index, SymbolNode* ownerSymbol, int scope);
    size_t parseBlockForLocalDeclarations(size_t index, int scope, SymbolNode* ownerSymbol);

    // token helpers
    bool isKeyword(size_t index, const std::string& word) const;
    bool isDatatypeToken(size_t index) const;
    bool isIdentifierToken(size_t index) const;
    bool isIntegerToken(size_t index) const;
    bool isAtEnd(size_t index) const;

    // symbol insertion / duplicate checking
    SymbolNode* appendSymbol(const std::string& name,
                             const std::string& identifierType,
                             const std::string& datatype,
                             bool isArray,
                             int arraySize,
                             int scope);

    void appendParameter(SymbolNode* ownerSymbol,
                         const std::string& name,
                         const std::string& datatype,
                         bool isArray,
                         int arraySize,
                         int scope);

    SymbolNode* findSymbolInScope(const std::string& name, int scope) const;
    SymbolNode* findGlobalVariable(const std::string& name) const;
    SymbolNode* findScopeOwner(int scope) const;
    bool parameterExists(SymbolNode* ownerSymbol, const std::string& name) const;

    bool canDeclareVariable(const std::string& name,
                            int scope,
                            int lineNumber,
                            SymbolNode* ownerSymbol);

    // output
    void writeOutput(std::ostream& out) const;
    void writeSymbolTable(std::ostream& out) const;
    void writeSymbol(std::ostream& out, const SymbolNode* symbol) const;
    void writeParameterLists(std::ostream& out) const;
    void writeParameterList(std::ostream& out, const SymbolNode* symbol) const;
};

#endif
