-- Test Luau type annotations
local x: number = 42
local y: number = 10
local z: number = x + y

print("With type annotations:", z)

function add(a: number, b: number): number
    return a + b
end

local result: number = add(5, 7)
print("Function with types:", result)
