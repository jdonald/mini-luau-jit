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

## Implementation

Use standard parsing tools flex and bison, Makefiles.

## Benchmarks

Include at least two benchmarks to compare the JIT's performance against the standard `lua` (non-JIT) implementation
and implement a way to quickly way to run a benchmark and report how performance compares between the two.

As benchmarks must support standard Lua (not Luau), do not include any use of the Luau extension `type` keyword
in the inluded benchmarks.
