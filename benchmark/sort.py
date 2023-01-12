from __future__ import print_function
import time
import random

# Map "range" to an efficient range in both Python 2 and 3.
try:
    range = xrange
except NameError:
    pass

start = time.clock()
a = []

for i in range(0, 1000000):
    a.append(random.randint(-10000, 10000))

a.sort()
print("elapsed: " + str(time.clock() - start))