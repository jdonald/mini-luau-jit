-- Arithmetic benchmark
-- Performs intensive arithmetic operations

function benchmark()
    local sum = 0
    local i = 1
    while i <= 1000000 do
        sum = sum + i
        sum = sum - 1
        sum = sum * 2
        sum = sum / 2
        sum = sum % 1000000
        i = i + 1
    end
    return sum
end

local result = benchmark()
print("Arithmetic benchmark result:", result)
