#include "p2_tokenizer.h"

#include <cctype>
#include <charconv>
#include <limits>
#include <system_error>

// ------------------------- Static Keyword Set ------------------------- //
const std::unordered_set<std::string> Tokenizer::KEYWORDS = {
    "if", "else", "for", "while", "return",
    "procedure", "function", "main",
    "int", "char", "bool", "void",
    "TRUE", "FALSE",
    "getchar", "printf", "sizeof"
};

// ------------------------- Public Entry ------------------------- //
bool Tokenizer::tokenize(std::istream& input,
                         std::vector<Token>& tokens,
                         int& errorLine,
                         std::string& errorMsg)
{
    tokens.clear();
    errorLine = -1;
    errorMsg.clear();

    int lineNumber = 1;

    char c;
    while (input.get(c)) {

        TokenizerDFAStates state = determineDFAState(input, c);

        if (state == NEWLINE) {
            ++lineNumber;
            continue;
        }

        if (state == SPACE) {
            continue;
        }

        input.unget();

        switch (state) {

            case WORD:
                if (!scanWord(input, tokens, lineNumber)) {
                    errorLine = lineNumber;
                    errorMsg = "invalid identifier.";
                    return false;
                }
                break;

            case NUMBER:
                if (!scanNumber(input, tokens, lineNumber, errorLine, errorMsg)) {
                    return false;
                }
                break;

            case STRING:
                if (!scanString(input, tokens, lineNumber, errorLine, errorMsg)) {
                    return false;
                }
                break;

            case SYMBOL:
                if (!scanSymbol(input, tokens, lineNumber, errorLine, errorMsg)) {
                    return false;
                }
                break;

            case UNKNOWN:
                errorLine = lineNumber;
                errorMsg = "unexpected character.";
                return false;

            default:
                break;
        }
    }

    if (!validateTokenSequence(tokens, errorLine, errorMsg)) {
        return false;
    }

    return true;
}

// ------------------------- DFA Classifier ------------------------- //
TokenizerDFAStates Tokenizer::determineDFAState(std::istream& in, char c)
{
    using traits = std::char_traits<char>;

    if (c == '\n') return NEWLINE;
    if (isSpace(c)) return SPACE;

    // string detection before number detection
    if (c == '"' || c == '\'') return STRING;

    if (isLetterOrUnderscore(c)) return WORD;

    int nextInt = in.peek();
    char next = (nextInt == traits::eof()) ? '\0' : static_cast<char>(nextInt);

    if (std::isdigit((unsigned char)c)) return NUMBER;
    if ((c == '+' || c == '-') && nextInt != traits::eof() &&
        std::isdigit((unsigned char)next)) {
        return NUMBER;
    }

    if (nextInt != traits::eof() && isDoubleOperator(c, next)) return SYMBOL;
    if (isSingleOperator(c)) return SYMBOL;

    return UNKNOWN;
}

// ------------------------- Scan WORD ------------------------- //
bool Tokenizer::scanWord(std::istream& in,
                         std::vector<Token>& out,
                         int& lineNumber)
{
    std::string buffer;

    while (true) {
        int p = in.peek();
        if (p == std::char_traits<char>::eof()) break;

        char c = static_cast<char>(p);
        if (std::isalnum((unsigned char)c) || c == '_') {
            in.get();
            buffer.push_back(c);
        } else {
            break;
        }
    }

    if (buffer.empty()) return false;

    Token tok;
    tok.type = (KEYWORDS.find(buffer) != KEYWORDS.end()) ? KEYWORD : IDENTIFIER;
    tok.content = buffer;
    tok.lineNum = lineNumber;

    out.push_back(std::move(tok));
    return true;
}

// ------------------------- Scan NUMBER ------------------------- //
bool Tokenizer::scanNumber(std::istream& in,
                           std::vector<Token>& out,
                           int& lineNumber,
                           int& errorLine,
                           std::string& errorMsg)
{
    std::string buffer;
    bool negative = false;

    // Optional sign
    int p = in.peek();
    if (p != std::char_traits<char>::eof() && (p == '+' || p == '-')) {
        char sign = static_cast<char>(in.get());
        buffer.push_back(sign);
        negative = (sign == '-');

        int p2 = in.peek();
        if (!(p2 != std::char_traits<char>::eof() &&
              std::isdigit((unsigned char)p2))) {
            Token tok;
            tok.type = (sign == '+') ? PLUS : MINUS;
            tok.content = std::string(1, sign);
            tok.lineNum = lineNumber;
            out.push_back(std::move(tok));
            return true;
        }
    }

    // Must have at least one digit
    p = in.peek();
    if (!(p != std::char_traits<char>::eof() &&
          std::isdigit((unsigned char)p))) {
        errorLine = lineNumber;
        errorMsg  = "invalid integer.";
        return false;
    }

    // Read digits
    std::string digitsOnly;
    while (true) {
        int d = in.peek();
        if (d == std::char_traits<char>::eof() ||
            !std::isdigit((unsigned char)d)) {
            break;
        }

        char digitChar = static_cast<char>(in.get());
        buffer.push_back(digitChar);
        digitsOnly.push_back(digitChar);
    }

    // If immediately followed by alpha/_ => invalid integer
    p = in.peek();
    if (p != std::char_traits<char>::eof() &&
        (std::isalpha((unsigned char)p) || p == '_')) {
        errorLine = lineNumber;
        errorMsg  = "invalid integer.";
        return false;
    }

    // Range check using digits-only parse, then apply sign manually
    {
        long long magnitude = 0;
        auto res = std::from_chars(
            digitsOnly.data(),
            digitsOnly.data() + digitsOnly.size(),
            magnitude,
            10
        );

        if (res.ec != std::errc{} ||
            res.ptr != digitsOnly.data() + digitsOnly.size()) {
            errorLine = lineNumber;
            errorMsg  = "invalid integer.";
            return false;
        }

        long long value = negative ? -magnitude : magnitude;

        if (value < std::numeric_limits<int>::min() ||
            value > std::numeric_limits<int>::max()) {
            errorLine = lineNumber;
            errorMsg  = "invalid integer.";
            return false;
        }
    }

    Token tok;
    tok.type = INTEGER;
    tok.content = buffer;
    tok.lineNum = lineNumber;
    out.push_back(std::move(tok));

    return true;
}

// ------------------------- Scan STRING ------------------------- //
bool Tokenizer::scanString(std::istream& in,
                           std::vector<Token>& out,
                           int& lineNumber,
                           int& errorLine,
                           std::string& errorMsg)
{
    char quote;
    if (!in.get(quote)) {
        errorLine = lineNumber;
        errorMsg  = "unterminated string quote.";
        return false;
    }

    if (quote != '"' && quote != '\'') {
        in.unget();
        errorLine = lineNumber;
        errorMsg  = "unterminated string quote.";
        return false;
    }

    std::string buffer;
    buffer.push_back(quote);

    char c;
    while (in.get(c)) {
        if (c == '\n') {
            // ++lineNumber;
            errorLine = lineNumber;
            errorMsg  = "unterminated string quote.";
            return false;
        }

        buffer.push_back(c);

        if (c == '\\') {
            char e;
            if (!in.get(e)) {
                errorLine = lineNumber;
                errorMsg  = "unterminated string quote.";
                return false;
            }
            buffer.push_back(e);
            continue;
        }

        if (c == quote) {
            Token tok;
            tok.type = TOKEN_STRING;
            tok.content = buffer;
            tok.lineNum = lineNumber;
            out.push_back(std::move(tok));
            return true;
        }
    }

    errorLine = lineNumber;
    errorMsg  = "unterminated string quote.";
    return false;
}

// ------------------------- Scan SYMBOL ------------------------- //
bool Tokenizer::scanSymbol(std::istream& in,
                           std::vector<Token>& out,
                           int& lineNumber,
                           int& errorLine,
                           std::string& errorMsg)
{
    char first;
    if (!in.get(first)) {
        errorLine = lineNumber;
        errorMsg  = "unexpected character.";
        return false;
    }

    int p = in.peek();
    if (p != std::char_traits<char>::eof()) {
        char next = static_cast<char>(p);
        if (isDoubleOperator(first, next)) {
            in.get();

            std::string two;
            two.push_back(first);
            two.push_back(next);

            Token tok;
            tok.type = classifyOperator(two);
            tok.content = two;
            tok.lineNum = lineNumber;
            out.push_back(std::move(tok));
            return true;
        }
    }

    if (isSingleOperator(first)) {
        std::string one(1, first);

        Token tok;
        tok.type = classifyOperator(one);
        tok.content = one;
        tok.lineNum = lineNumber;
        out.push_back(std::move(tok));
        return true;
    }

    errorLine = lineNumber;
    errorMsg  = "unexpected character.";
    return false;
}

// ------------------------- Validation Helpers ------------------------- //
bool Tokenizer::isReservedWord(const Token& tok) const
{
    return tok.type == KEYWORD;
}

bool Tokenizer::isDatatypeKeyword(const Token& tok) const
{
    return tok.type == KEYWORD &&
           (tok.content == "int" || tok.content == "char" || tok.content == "bool");
}

// ------------------------- Validation Pass ------------------------- //
bool Tokenizer::validateTokenSequence(const std::vector<Token>& tokens,
                                      int& errorLine,
                                      std::string& errorMsg) const
{
    for (size_t i = 0; i < tokens.size(); ++i) {
        const Token& tok = tokens[i];

        // function <type> <name>
        if (tok.type == KEYWORD && tok.content == "function") {
			if (i + 2 < tokens.size()) {
				const Token& nameTok = tokens[i + 2];
				if (isReservedWord(nameTok)) {
					errorLine = nameTok.lineNum;
					errorMsg =
						"reserved word \"" + nameTok.content +
						"\" cannot be used for the\nname of a function.";
					return false;
				}
			}
		}

        // procedure <name> , but allow procedure main
        if (tok.type == KEYWORD && tok.content == "procedure") {
            if (i + 1 < tokens.size()) {
                const Token& nameTok = tokens[i + 1];
                if (isReservedWord(nameTok) && nameTok.content != "main") {
                    errorLine = nameTok.lineNum;
                    errorMsg =
                        "reserved word \"" + nameTok.content +
                        "\" cannot be used for the name\nof a variable.";
                    return false;
                }
            }
        }

        // datatype name / parameter checks
        if (isDatatypeKeyword(tok)) {
            if (i + 1 >= tokens.size()) continue;

            const Token& nameTok = tokens[i + 1];

            if (isReservedWord(nameTok)) {
                errorLine = nameTok.lineNum;
                errorMsg =
                    "reserved word \"" + nameTok.content +
                    "\" cannot be used for the name\nof a variable.";
                return false;
            }

            if (i + 2 < tokens.size() && tokens[i + 2].type == LBRACKET) {
                if (i + 3 >= tokens.size()) continue;

                const Token& sizeTok = tokens[i + 3];

                if (sizeTok.type != INTEGER) {
                    errorLine = sizeTok.lineNum;
                    errorMsg = "array declaration size must be a positive integer.";
                    return false;
                }

                long long sizeValue = 0;
                const char* begin = sizeTok.content.data();
                const char* end   = sizeTok.content.data() + sizeTok.content.size();

                auto res = std::from_chars(begin, end, sizeValue, 10);

                if (res.ec != std::errc{} || res.ptr != end || sizeValue <= 0) {
                    errorLine = sizeTok.lineNum;
                    errorMsg = "array declaration size must be a positive integer.";
                    return false;
                }
            }
        }
    }

    return true;
}

// ------------------------- Operator Classification ------------------------- //
ChagaLiteTokens Tokenizer::classifyOperator(const std::string& text)
{
    if (text == "==") return EQUALS;
    if (text == "!=") return NOT_EQUALS;
    if (text == "<=") return LE;
    if (text == ">=") return GE;
    if (text == "&&") return AND_AND;
    if (text == "||") return OR_OR;

    if (text == "=")  return ASSIGN;
    if (text == "+")  return PLUS;
    if (text == "-")  return MINUS;
    if (text == "*")  return ASTERISK;
    if (text == "/")  return DIVIDE;
    if (text == "%")  return MODULO;
    if (text == "^")  return CARET;
    if (text == "<")  return LT;
    if (text == ">")  return GT;
    if (text == "!")  return NOT;

    if (text == "(")  return LPAREN;
    if (text == ")")  return RPAREN;
    if (text == "[")  return LBRACKET;
    if (text == "]")  return RBRACKET;
    if (text == "{")  return LBRACE;
    if (text == "}")  return RBRACE;
    if (text == ",")  return COMMA;
    if (text == ";")  return SEMICOLON;

    return TOKEN_UNKNOWN;
}
