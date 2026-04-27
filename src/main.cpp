#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include <vector>

#include "p1_remove_comments.h"
#include "p2_tokenizer.h"
#include "p3_parser.h"
#include "p4_symbol_table.h"

std::string makeOutputName(const std::string& inputPath) {
    std::filesystem::path inPath(inputPath);
    std::string stem = inPath.stem().string();

    std::filesystem::path outPath =
        std::filesystem::path("outputs/p4") /
        ("output-" + stem + ".txt");

    return outPath.string();
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input-file> [output-file]\n";
        return 1;
    }

    // Assignment 4 test files should come from inputs/p4/
    std::string inputPath = "inputs/p4/" + std::string(argv[1]);
    std::string outputPath = (argc >= 3) ? argv[2] : makeOutputName(inputPath);

    std::ifstream inFile(inputPath);
    if (!inFile) {
        std::cerr << "Error: could not open input file: " << inputPath << "\n";
        return 1;
    }

    std::stringstream buffer;
    buffer << inFile.rdbuf();
    std::string sourceCode = buffer.str();

    // Phase 1: remove comments, preserving line structure
    CommentRemover remover;
    std::string cleaned = remover.removeComments(sourceCode);
    if (cleaned.empty()) {
        return 1;
    }

    // Phase 2: tokenize
    std::vector<Token> tokens;
    int errorLine = -1;
    std::string errorMsg;

    std::istringstream cleanedIn(cleaned);
    Tokenizer tokenizer;

    if (!tokenizer.tokenize(cleanedIn, tokens, errorLine, errorMsg)) {
        std::cout << "Syntax error on line " << errorLine << ": " << errorMsg << "\n";
        return 1;
    }

    // Phase 3: parse CST
    Parser parser(tokens);
    CSTNode* root = nullptr;

    try {
        root = parser.parseProgram();
    } catch (const std::exception& e) {
        std::cerr << "Parse error: " << e.what() << "\n";
        return 1;
    }

    // Phase 4: build symbol table + write assignment 4 output
    std::filesystem::create_directories("outputs/p4");
    std::filesystem::remove(outputPath);

    std::ofstream outFile(outputPath, std::ios::out | std::ios::trunc);
    if (!outFile) {
        std::cerr << "Error: could not create output file: " << outputPath << "\n";
        deleteTree(root);
        return 1;
    }

    SymbolTableBuilder builder;
    bool ok = builder.build(tokens, root, outFile);

    outFile.close();
    deleteTree(root);

    if (!ok) {
        return 1;
    }

    std::cout << "Output file created at: " << outputPath << "\n";
    return 0;
}
