CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude

TARGET = main

SRC = src/main.cpp \
      src/p1_remove_comments.cpp \
      src/p2_tokenizer.cpp \
      src/p3_parser.cpp \
      src/p4_symbol_table.cpp

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
	rm -f *:Zone.Identifier
	rm -f inputs/*:Zone.Identifier
	rm -f inputs/p5/*:Zone.Identifier

.PHONY: all clean
