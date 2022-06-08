function fib(n)
	if n < 2 then
		return n
	end

	return fib(n - 2) + fib(n - 1)
end

local start = os.clock()

io.write(fib(35).."\n")

io.write(string.format("elapsed: %g\n", os.clock() - start))