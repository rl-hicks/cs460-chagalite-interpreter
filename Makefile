CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -O0 -g -Iinclude

TARGET := main

SRCS := $(wildcard src/*.cpp)
OBJS := $(patsubst src/%.cpp, build/%.o, $(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET)

# Compile source files into build directory
build/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Ensure build directory exists
build:
	mkdir -p build

clean:
	rm -rf build $(TARGET)

.PHONY: all clean
