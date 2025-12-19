#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo "======================================="
echo "Mini-Luau-JIT Benchmark Suite"
echo "======================================="
echo ""

# Check if standard lua is available
if ! command -v lua &> /dev/null; then
    echo -e "${YELLOW}Warning: Standard lua not found. Will only test our implementation.${NC}"
    HAS_LUA=false
else
    HAS_LUA=true
    echo "Standard lua version:"
    lua -v
    echo ""
fi

run_benchmark() {
    local name=$1
    local file=$2

    echo -e "${BLUE}Running benchmark: $name${NC}"
    echo "-----------------------------------"

    if [ "$HAS_LUA" = true ]; then
        echo "Standard Lua (interpreted):"
        time lua "$file" 2>&1
        echo ""
    fi

    echo "Our implementation (interpreted):"
    time ./luau "$file" 2>&1
    echo ""

    echo "Our implementation (JIT):"
    time ./luau --jit "$file" 2>&1
    echo ""

    echo "-----------------------------------"
    echo ""
}

# Build if needed
if [ ! -f "./luau" ]; then
    echo "Building luau..."
    make clean && make
    echo ""
fi

# Run benchmarks
run_benchmark "Arithmetic Operations" "benchmarks/arithmetic.lua"
run_benchmark "Fibonacci" "benchmarks/fibonacci.lua"

echo "======================================="
echo "Benchmark Complete"
echo "======================================="
