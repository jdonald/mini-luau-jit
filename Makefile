CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2
LDFLAGS =

TARGET = luau
SOURCES = main.cpp interpreter.cpp native_jit.cpp codegen_x86_64.cpp codegen_arm64.cpp parser.tab.cpp lexer.yy.cpp
OBJECTS = $(SOURCES:.cpp=.o)
HEADERS = ast.h interpreter.h native_jit.h codegen.h

all: $(TARGET)

# Parser generation (requires bison)
parser.tab.cpp parser.tab.hpp: parser.y
	bison -d -o parser.tab.cpp parser.y

# Note: lexer.yy.cpp is now a hand-written lexer, not generated from lexer.l

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

parser.tab.o: parser.tab.cpp $(HEADERS) parser.tab.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

lexer.yy.o: lexer.yy.cpp $(HEADERS) parser.tab.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.cpp $(HEADERS) parser.tab.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJECTS) parser.tab.cpp parser.tab.hpp *.o

benchmark: $(TARGET)
	@echo "Running benchmarks..."
	@./benchmark.sh

.PHONY: all clean benchmark
