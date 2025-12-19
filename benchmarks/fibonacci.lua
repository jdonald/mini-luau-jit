-- Fibonacci benchmark
-- Computes Fibonacci numbers iteratively

function fib(n)
    if n <= 1 then
        return n
    end

    local a = 0
    local b = 1
    local i = 2

    while i <= n do
        local temp = a + b
        a = b
        b = temp
        i = i + 1
    end

    return b
end

function benchmark()
    local sum = 0
    local i = 1
    while i <= 10000 do
        sum = sum + fib(30)
        i = i + 1
    end
    return sum
end

local result = benchmark()
print("Fibonacci benchmark result:", result)
