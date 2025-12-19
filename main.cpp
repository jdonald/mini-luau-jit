#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include "ast.h"
#include "interpreter.h"
#include "jit.h"

extern FILE* yyin;
extern int yyparse();
extern BlockNode* programRoot;

void printUsage(const char* progName) {
    std::cerr << "Usage: " << progName << " [--jit] <filename.lua>" << std::endl;
    std::cerr << "  --jit: Enable JIT compilation" << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    bool useJIT = false;
    const char* filename = nullptr;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--jit") == 0) {
            useJIT = true;
        } else if (argv[i][0] != '-') {
            filename = argv[i];
        }
    }

    if (!filename) {
        printUsage(argv[0]);
        return 1;
    }

    yyin = fopen(filename, "r");
    if (!yyin) {
        std::cerr << "Error: Cannot open file " << filename << std::endl;
        return 1;
    }

    if (yyparse() != 0) {
        std::cerr << "Error: Failed to parse " << filename << std::endl;
        fclose(yyin);
        return 1;
    }

    fclose(yyin);

    if (!programRoot) {
        std::cerr << "Error: No program to execute" << std::endl;
        return 1;
    }

    try {
        Interpreter interp;
        if (useJIT) {
            JITCompiler jit(&interp);
            jit.executeJIT(programRoot);
        } else {
            interp.execute(programRoot);
        }
    } catch (const std::exception& e) {
        std::cerr << "Runtime error: " << e.what() << std::endl;
        return 1;
    }

    delete programRoot;
    return 0;
}
