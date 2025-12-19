# mini-luau-jit

A rudimentary Luau JIT demo (Luau implementation has one extension beyond Lua, just for kicks) written in C/C++

## Functionality

While this repo is named mini-luau-jit, For all intents and purposes this is a *Lua* JIT. Supports only one Luau extension described later.

Lua/Luau functionality is limited to simple arithmetic, basic logic operations, functions (functions can take zero or multiple arguments), and string support.
Supports the built-in `print()` function.
The only types are integers, bools, and strings. Arithmetic functions include addition, subtraction,
multiply, divide, modulo, equality comparison, inequality comparisons. Logic includes AND/OR.

Note although we support, do not yet implement first-class functions that can be used as args (hence functions are not one of the types)

The only Luau extension supported is the `type` keyword, and it just parses it but doesn't do actual type checking.

As we're focused on building a JIT, implement the ability to run a .lua/.luau file, but no need to support a REPL interpreter mode.

## Implementation

Use standard parsing tools flex and bison, Makefiles.

## Building

### Prerequisites

- g++ with C++17 support
- flex
- bison

On Ubuntu/Debian:
```bash
sudo apt-get install g++ flex bison
```

### Build Instructions

```bash
make
```

This will generate the `luau` executable.

To clean build artifacts:
```bash
make clean
```

## Usage

Run a Lua/Luau file:
```bash
./luau <filename.lua>
```

Run with JIT compilation enabled:
```bash
./luau --jit <filename.lua>
```

### Example Programs

Test basic functionality:
```bash
./luau test.lua
```

Test Luau type annotations:
```bash
./luau test_types.luau
```

## Benchmarks

Include at least two benchmarks to compare the JIT's performance against the standard `lua` (non-JIT) implementation
and implement a way to quickly way to run a benchmark and report how performance compares between the two.

As benchmarks must support standard Lua (not Luau), do not include any use of the Luau extension `type` keyword
in the inluded benchmarks.

### Running Benchmarks

Run all benchmarks:
```bash
make benchmark
# or
./benchmark.sh
```

This will run the benchmark suite comparing:
- Standard Lua (interpreted)
- Our implementation (interpreted)
- Our implementation (JIT)

Individual benchmarks can be run directly:
```bash
./luau benchmarks/arithmetic.lua
./luau benchmarks/fibonacci.lua
```

## Language Features

### Supported Features

- **Variables**: `local x = 10` or `x = 10`
- **Arithmetic**: `+`, `-`, `*`, `/`, `%`
- **Comparisons**: `==`, `~=`, `<`, `<=`, `>`, `>=`
- **Logic**: `and`, `or`, `not`
- **Functions**:
  ```lua
  function add(a, b)
      return a + b
  end
  ```
- **Control Flow**: `if`/`then`/`else`, `while`/`do`
- **Types**: integers, booleans, strings
- **Built-in**: `print()`
- **Luau Extension**: Type annotations (parsed but not enforced)
  ```lua
  local x: number = 42
  function add(a: number, b: number): number
      return a + b
  end
  ```

### Not Supported

- First-class functions
- Tables/arrays
- For loops
- Multiple return values
- Metatables
- Coroutines
- Most standard library functions beyond `print()`
