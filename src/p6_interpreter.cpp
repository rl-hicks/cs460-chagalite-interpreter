#include "p6_interpreter.h"

#include <cctype>
#include <iostream>
#include <stack>
#include <stdexcept>

Interpreter::Interpreter() = default;

bool Interpreter::execute(ASTNode* astRoot,
                          SymbolTableBuilder& symbolTable,
                          std::ostream& out)
{
    (void)symbolTable;

    if (astRoot == nullptr)
    {
        out << "ERROR: AST root is null.\n";
        return false;
    }

    outputStream = &out;
    routines.clear();
    frames.clear();
    returnActive = false;
    returnValue = Value{};

    try
    {
        buildRoutineTable(astRoot);
        executeProgram(astRoot);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Interpreter error: " << e.what() << '\n';
        return false;
    }

    return true;
}

void Interpreter::buildRoutineTable(ASTNode* root)
{
    ASTNode* current = root == nullptr ? nullptr : root->leftChild;

    while (current != nullptr)
    {
        if (isRoutineDeclaration(current) &&
            current->rightSibling != nullptr &&
            current->rightSibling->label == "BEGIN BLOCK")
        {
            Routine routine = parseRoutineDeclaration(current);
            routine.declarationNode = current;
            routine.blockNode = current->rightSibling;
            routines[routine.name] = routine;
        }

        current = current->rightSibling;
    }
}

void Interpreter::executeProgram(ASTNode* root)
{
    auto mainIt = routines.find("main");
    if (mainIt != routines.end())
    {
        executeRoutine(mainIt->second, {});
        return;
    }

    // Fallback for old ASTs that did not preserve declaration payloads.
    ASTNode* current = root == nullptr ? nullptr : root->leftChild;
    ASTNode* lastBeginBlock = nullptr;

    while (current != nullptr)
    {
        if (current->label == "BEGIN BLOCK")
        {
            lastBeginBlock = current;
        }
        current = current->rightSibling;
    }

    if (lastBeginBlock == nullptr)
    {
        throw std::runtime_error("No executable main block found.");
    }

    frames.push_back({});
    executeBlock(lastBeginBlock);
    frames.pop_back();
}

void Interpreter::executeRoutine(const Routine& routine, const std::vector<Value>& args)
{
    std::unordered_map<std::string, Value> frame;

    for (size_t i = 0; i < routine.parameterNames.size(); ++i)
    {
        Value value;
        if (i < args.size())
        {
            value = args[i];
        }
        frame[routine.parameterNames[i]] = value;
    }

    frames.push_back(frame);
    const bool oldReturnActive = returnActive;
    const Value oldReturnValue = returnValue;
    returnActive = false;
    returnValue = Value{};

    executeBlock(routine.blockNode);

    frames.pop_back();

    // Preserve a function's return state for callFunction(), but clear procedure returns.
    if (!routine.isFunction)
    {
        returnActive = oldReturnActive;
        returnValue = oldReturnValue;
    }
}

Value Interpreter::callFunction(const std::string& name, const std::vector<Value>& args)
{
    auto it = routines.find(name);
    if (it == routines.end())
    {
        throw std::runtime_error("Unknown function: " + name);
    }

    executeRoutine(it->second, args);
    Value result = returnValue;
    returnActive = false;
    returnValue = Value{};
    return result;
}

void Interpreter::callProcedure(const std::string& name, const std::vector<Value>& args)
{
    auto it = routines.find(name);
    if (it == routines.end())
    {
        throw std::runtime_error("Unknown procedure: " + name);
    }

    executeRoutine(it->second, args);
}

ASTNode* Interpreter::executeBlock(ASTNode* beginBlockNode)
{
    if (beginBlockNode == nullptr || beginBlockNode->label != "BEGIN BLOCK")
    {
        return beginBlockNode;
    }

    ASTNode* stmt = beginBlockNode->rightSibling;

    while (stmt != nullptr && stmt->label != "END BLOCK")
    {
        stmt = executeStatement(stmt);

        if (returnActive)
        {
            return nodeAfterBlock(beginBlockNode);
        }
    }

    if (stmt != nullptr && stmt->label == "END BLOCK")
    {
        return stmt->rightSibling;
    }

    return stmt;
}

ASTNode* Interpreter::executeStatement(ASTNode* stmtNode)
{
    if (stmtNode == nullptr)
    {
        return nullptr;
    }

    const std::string& label = stmtNode->label;

    if (label == "DECLARATION")
    {
        return executeDeclaration(stmtNode);
    }
    if (label == "ASSIGNMENT")
    {
        return executeAssignment(stmtNode);
    }
    if (label == "PRINTF")
    {
        return executePrintf(stmtNode);
    }
    if (label == "IF")
    {
        return executeIf(stmtNode);
    }
    if (label == "WHILE")
    {
        return executeWhile(stmtNode);
    }
    if (label == "FOR EXPRESSION 1")
    {
        return executeFor(stmtNode);
    }
    if (label == "CALL")
    {
        return executeCall(stmtNode);
    }
    if (label == "RETURN")
    {
        return executeReturn(stmtNode);
    }
    if (label == "BEGIN BLOCK")
    {
        return executeBlock(stmtNode);
    }
    if (label == "ELSE")
    {
        return stmtNode->rightSibling;
    }
    if (label == "END BLOCK")
    {
        return stmtNode;
    }

    return stmtNode->rightSibling;
}

ASTNode* Interpreter::executeDeclaration(ASTNode* node)
{
    if (node != nullptr && !isRoutineDeclaration(node))
    {
        declareFromPayload(node->payload);
    }

    return node == nullptr ? nullptr : node->rightSibling;
}

ASTNode* Interpreter::executeAssignment(ASTNode* node)
{
    if (node != nullptr)
    {
        executeAssignmentPayload(node->payload);
    }

    return node == nullptr ? nullptr : node->rightSibling;
}

ASTNode* Interpreter::executePrintf(ASTNode* node)
{
    if (node == nullptr || node->payload.empty())
    {
        return node == nullptr ? nullptr : node->rightSibling;
    }

    const std::string format = decodeEscapes(node->payload[0]);
    size_t argIndex = 1;

    for (size_t i = 0; i < format.size(); ++i)
    {
        if (format[i] == '%' && i + 1 < format.size())
        {
            const char spec = format[i + 1];
            if (spec == 'd' || spec == 's' || spec == 'c')
            {
                Value arg;
                if (argIndex < node->payload.size())
                {
                    arg = evaluatePayloadExpression({node->payload[argIndex]});
                    ++argIndex;
                }

                if (spec == 'd')
                {
                    (*outputStream) << toInt(arg);
                }
                else if (spec == 's')
                {
                    (*outputStream) << stringUntilNull(toPrintableString(arg));
                }
                else if (spec == 'c')
                {
                    (*outputStream) << static_cast<char>(toInt(arg));
                }

                ++i;
                continue;
            }
        }

        (*outputStream) << format[i];
    }

    return node->rightSibling;
}

ASTNode* Interpreter::executeIf(ASTNode* node)
{
    if (node == nullptr)
    {
        return nullptr;
    }

    ASTNode* trueBlock = node->rightSibling;
    if (trueBlock == nullptr || trueBlock->label != "BEGIN BLOCK")
    {
        return node->rightSibling;
    }

    ASTNode* afterTrue = nodeAfterBlock(trueBlock);
    ASTNode* elseNode = afterTrue;
    ASTNode* falseBlock = nullptr;
    ASTNode* afterIf = afterTrue;

    if (elseNode != nullptr && elseNode->label == "ELSE")
    {
        falseBlock = elseNode->rightSibling;
        if (falseBlock != nullptr && falseBlock->label == "BEGIN BLOCK")
        {
            afterIf = nodeAfterBlock(falseBlock);
        }
        else
        {
            afterIf = elseNode->rightSibling;
        }
    }

    if (toBool(evaluatePayloadExpression(node->payload)))
    {
        executeBlock(trueBlock);
    }
    else if (falseBlock != nullptr && falseBlock->label == "BEGIN BLOCK")
    {
        executeBlock(falseBlock);
    }

    return afterIf;
}

ASTNode* Interpreter::executeWhile(ASTNode* node)
{
    if (node == nullptr)
    {
        return nullptr;
    }

    ASTNode* bodyBlock = node->rightSibling;
    if (bodyBlock == nullptr || bodyBlock->label != "BEGIN BLOCK")
    {
        return node->rightSibling;
    }

    ASTNode* afterWhile = nodeAfterBlock(bodyBlock);

    while (toBool(evaluatePayloadExpression(node->payload)))
    {
        executeBlock(bodyBlock);
        if (returnActive)
        {
            break;
        }
    }

    return afterWhile;
}

ASTNode* Interpreter::executeFor(ASTNode* node)
{
    if (node == nullptr)
    {
        return nullptr;
    }

    ASTNode* expr2 = node->rightSibling;
    ASTNode* expr3 = expr2 == nullptr ? nullptr : expr2->rightSibling;
    ASTNode* bodyBlock = expr3 == nullptr ? nullptr : expr3->rightSibling;

    if (expr2 == nullptr || expr2->label != "FOR EXPRESSION 2" ||
        expr3 == nullptr || expr3->label != "FOR EXPRESSION 3" ||
        bodyBlock == nullptr || bodyBlock->label != "BEGIN BLOCK")
    {
        return node->rightSibling;
    }

    executeAssignmentPayload(node->payload);
    ASTNode* afterFor = nodeAfterBlock(bodyBlock);

    while (toBool(evaluatePayloadExpression(expr2->payload)))
    {
        executeBlock(bodyBlock);
        if (returnActive)
        {
            break;
        }
        executeAssignmentPayload(expr3->payload);
    }

    return afterFor;
}

ASTNode* Interpreter::executeCall(ASTNode* node)
{
    if (node != nullptr && !node->payload.empty())
    {
        const std::string& name = node->payload[0];
        size_t open = 1;
        size_t close = node->payload.size();
        for (size_t i = 1; i < node->payload.size(); ++i)
        {
            if (node->payload[i] == "(") open = i;
            if (node->payload[i] == ")") { close = i; break; }
        }
        callProcedure(name, evaluateCallArguments(node->payload, open, close));
    }

    return node == nullptr ? nullptr : node->rightSibling;
}

ASTNode* Interpreter::executeReturn(ASTNode* node)
{
    if (node != nullptr)
    {
        returnValue = evaluatePayloadExpression(node->payload);
        returnActive = true;
    }

    return node == nullptr ? nullptr : node->rightSibling;
}

void Interpreter::executeAssignmentPayload(const std::vector<std::string>& payload)
{
    if (payload.size() < 3 || payload.back() != "=")
    {
        return;
    }

    const std::string target = payload[0];
    std::vector<std::string> rhs(payload.begin() + 1, payload.end() - 1);
    Value value = evaluatePayloadExpression(rhs);
    assignVariable(target, value);
}

void Interpreter::declareFromPayload(const std::vector<std::string>& payload)
{
    if (payload.empty())
    {
        return;
    }

    const std::string datatype = payload[0];
    if (!isDatatype(datatype))
    {
        return;
    }

    for (size_t i = 1; i < payload.size(); ++i)
    {
        const std::string& tok = payload[i];
        if (!isIdentifierLike(tok))
        {
            continue;
        }

        Value value;
        if (datatype == "int")
        {
            value.type = ValueType::INT;
            value.intValue = 0;
        }
        else if (datatype == "bool")
        {
            value.type = ValueType::BOOL;
            value.boolValue = false;
        }
        else if (datatype == "char")
        {
            if (i + 1 < payload.size() && payload[i + 1] == "[")
            {
                value.type = ValueType::STRING;
                value.stringValue.clear();
            }
            else
            {
                value.type = ValueType::CHAR;
                value.charValue = '\0';
                value.intValue = 0;
            }
        }
        else if (datatype == "string")
        {
            value.type = ValueType::STRING;
            value.stringValue.clear();
        }
        else
        {
            value.type = ValueType::NONE;
        }

        declareVariable(tok, value);
    }
}

Value Interpreter::evaluatePayloadExpression(const std::vector<std::string>& tokens)
{
    std::stack<Value> evalStack;

    for (size_t i = 0; i < tokens.size(); ++i)
    {
        const std::string& tok = tokens[i];

        if (tok.empty() || tok == ",")
        {
            continue;
        }

        if (tok == "\"")
        {
            std::string content;
            ++i;
            while (i < tokens.size() && tokens[i] != "\"")
            {
                if (!content.empty()) content += " ";
                content += tokens[i];
                ++i;
            }

            Value value;
            value.type = ValueType::STRING;
            value.stringValue = decodeEscapes(content);
            evalStack.push(value);
            continue;
        }

        if (tok == "'")
        {
            std::string content;
            ++i;
            while (i < tokens.size() && tokens[i] != "'")
            {
                content += tokens[i];
                ++i;
            }

            const std::string decoded = decodeEscapes(content);
            const char c = decoded.empty() ? '\0' : decoded[0];

            Value value;
            value.type = ValueType::CHAR;
            value.charValue = c;
            value.intValue = static_cast<unsigned char>(c);
            evalStack.push(value);
            continue;
        }

        if (isInteger(tok))
        {
            Value value;
            value.type = ValueType::INT;
            value.intValue = std::stoi(tok);
            evalStack.push(value);
            continue;
        }

        if (tok == "true" || tok == "TRUE" || tok == "false" || tok == "FALSE")
        {
            Value value;
            value.type = ValueType::BOOL;
            value.boolValue = (tok == "true" || tok == "TRUE");
            evalStack.push(value);
            continue;
        }

        if (isIdentifierLike(tok) && i + 1 < tokens.size() && tokens[i + 1] == "(")
        {
            size_t close = i + 1;
            int depth = 0;
            for (; close < tokens.size(); ++close)
            {
                if (tokens[close] == "(") ++depth;
                else if (tokens[close] == ")")
                {
                    --depth;
                    if (depth == 0) break;
                }
            }

            std::vector<Value> args = evaluateCallArguments(tokens, i + 1, close);
            evalStack.push(callFunction(tok, args));
            i = close;
            continue;
        }

        if (isIdentifierLike(tok) && i + 1 < tokens.size() && tokens[i + 1] == "[")
        {
            size_t consumedThrough = i;
            evalStack.push(evaluateArrayRead(tokens, i, consumedThrough));
            i = consumedThrough;
            continue;
        }

        if (isOperator(tok))
        {
            if (tok == "!")
            {
                if (evalStack.empty()) throw std::runtime_error("Invalid postfix expression near '!'.");
                Value operand = evalStack.top();
                evalStack.pop();
                Value result;
                result.type = ValueType::BOOL;
                result.boolValue = !toBool(operand);
                evalStack.push(result);
                continue;
            }

            if (evalStack.size() < 2)
            {
                throw std::runtime_error("Invalid postfix expression near operator: " + tok);
            }

            Value rhs = evalStack.top();
            evalStack.pop();
            Value lhs = evalStack.top();
            evalStack.pop();
            evalStack.push(applyOperator(tok, lhs, rhs));
            continue;
        }

        if (isIdentifierLike(tok))
        {
            evalStack.push(lookupVariable(tok));
            continue;
        }

        throw std::runtime_error("Unexpected expression token: " + tok);
    }

    if (evalStack.empty())
    {
        return Value{};
    }

    return evalStack.top();
}

Value Interpreter::applyOperator(const std::string& op, const Value& lhs, const Value& rhs)
{
    Value result;

    if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%")
    {
        result.type = ValueType::INT;
        const int a = toInt(lhs);
        const int b = toInt(rhs);

        if (op == "+") result.intValue = a + b;
        else if (op == "-") result.intValue = a - b;
        else if (op == "*") result.intValue = a * b;
        else if (op == "/") result.intValue = a / b;
        else result.intValue = a % b;
        return result;
    }

    if (op == "<" || op == ">" || op == "<=" || op == ">=" || op == "==" || op == "!=")
    {
        result.type = ValueType::BOOL;
        const int a = toInt(lhs);
        const int b = toInt(rhs);

        if (op == "<") result.boolValue = a < b;
        else if (op == ">") result.boolValue = a > b;
        else if (op == "<=") result.boolValue = a <= b;
        else if (op == ">=") result.boolValue = a >= b;
        else if (op == "==") result.boolValue = a == b;
        else result.boolValue = a != b;
        return result;
    }

    if (op == "&&" || op == "||")
    {
        result.type = ValueType::BOOL;
        if (op == "&&") result.boolValue = toBool(lhs) && toBool(rhs);
        else result.boolValue = toBool(lhs) || toBool(rhs);
        return result;
    }

    throw std::runtime_error("Unknown operator: " + op);
}

std::vector<Value> Interpreter::evaluateCallArguments(const std::vector<std::string>& tokens,
                                                      size_t openParenIndex,
                                                      size_t closeParenIndex)
{
    std::vector<Value> args;
    if (openParenIndex >= tokens.size() || closeParenIndex <= openParenIndex)
    {
        return args;
    }

    std::vector<std::string> currentArg;
    int parenDepth = 0;
    int bracketDepth = 0;

    for (size_t i = openParenIndex + 1; i < closeParenIndex; ++i)
    {
        if (tokens[i] == "(" ) ++parenDepth;
        else if (tokens[i] == ")" ) --parenDepth;
        else if (tokens[i] == "[" ) ++bracketDepth;
        else if (tokens[i] == "]" ) --bracketDepth;

        if (tokens[i] == "," && parenDepth == 0 && bracketDepth == 0)
        {
            if (!currentArg.empty())
            {
                args.push_back(evaluatePayloadExpression(currentArg));
                currentArg.clear();
            }
            continue;
        }

        currentArg.push_back(tokens[i]);
    }

    if (!currentArg.empty())
    {
        args.push_back(evaluatePayloadExpression(currentArg));
    }

    return args;
}

Value Interpreter::evaluateArrayRead(const std::vector<std::string>& tokens,
                                     size_t identifierIndex,
                                     size_t& consumedThrough)
{
    const std::string name = tokens[identifierIndex];
    size_t close = identifierIndex + 1;
    int depth = 0;

    for (; close < tokens.size(); ++close)
    {
        if (tokens[close] == "[") ++depth;
        else if (tokens[close] == "]")
        {
            --depth;
            if (depth == 0) break;
        }
    }

    if (close >= tokens.size())
    {
        throw std::runtime_error("Unclosed array index for: " + name);
    }

    std::vector<std::string> indexTokens(tokens.begin() + static_cast<long>(identifierIndex + 2),
                                         tokens.begin() + static_cast<long>(close));
    int index = toInt(evaluatePayloadExpression(indexTokens));
    Value arrayValue = lookupVariable(name);

    if (arrayValue.type != ValueType::STRING)
    {
        throw std::runtime_error("Indexing non-string/char-array variable: " + name);
    }

    char c = '\0';
    if (index >= 0 && static_cast<size_t>(index) < arrayValue.stringValue.size())
    {
        c = arrayValue.stringValue[static_cast<size_t>(index)];
    }

    Value result;
    result.type = ValueType::CHAR;
    result.charValue = c;
    result.intValue = static_cast<unsigned char>(c);
    consumedThrough = close;
    return result;
}

Value Interpreter::lookupVariable(const std::string& name) const
{
    for (auto it = frames.rbegin(); it != frames.rend(); ++it)
    {
        auto found = it->find(name);
        if (found != it->end())
        {
            return found->second;
        }
    }

    throw std::runtime_error("Undefined variable: " + name);
}

void Interpreter::assignVariable(const std::string& name, const Value& value)
{
    for (auto it = frames.rbegin(); it != frames.rend(); ++it)
    {
        auto found = it->find(name);
        if (found != it->end())
        {
            found->second = value;
            return;
        }
    }

    if (frames.empty())
    {
        frames.push_back({});
    }

    frames.back()[name] = value;
}

void Interpreter::declareVariable(const std::string& name, const Value& value)
{
    if (frames.empty())
    {
        frames.push_back({});
    }

    frames.back()[name] = value;
}

ASTNode* Interpreter::findMatchingEndBlock(ASTNode* beginBlockNode) const
{
    if (beginBlockNode == nullptr || beginBlockNode->label != "BEGIN BLOCK")
    {
        return nullptr;
    }

    int depth = 0;
    ASTNode* current = beginBlockNode;

    while (current != nullptr)
    {
        if (current->label == "BEGIN BLOCK") ++depth;
        else if (current->label == "END BLOCK")
        {
            --depth;
            if (depth == 0) return current;
        }

        current = current->rightSibling;
    }

    return nullptr;
}

ASTNode* Interpreter::nodeAfterBlock(ASTNode* beginBlockNode) const
{
    ASTNode* end = findMatchingEndBlock(beginBlockNode);
    return end == nullptr ? nullptr : end->rightSibling;
}

bool Interpreter::isRoutineDeclaration(const ASTNode* node) const
{
    if (node == nullptr || node->label != "DECLARATION" || node->payload.empty())
    {
        return false;
    }

    return node->payload[0] == "function" ||
           node->payload[0] == "procedure" ||
           node->payload[0] == "main";
}

Interpreter::Routine Interpreter::parseRoutineDeclaration(ASTNode* declarationNode) const
{
    Routine routine;
    routine.declarationNode = declarationNode;

    const std::vector<std::string>& p = declarationNode->payload;
    if (p.empty())
    {
        return routine;
    }

    if (p[0] == "function")
    {
        routine.isFunction = true;
        if (p.size() > 2) routine.name = p[2];
    }
    else if (p[0] == "procedure")
    {
        routine.isFunction = false;
        if (p.size() > 1) routine.name = p[1];
    }
    else if (p[0] == "main")
    {
        routine.isFunction = false;
        routine.name = "main";
    }

    size_t open = p.size();
    size_t close = p.size();
    for (size_t i = 0; i < p.size(); ++i)
    {
        if (p[i] == "(" && open == p.size()) open = i;
        else if (p[i] == ")") { close = i; break; }
    }

    if (open < close)
    {
        for (size_t i = open + 1; i < close; ++i)
        {
            if (isDatatype(p[i]) && p[i] != "void" && i + 1 < close && isIdentifierLike(p[i + 1]))
            {
                routine.parameterNames.push_back(p[i + 1]);
                ++i;
            }
        }
    }

    return routine;
}

bool Interpreter::isInteger(const std::string& text) const
{
    if (text.empty()) return false;

    size_t start = 0;
    if (text[0] == '-' || text[0] == '+')
    {
        if (text.size() == 1) return false;
        start = 1;
    }

    for (size_t i = start; i < text.size(); ++i)
    {
        if (!std::isdigit(static_cast<unsigned char>(text[i]))) return false;
    }

    return true;
}

bool Interpreter::isOperator(const std::string& text) const
{
    return text == "+"  || text == "-"  || text == "*"  || text == "/"  ||
           text == "%"  || text == "<"  || text == ">"  || text == "<=" ||
           text == ">=" || text == "==" || text == "!=" || text == "&&" ||
           text == "||" || text == "!";
}

bool Interpreter::isIdentifierLike(const std::string& text) const
{
    if (text.empty()) return false;
    if (!(std::isalpha(static_cast<unsigned char>(text[0])) || text[0] == '_')) return false;

    for (char c : text)
    {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return false;
    }

    return !isDatatype(text) && text != "function" && text != "procedure" &&
           text != "return" && text != "if" && text != "else" && text != "while" &&
           text != "for" && text != "printf" && text != "void";
}

bool Interpreter::isDatatype(const std::string& text) const
{
    return text == "int" || text == "char" || text == "bool" ||
           text == "string" || text == "void";
}

int Interpreter::toInt(const Value& value) const
{
    switch (value.type)
    {
        case ValueType::INT: return value.intValue;
        case ValueType::BOOL: return value.boolValue ? 1 : 0;
        case ValueType::CHAR: return static_cast<unsigned char>(value.charValue);
        case ValueType::STRING: return value.stringValue.empty() ? 0 : std::stoi(value.stringValue);
        case ValueType::NONE: return 0;
    }

    return 0;
}

bool Interpreter::toBool(const Value& value) const
{
    switch (value.type)
    {
        case ValueType::BOOL: return value.boolValue;
        case ValueType::INT: return value.intValue != 0;
        case ValueType::CHAR: return value.charValue != '\0';
        case ValueType::STRING: return !value.stringValue.empty();
        case ValueType::NONE: return false;
    }

    return false;
}

std::string Interpreter::toPrintableString(const Value& value) const
{
    switch (value.type)
    {
        case ValueType::INT: return std::to_string(value.intValue);
        case ValueType::BOOL: return value.boolValue ? "true" : "false";
        case ValueType::CHAR: return std::string(1, value.charValue);
        case ValueType::STRING: return value.stringValue;
        case ValueType::NONE: return "";
    }

    return "";
}

std::string Interpreter::decodeEscapes(const std::string& text) const
{
    std::string result;

    for (size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == '\\' && i + 1 < text.size())
        {
            const char next = text[i + 1];
            if (next == 'n')
            {
                result.push_back('\n');
                ++i;
            }
            else if (next == 't')
            {
                result.push_back('\t');
                ++i;
            }
            else if (next == '0')
            {
                result.push_back('\0');
                ++i;
            }
            else if (next == 'x')
            {
                size_t j = i + 2;
                int value = 0;
                bool sawHex = false;
                while (j < text.size() && std::isxdigit(static_cast<unsigned char>(text[j])))
                {
                    sawHex = true;
                    value *= 16;
                    if (text[j] >= '0' && text[j] <= '9') value += text[j] - '0';
                    else if (text[j] >= 'a' && text[j] <= 'f') value += 10 + text[j] - 'a';
                    else if (text[j] >= 'A' && text[j] <= 'F') value += 10 + text[j] - 'A';
                    ++j;
                }
                if (sawHex)
                {
                    result.push_back(static_cast<char>(value));
                    i = j - 1;
                }
                else
                {
                    result.push_back(next);
                    ++i;
                }
            }
            else
            {
                result.push_back(next);
                ++i;
            }
        }
        else
        {
            result.push_back(text[i]);
        }
    }

    return result;
}

std::string Interpreter::stringUntilNull(const std::string& text) const
{
    size_t pos = text.find('\0');
    if (pos == std::string::npos)
    {
        return text;
    }
    return text.substr(0, pos);
}
