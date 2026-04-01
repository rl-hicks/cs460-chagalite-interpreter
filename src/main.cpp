#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>


#include "p1_remove_comments.h"
#include "p2_tokenizer.h"
#include "p3_parser.h"


std::string makeOutputName(const std::string& inputPath) {
    std::filesystem::path inPath(inputPath);

    std::string stem = inPath.stem().string();  
    // programming_assignment_2-test_file_1

    std::filesystem::path outPath =
        std::filesystem::path("outputs/p3") /
        ("output-" + stem + ".txt");

    return outPath.string();
}

/*
std::string makeOutputName(const std::string& inputPath) {
    std::filesystem::path inPath(inputPath);

    std::string stem = inPath.stem().string();

    return "output-" + stem + ".txt";
}
*/

// Converts enum to string
std::string tokenTypeToString(ChagaLiteTokens type) {
    switch (type) {

        // Identifiers & keywords
        case IDENTIFIER: return "IDENTIFIER";
        case KEYWORD: return "KEYWORD";

        // Literals
        case INTEGER: return "INTEGER";
        case TOKEN_STRING: return "STRING";

        // Operators
        case ASSIGN: return "ASSIGNMENT_OPERATOR";
        case PLUS: return "PLUS";
        case MINUS: return "MINUS";
        case ASTERISK: return "ASTERISK";
        case DIVIDE: return "DIVIDE";
        case MODULO: return "MODULO";
        case CARET: return "CARET";
        case LT: return "LT";
        case GT: return "GT";
        case LE: return "LE";
        case GE: return "GE";
        case EQUALS: return "EQUALS";
        case NOT_EQUALS: return "NOT_EQUALS";
        case AND_AND: return "AND_AND";
        case OR_OR: return "OR_OR";
        case NOT: return "NOT";

        // Delimiters
        case LPAREN: return "L_PAREN";
        case RPAREN: return "R_PAREN";
        case LBRACKET: return "L_BRACKET";
        case RBRACKET: return "R_BRACKET";
        case LBRACE: return "L_BRACE";
        case RBRACE: return "R_BRACE";
        case COMMA: return "COMMA";
        case SEMICOLON: return "SEMICOLON";

        // Quotes
        case DQUOTE: return "DOUBLE_QUOTE";
        case SQUOTE: return "SINGLE_QUOTE";

        // Special
        case TOKEN_END_OF_FILE: return "END_OF_FILE";
        case TOKEN_UNKNOWN: return "UNKNOWN";

        default:
            return "UNKNOWN";
    }
}



void printTokens(const std::vector<Token>& tokens, std::ostream& out) {
    out << "Token list:\n\n";

    for (const Token& tok : tokens) {

        std::string typeStr =
            (tok.type == KEYWORD) ? "IDENTIFIER"
                                  : tokenTypeToString(tok.type);

        // Expand quoted STRING into 3 tokens (to match expected output)
        if (typeStr == "STRING") {
            const std::string& s = tok.content;

            if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {

                out << "Token type: DOUBLE_QUOTE\n";
                out << "Token:      \"\n\n";

                out << "Token type: STRING\n";
                out << "Token:      " << s.substr(1, s.size() - 2) << "\n\n";

                out << "Token type: DOUBLE_QUOTE\n";
                out << "Token:      \"\n\n";

                continue;
            }
        }

        out << "Token type: " << typeStr << "\n";
        out << "Token:      " << tok.content << "\n\n";
    }
}




int main(int argc, char* argv[])
{
	// Makes sure there's an inputfile
    if (argc < 2) {
        std::cerr << "uh-oh: " << argv[0];
        return 1;
    }

	// input == file name is second argument
	// inputFile == inputPath
	std::string inputPath = "inputs/p3/" + std::string(argv[1]);    
    //std::cout << inputPath << std::endl;

    std::string outputPath;

	// if output file is given use it, otherwise create it
    if (argc >= 3) {
        outputPath = argv[2];
    } else {
        outputPath = makeOutputName(inputPath);
    }

	// creates a input file stream obj for the input file
    std::ifstream inFile(inputPath);
    if (!inFile) {
        std::cerr << "Error: could not open input file: " << inputPath << "\n";
        return 1;
    }

	// puts file contents into a string
    std::stringstream buffer;
    buffer << inFile.rdbuf();
    std::string sourceCode = buffer.str();
    
    // creates CommentRemover class obj and calls assignment 1
    // puts the "cleaned" contents into string called cleaned
	CommentRemover remover;
	std::string cleaned = remover.removeComments(sourceCode);
	if (cleaned.empty()) {
		return 1;
	}
	



	// =================================================================
	// assignment 1
	// puts the contents of cleaned into output file
    //outFile << cleaned;
    //outFile.close();
    // =================================================================
    
    
    
    // =================================================================
	// Assignment 2
    std::vector<Token> tokens;
	int errorLine;
	std::string errorMsg;
	
	std::istringstream cleanedIn(cleaned);
	
	Tokenizer tokenizer;

	if (!tokenizer.tokenize(cleanedIn, tokens, errorLine, errorMsg)) {
		std::cout << "Syntax error on line " << errorLine << ": " << errorMsg << "\n";
		return 1; // no tokens printed
	}
	// =================================================================
	
	
	// make sure outputs/p2 folder exists
	std::filesystem::create_directories("outputs/p2");
	
	std::filesystem::remove(outputPath);

	// open output file and overwrite it every run
	//std::ofstream outFile(outputPath, std::ios::trunc);
	std::ofstream outFile(outputPath, std::ios::out | std::ios::trunc);
	if (!outFile) {
		std::cerr << "Error: could not create output file: "
				<< outputPath << "\n";
		return 1;
	}






	printTokens(tokens, outFile);
	outFile.close();
	std::cout << "Output file created at: " << outputPath << "\n";

    // printTokens(tokens, std::cout);
    
    
    
    return 0;

    
}
