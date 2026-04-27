// p1_comment_remover.cpp
#include "p1_remove_comments.h"

#include <sstream>
#include <iostream>
#include <string>
#include <istream>



std::string CommentRemover::removeComments(const std::string& input) {

    std::istringstream inputFileStream(input);
    std::ostringstream outputFileStream;

    CommentRemover::State state = CommentRemover::State::NORMAL;
    std::istream::int_type chInt;
    char c;
    int open = 1;
    int block = 0;

    while ((chInt = inputFileStream.get()) != std::char_traits<char>::eof()) {
        c = static_cast<char>(chInt);

        switch (state) {

            case State::NORMAL: {
                if (c == '/') {
                    state = State::FORWARD_SLASH;
                }
                else if (c == '"') {
                    state = State::STRING_LITERAL;
                    outputFileStream.put(c);
                }
                else if (c == '*') {
					// Just output the character normally
					outputFileStream.put(c);
				}
                else {
                    outputFileStream.put(c);
                }
                if (c == '\n') { open++; }
                break;
            }

            case State::FORWARD_SLASH: {
                if (c == '/') {
                    outputFileStream.put(' ');
                    outputFileStream.put(' ');
                    state = State::LINE_COMMENT;
                }
                else if (c == '*') {
                    outputFileStream.put(' ');
                    outputFileStream.put(' ');
                    state = State::BLOCK_COMMENT;
                }
                else {
                    outputFileStream.put('/');
                    outputFileStream.put(c);
                    if (c == '\n') ++open;
                    state = State::NORMAL;
                }
                break;
            }

            case State::LINE_COMMENT: {
                if (c == '\n') {
                    outputFileStream.put('\n');
                    ++open;
                    state = State::NORMAL;
                }
                else {
                    outputFileStream.put(' ');
                }
                break;
            }

            case State::BLOCK_COMMENT: {
                if (c == '*') {
                    outputFileStream.put(' ');
                    state = State::ASTERISK;
                }
                else if (c == '\n') {
                    ++block;
                    outputFileStream.put('\n');
                }
                else {
                    outputFileStream.put(' ');
                }
                break;
            }

            case State::ASTERISK: {
                if (c == '/') {
                    outputFileStream.put(' ');
                    open += block;
                    block = 0;
                    state = State::NORMAL;
                }
                else if (c == '*') {
                    outputFileStream.put(' ');
                }
                else if (c == '\n') {
                    ++block;
                    outputFileStream.put('\n');
                    state = State::BLOCK_COMMENT;
                }
                else {
                    outputFileStream.put(' ');
                    state = State::BLOCK_COMMENT;
                }
                break;
            }

            case State::STRING_LITERAL: {
                if (c == '"') {
                    outputFileStream.put('"');
                    state = State::NORMAL;
                    break;
                }
                outputFileStream.put(c);
                break;
            }

            default:
                outputFileStream.put(c);
                break;
        }
    }

    if (state == State::NORMAL || state == State::STRING_LITERAL) {
        return outputFileStream.str();
    }
    if (state == State::FORWARD_SLASH && chInt == std::char_traits<char>::eof()) {
        outputFileStream.put('/');
        return outputFileStream.str();
    }
    if (state == State::BLOCK_COMMENT || state == State::ASTERISK) {
        std::cerr << "ERROR: Program contains C-style, unterminated comment on line " << open << std::endl;
        return std::string{};
    }

    return outputFileStream.str();
}

