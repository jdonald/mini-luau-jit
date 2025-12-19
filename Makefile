CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2
LDFLAGS =

TARGET = luau
SOURCES = main.cpp interpreter.cpp jit.cpp parser.tab.cpp lexer.yy.cpp
OBJECTS = $(SOURCES:.cpp=.o)
HEADERS = ast.h interpreter.h jit.h

all: $(TARGET)

parser.tab.cpp parser.tab.hpp: parser.y
	bison -d -o parser.tab.cpp parser.y

lexer.yy.cpp: lexer.l parser.tab.hpp
	flex -o lexer.yy.cpp lexer.l

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

parser.tab.o: parser.tab.cpp $(HEADERS) parser.tab.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

lexer.yy.o: lexer.yy.cpp $(HEADERS) parser.tab.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.cpp $(HEADERS) parser.tab.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJECTS) parser.tab.cpp parser.tab.hpp lexer.yy.cpp

benchmark: $(TARGET)
	@echo "Running benchmarks..."
	@./benchmark.sh

.PHONY: all clean benchmark
