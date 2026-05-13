#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "p1_remove_comments.h"
#include "p2_tokenizer.h"
#include "p3_parser.h"
#include "p4_symbol_table.h"
#include "p5_ast.h"
#include "p6_interpreter.h"

std::string makeOutputName(const std::string& inputPath) {
    std::filesystem::path inPath(inputPath);
    std::string stem = inPath.stem().string();

    std::filesystem::path outDir = "outputs";
    std::filesystem::create_directories(outDir);

    std::filesystem::path outPath = outDir / ("output-" + stem + ".txt");
    return outPath.string();
}

std::filesystem::path resolveInputPath(const std::string& inputArg) {
    std::filesystem::path inputPath(inputArg);

    if (std::filesystem::exists(inputPath)) {
        return inputPath;
    }

    const std::filesystem::path fallbackP6 = std::filesystem::path("inputs/p6") / inputArg;
    if (std::filesystem::exists(fallbackP6)) {
        return fallbackP6;
    }

    const std::filesystem::path fallbackInputs = std::filesystem::path("inputs") / inputArg;
    if (std::filesystem::exists(fallbackInputs)) {
        return fallbackInputs;
    }

    return inputPath;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: ./p6 <input_file>\n";
        return 1;
    }

    const std::filesystem::path inputPath = resolveInputPath(argv[1]);

    std::ifstream inFile(inputPath);
    if (!inFile) {
        std::cerr << "Error: Cannot open input file: " << inputPath << "\n";
        return 1;
    }

    std::stringstream buffer;
    buffer << inFile.rdbuf();
    const std::string input = buffer.str();

    // ---------------- P1: Remove Comments ----------------
    CommentRemover remover;
    const std::string noComments = remover.removeComments(input);

    // ---------------- P2: Tokenize ----------------
    std::istringstream tokenStream(noComments);
    Tokenizer tokenizer;

    std::vector<Token> tokens;
    int errorLine = 0;
    std::string errorMsg;

    if (!tokenizer.tokenize(tokenStream, tokens, errorLine, errorMsg)) {
        std::cerr << "Syntax error on line " << errorLine << ": " << errorMsg << "\n";
        return 1;
    }

    // ---------------- P3: Parse CST ----------------
    Parser parser(tokens);
    CSTNode* cstRoot = nullptr;

    try {
        cstRoot = parser.parseProgram();
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }

    // ---------------- P4: Build Symbol Table ----------------
    SymbolTableBuilder symbolTable;
    std::ostringstream symbolTableOutput;

    if (!symbolTable.build(tokens, cstRoot, symbolTableOutput)) {
        std::cerr << symbolTableOutput.str();
        deleteTree(cstRoot);
        return 1;
    }

    // ---------------- P5: Build AST ----------------
    ASTBuilder astBuilder;
    ASTNode* astRoot = astBuilder.build(tokens, cstRoot);

    if (astRoot == nullptr) {
        std::cerr << "Error: Failed to build AST.\n";
        deleteTree(cstRoot);
        return 1;
    }

    // ---------------- P6: Interpret Program ----------------
    const std::string outputPath = makeOutputName(inputPath.string());
    std::ofstream outFile(outputPath);

    if (!outFile) {
        std::cerr << "Error: Cannot create output file: " << outputPath << "\n";
        deleteAST(astRoot);
        deleteTree(cstRoot);
        return 1;
    }

    Interpreter interpreter;
    const bool executed = interpreter.execute(astRoot, symbolTable, outFile);

    // ---------------- Cleanup ----------------
    deleteAST(astRoot);
    deleteTree(cstRoot);

    if (!executed) {
        std::cerr << "Error: Program interpretation failed.\n";
        return 1;
    }

    return 0;
}
