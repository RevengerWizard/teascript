local a = {}

for i = 0, 1000000 do
	table.insert(a, math.random(-10000, 10000))
end

local start = os.clock()

table.sort(a)

io.write(string.format("elapsed: %g\n", os.clock() - start))