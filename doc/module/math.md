The `math` module provides a variety of mathematical functions and constants for use.

#### Constants

-   `math.pi`: The value of the constant pi (3.14159...), the ratio of the circumference of a circle to its diameter.
-   `math.tau`: The value of tau (6.28319...), which is equal to 2 * pi.
-   `math.e`: The value of the constant e (2.71828...), the base of the natural logarithm.
-   `math.phi`: The value of the golden ratio (1.618033...), a mathematical constant that appears in various areas of mathematics and physics.

# math.min

The `math.min` function returns the minimum of its arguments.

#### Syntax

```tea
math.min(x, y, ...)
math.min(iterable)
```

#### Parameters

-   `x` : The first number to compare.
-   `y` : The second number to compare.
-   `...`: Additional numbers to compare.

-   `iterable`: An iterable of numbers to compare.

#### Return value

The minimum of the arguments.

#### Example

```tea
import math

var value = math.min(5, 10)
print(value)

var numbers = [5, 10, 20, 30]
value = math.min(numbers)
print(value)

value = math.min(5..11)
print(value)
```

# math.max

The `math.max` function returns the maximum of its arguments.

#### Syntax

```tea
math.max(x, y, ...)
math.max(iterable)
```

#### Parameters

-   `x` : The first number to compare.
-   `y` : The second number to compare.
-   `...`: Additional numbers to compare.

-   `iterable`: An iterable of numbers to compare.

#### Return value

The maximum of the arguments.

#### Example

```tea
import math

var value = math.max(5, 10)
print(value)

var numbers = [5, 10, 20, 30]
value = math.max(numbers)
print(value)

value = math.min(5..11)
print(value)
```

# math.mid

The `math.mid` function returns the midpoint of its arguments.

#### Syntax

```tea
math.mid(x, y, z)
math.max(numbers)
```

#### Parameters

-   `x`: The first number to compare.
-   `y`: The second number to compare.
-   `z`: The third number to compare.

-   `numbers`: A list of three numbers to compare.

#### Return value

The midpoint of the arguments.

#### Example

```tea
import math

var middle = math.mid(5, 10, 15)
print(middle)

var numbers = [5, 10, 15]
middle = math.mid(numbers)
print(middle)
```

# math.sum

The `math.sum` function returns the sum of its arguments.

#### Syntax

```tea
math.sum(x, y, ...)
math.sum(iterable)
```

#### Parameters

-   `x`: The first number to add.
-   `y`: The second number to add.
-   `...`: Additional numbers to add.

-   `iterable`: An iterable of numbers to add.

#### Return value

The sum of the arguments.

#### Example

```tea
import math

var total = math.sum(5, 10)
print(total)

var numbers = [5, 10, 20, 30]
total = math.sum(numbers)
print(total)

total = math.sum(5..11)
print(total)
```

# math.floor

The `math.floor` function returns the largest integer less than or equal to the argument.

#### Syntax

```tea
math.floor(x)
```

#### Parameters

-   `x`: The number to round down.

#### Return value

The largest integer less than or equal to the argument.

#### Example

```tea
import math

var rounded = math.floor(5.8)
print(rounded)

rounded = math.floor(-5.8)
print(rounded)

rounded = math.floor(5)
print(rounded)
```

# math.ceil

The `math.ceil` function returns the smallest integer greater than or equal to the argument.

#### Syntax

```tea
math.ceil(x)
```

#### Parameters

-   `x`: The number to round up.

#### Return value

The smallest integer greater than or equal to the argument.

#### Example

```tea
import math

var rounded = math.ceil(5.2)
print(rounded)

rounded = math.ceil(-5.2)
print(rounded)

rounded = math.ceil(5)
print(rounded)
```

# math.round

The `math.round` function returns the nearest integer to the argument. If the argument is exactly halfway between two integers, it rounds to the nearest even integer.

#### Syntax

```tea
math.round(x)
```

#### Parameters

-   `x`: The number to round.

#### Return value

The nearest integer to the argument.

#### Example

```tea
import math

var rounded = math.round(5.5)
print(rounded)

rounded = math.round(-5.5)
print(rounded)

rounded = math.round(5)
print(rounded)
```

# math.acos

The `math.acos` function returns the inverse cosine (in radians) of the argument.

#### Syntax

```tea
math.acos(x)
```

#### Parameters

-   `x`: The number to find the inverse cosine of.

#### Return value

The inverse cosine (in radians) of the argument.

#### Example

```tea
import math

// Find the inverse cosine of a number
var radians = math.acos(0.5)
print(radians)  // 1.047197551196598

// Find the inverse cosine of a negative number
radians = math.acos(-0.5)
print(radians)  // 2.094395102393196

// Find the inverse cosine of an integer
radians = math.acos(1)
print(radians)  // 0
```

# math.cos

The `math.cos` function returns the cosine of the argument (in radians).

#### Syntax

```tea
math.cos(x)
```

#### Parameters

-   `x`: The number (in radians) to find the cosine of.

#### Return Value

The cosine of the argument.

#### Example

```tea
import math

// Find the cosine of a number (in radians)
var cosine = math.cos(1.0471975511965979)
print(cosine)  // 0.4999999999999999
```

# math.asin

The `math.asin` function returns the inverse sine (in radians) of the argument.

#### Syntax

```tea
math.asin(x)
```

#### Parameters

-   `x`: The number to find the inverse sine of.

#### Return Value

The inverse sine (in radians) of the argument.

#### Example

```tea
import math

// Find the inverse sine of a number
var radians = math.asin(0.5)
print(radians)  // 0.5235987755982989

// Find the inverse sine of a negative number
radians = math.asin(-0.5)
print(radians)  // -0.5235987755982989

// Find the inverse sine of an integer
radians = math.asin(1)
print(radians)  // 1.5707963267948966
```

# math.sin

The `math.sin` function returns the sine of the argument (in radians).

#### Syntax

```tea
math.sin(x)
```

#### Parameters

-   `x`: The number (in radians) to find the sine of.

#### Return Value

The sine of the argument.

#### Example

```tea
import math

// Find the sine of a number (in radians) 
sine = math.sin(0.5235987755982989) 
print(sine)  // 0.5  

// Find the sine of a negative number (in radians) sine = math.sin(-0.5235987755982989) print(sine)  // -0.5  

// Find the sine of an integer (in radians) 
sine = math.sin(1) 
print(sine)  // 0.8414709848078965
```

# math.atan

The `math.atan` function returns the inverse tangent (in radians) of the argument.

#### Syntax

```tea
math.atan(x)
```

#### Parameters

-   `x`: The number to find the inverse tangent of.

#### Return Value

The inverse tangent (in radians) of the argument.

#### Example

```tea
import math

// Find the inverse tangent of a number
var radians = math.atan(1)
print(radians)  // 0.7853981633974483

// Find the inverse tangent of a negative number
radians = math.atan(-1)
print(radians)  // -0.7853981633974483

// Find the inverse tangent of an integer
radians = math.atan(0)
print(radians)  // 0.0
```

# math.atan2

The `math.atan2` function returns the inverse tangent (in radians) of the quotient of its arguments.

#### Syntax

```
math.atan2(y, x)
```

#### Parameters

-   `y`: The numerator of the quotient.
-   `x`: The denominator of the quotient.

#### Return Value

The inverse tangent (in radians) of the quotient of the arguments.

#### Example

```tea
import math

// Find the inverse tangent of a quotient
radians = math.atan2(1, 1)
print(radians)  // 0.7853981633974483

// Find the inverse tangent of a negative quotient
radians = math.atan2(-1, -1)
print(radians)  // -2.356194490192345

// Find the inverse tangent of a quotient where the numerator is 0
radians = math.atan2(0, 1)
print(radians)  // 0

// Find the inverse tangent of a quotient where the denominator is 0
radians = math.atan2(1, 0)
print(radians)  // 1.5707963267948966
```

# math.tan

The `math.tan` function returns the tangent of the argument (in radians).

#### Syntax

```tea
teamath.tan(x)
```

#### Parameters

-   `x`: The number (in radians) to find the tangent of.

#### Return Value

The tangent of the argument.

#### Example

```tea
import math

// Find the tangent of a number (in radians)
var tangent = math.tan(0.7853981633974483)
print(tangent)  // 1.0

// Find the tangent of a negative number (in radians)
tangent = math.tan(-0.7853981633974483)
print(tangent)  // -1.0

// Find the tangent of an integer (in radians)
tangent = math.tan(1)
print(tangent)  // 1.5574077246549023
```

# math.sign

The `math.sign` function returns the sign of the argument.

#### Syntax

```tea
math.sign(x)
```

#### Parameters

-   `x`: The number to find the sign of.

#### Return Value

The sign of the argument: 1 if the argument is positive, -1 if the argument is negative, or 0 if the argument is 0.

#### Example

```tea
import math

// Find the sign of a positive number
var sign = math.sign(5)
print(sign)  // 1

// Find the sign of a negative number
sign = math.sign(-5)
print(sign)  // -1

// Find the sign of 0
sign = math.sign(0)
print(sign)  // 0
```

# math.abs

The `math.abs` function returns the absolute value of the argument.

#### Syntax

```tea
math.abs(x)
```

#### Parameters

-   `x`: The number to find the absolute value of.

#### Return Value

The absolute value of the argument.

#### Example

Here are some examples of how to use the `math.abs` function:

```tea
import math

// Find the absolute value of a positive number
var abs = math.abs(5)
print(abs_value)  // 5

// Find the absolute value of a negative number
abs = math.abs(-5)
print(abs_value)  // 5

// Find the absolute value of 0
abs_value = math.abs(0)
print(abs_value)  // 0
```

# math.sqrt

The `math.sqrt` function returns the square root of the argument.

#### Syntax

```tea
math.sqrt(x)
```

#### Parameters

-   `x`: The number to find the square root of.

#### Return Value

The square root of the argument.

#### Example

```tea
import math

// Find the square root of a positive number
var sqrt = math.sqrt(25)
print(sqrt)  // 5

// Find the square root of 0
sqrt = math.sqrt(0)
print(sqrt)  // 0
```

# math.deg

The `math.deg` function converts an angle from radians to degrees.

#### Syntax

```tea
math.deg(x)
```

#### Parameters

-   `x`: The angle in radians to be converted to degrees.

#### Return Value

The angle in degrees.

#### Example

```tea
import math

// Convert an angle from radians to degrees
var degrees = math.deg(1.5707963267948966)
print(degrees)  // 90

// Convert a negative angle from radians to degrees
degrees = math.deg(-1.5707963267948966)
print(degrees)  // -90

// Convert an integer angle from radians to degrees
degrees = math.deg(0)
print(degrees)  // 0
```

# math.rad

The `math.rad` function converts an angle from degrees to radians.

#### Syntax

```tea
math.rad(x)
```

#### Parameters

-   `x`: The angle in degrees to be converted to radians.

#### Return Value

The angle in radians.

#### Example

```tea
import math

// Convert an angle from degrees to radians
var radians = math.rad(90)
print(radians)  // 1.5707963267948966

// Convert a negative angle from degrees to radians
radians = math.rad(-90)
print(radians)  // -1.5707963267948966

// Convert an integer angle from degrees to radians
radians = math.rad(0)
print(radians)  // 0.0
```

# math.exp

The `math.exp` function returns the value of the constant `e` (2.71828...) raised to the power of the argument.

#### Syntax

```
math.exp(x)
```

#### Parameters

-   `x`: The power to raise `e` to.

#### Return Value

The value of `e` raised to the power of the argument.

#### Example

```tea
import math

// Raise e to the power of a positive number
var result = math.exp(5)
print(result)  // 148.4131591025766

// Raise e to the power of a negative number
result = math.exp(-5)
print(result)  // 0.006737946999085467

// Raise e to the power of 0
result = math.exp(0)
print(result)  // 1
```