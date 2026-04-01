#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <istream>
#include <ostream>
#include <string>
#include <vector>
#include <unordered_set>


// ------------------------- DFA Categories ------------------------- //
enum TokenizerDFAStates {
    WORD,
    NUMBER,
    STRING,
    SYMBOL,
    SPACE,
    NEWLINE,
    END,
    UNKNOWN
};

// ------------------------- Token Types ------------------------- //
enum ChagaLiteTokens {
    // Identifiers & keywords
    IDENTIFIER,
    KEYWORD,

    // Literals
    INTEGER,
    TOKEN_STRING,

    // Operators
    ASSIGN,
    PLUS,
    MINUS,
    ASTERISK,
    DIVIDE,
    MODULO,
    CARET,
    LT,
    GT,
    LE,
    GE,
    EQUALS,
    NOT_EQUALS,
    AND_AND,
    OR_OR,
    NOT,

    // Punctuation / delimiters
    LPAREN,
    RPAREN,
    LBRACKET,
    RBRACKET,
    LBRACE,
    RBRACE,
    COMMA,
    SEMICOLON,

    // Quotes (only if you want to output them as separate tokens)
    DQUOTE,
    SQUOTE,

    // Special markers
    TOKEN_END_OF_FILE,
    TOKEN_UNKNOWN
};

// ------------------------- Token Struct ------------------------- //
struct Token {
    ChagaLiteTokens type;
    std::string content;
    int lineNum;
};




class Tokenizer {
public:
    // Constructor
    Tokenizer() = default;

    // Destructor
    ~Tokenizer() = default;

    // Main tokenization function
	bool tokenize(std::istream& input, std::vector<Token>& tokens, 
				  int& errorLine, std::string& errorMsg);

private:

	// Keywords
    static const std::unordered_set<std::string> KEYWORDS;
    
    // Main category selector
    TokenizerDFAStates determineDFAState(std::istream& in, char c);

	// Scan helpers
    bool scanWord(std::istream& in, std::vector<Token>& out, int& lineNumber);
    bool scanNumber(std::istream& in, std::vector<Token>& out, int& lineNumber, int& errorLine, std::string& errorMsg);
    bool scanString(std::istream& in, std::vector<Token>& out, int& lineNumber, int& errorLine, std::string& errorMsg);
    bool scanSymbol(std::istream& in, std::vector<Token>& out, int& lineNumber, int& errorLine, std::string& errorMsg);

	// Character helpers
    static inline bool isSpace(char c) {
        return c==' ' || c=='\t' || c=='\v' || c=='\f' || c=='\r';
    }
    static inline bool isLetterOrUnderscore(char c) {
        return std::isalpha((unsigned char)c) || c=='_';
    }
    static inline bool isLetterDigitOrUnderscore(char c) {
        return std::isalnum((unsigned char)c) || c=='_';
    }
    static inline bool isDoubleOperator(char a, char b) {
        return (a=='='&&b=='=') || (a=='!'&&b=='=') || (a=='<'&&b=='=') || (a=='>'&&b=='=') ||
               (a=='&'&&b=='&') || (a=='|'&&b=='|');
    }
    static inline bool isSingleOperator(char c) {
        switch (c) {
            case '=': case '+': case '-': case '*': case '/': case '%': case '^':
            case '<': case '>': case '!':
            case '(': case ')': case '[': case ']': case '{': case '}':
            case ',': case ';':
                return true;
            default:
                return false;
        }
    }

    // Operator classification
    ChagaLiteTokens classifyOperator(const std::string& text);


};

#endif // TOKENIZER_H
