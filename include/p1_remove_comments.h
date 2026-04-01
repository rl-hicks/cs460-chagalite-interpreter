#ifndef P1_COMMENT_REMOVER_H
#define P1_COMMENT_REMOVER_H

#include <string>

class CommentRemover {
public:
    // Removes comments from the input source code.
    // Preserves all non-comment characters and line structure.
    std::string removeComments(const std::string& input);

private:
    // DFA states for comment removal
    enum class State {
        NORMAL,
        SLASH_SEEN,
        LINE_COMMENT,
        BLOCK_COMMENT,
        BLOCK_COMMENT_STAR,
        STRING_LITERAL,
        CHAR_LITERAL,
        ESCAPE_SEQUENCE,
        FORWARD_SLASH, 
        ASTERISK
    };
};

#endif // P1_COMMENT_REMOVER_H



// enum CommentRemoverDFAStates { Normal, ForewardSlash, LineComment, BlockComment, Asterisk, String };

