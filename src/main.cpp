#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

#include "p1_remove_comments.h"
#include "p2_tokenizer.h"
#include "p3_parser.h"
#include "p5_ast.h"   // NEW

#include <filesystem>

std::string makeOutputName(const std::string& inputPath) {
    std::filesystem::path inPath(inputPath);
    std::string stem = inPath.stem().string();

    std::filesystem::path outDir = "outputs";
    std::filesystem::create_directories(outDir);

    std::filesystem::path outPath =
        outDir / ("output-" + stem + ".txt");

    return outPath.string();
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: ./main <input_file>\n";
        return 1;
    }

    std::string inputPath = argv[1];
    std::ifstream inFile(inputPath);

    if (!inFile) {
        std::cerr << "Error: Cannot open input file.\n";
        return 1;
    }

    std::stringstream buffer;
    buffer << inFile.rdbuf();
    std::string input = buffer.str();

    // ---------------- P1: Remove Comments ----------------
    CommentRemover remover;
    std::string noComments = remover.removeComments(input);

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

    // ---------------- P3: Parse (CST) ----------------
    Parser parser(tokens);
    CSTNode* cstRoot = nullptr;

    try {
        cstRoot = parser.parseProgram();
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }

    // ---------------- P5: Build AST ----------------
    ASTBuilder astBuilder;
    ASTNode* astRoot = astBuilder.build(tokens, cstRoot);

    // ---------------- Output ----------------
    std::string outputPath = makeOutputName(inputPath);
    std::ofstream outFile(outputPath);

    if (!outFile) {
        std::cerr << "Error: Cannot create output file.\n";
        return 1;
    }

    printASTBreadthFirst(astRoot, outFile);

    // ---------------- Cleanup ----------------
    deleteTree(cstRoot);   // from p3_parser.h :contentReference[oaicite:0]{index=0}
    deleteAST(astRoot);

    return 0;
}