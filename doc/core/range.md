The `range` type represents a sequence of numbers, starting from a start value, up to an end value, with a specified step. It is commonly used to iterate over a sequence of numbers in a for loop.

#### Properties

-   `range.start`: the start value of the range.
-   `range.end`: the end value of the range.
-   `range.step`: the step value of the range.
-   `range.len`: the length of the range.

#### Example

```tea
var my_range = 1..10..2

print(my_range.start)  // 1 
print(my_range.end)  // 10 
print(my_range.step)  // 2

print(my_range.len)   // 4
```

# range.contains

The `range.contains` method checks if a given value is within the range.

#### Syntax

```tea
range.contains(x)
```

#### Parameters

-   `x`: The value to check for membership in the range.

#### Return Value

A boolean value indicating whether `x` is within the range.

#### Example

```tea
var r = 1..10..2
print(r.contains(1))  // true
print(r.contains(2))  // false
print(r.contains(3))  // true
print(r.contains(4))  // false
print(r.contains(5))  // true
```