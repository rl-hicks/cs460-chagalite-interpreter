CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude

TARGET = p6

SRC = src/main.cpp \
      src/p1_remove_comments.cpp \
      src/p2_tokenizer.cpp \
      src/p3_parser.cpp \
      src/p4_symbol_table.cpp \
      src/p5_ast.cpp \
      src/p6_interpreter.cpp

OBJ = $(SRC:src/%.cpp=build/%.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

build/%.o: src/%.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf build/
	rm -f $(TARGET)
	rm -rf outputs/*

.PHONY: all clean
