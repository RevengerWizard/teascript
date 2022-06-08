from __future__ import print_function
import time

def fib(n):
	if n < 2: return n
	return fib(n - 1) + fib(n - 2)

start = time.time()

print(fib(35))

print("elapsed: " + str(time.time() - start))