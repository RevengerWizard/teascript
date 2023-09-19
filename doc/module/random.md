The `random` module provides functions for generating random numbers and selecting random elements.

# random.seed

The `random.seed` function sets the seed value for the random number generator. This allows you to generate reproducible sequences of random numbers.

## Syntax

```tea
random.seed(x)
```

#### Parameters

-   `x`: An optional number value used as the seed for the random number generator. If not, it defaults to `time.time`

#### Return Value

None.

#### Example

```tea
import random

// Set the random seed to a fixed value
random.seed(1234)

// Generate a random number
print(random.random())  // [random number]

// Generate another random number with the same seed
random.seed(1234)
print(random.random())  // [same random number as above]
```

# random.random

The `random.random` function returns a random number between 0.0 and 1.0.

#### Syntax

```tea
random.random()
```

#### Return Value

A random number between 0.0 and 1.0.

#### Example

Here are some examples of how to use the `random.random` function:

```tea
import random

// Generate a random float between 0.0 and 1.0
var random_float = random.random()
print(random_float)  // [random float between 0.0 and 1.0]
```

# random.range

The `random.range` function returns a random integer within a specified range.

#### Syntax

```
random.range(start, stop)
random.range(range)
```

#### Parameters

-   `start`: The starting value of the range.
-   `stop`: The ending value of the range.

-   `range`: A range object.

#### Return Value

A random integer within the specified range.

#### Example

Here are some examples of how to use the `random.range` function:

```tea
import random

// Generate a random integer between 0 and 9
var random = random.range(0, 10)
print(random)  // [random number between 0 and 9]

// Generate a random number from range
random = random.range(1..11)
print(random)  // [random number between 1 and 10]
```

# random.choice

The `random.choice` function selects a random element from a list.

#### Syntax

```tea
random.choice(seq)
```

#### Parameters

-   `seq`: A list of elements to select from.

#### Return Value

A random element from the list.

#### Example

```tea
import random

// Select a random element from a list of integers
var random = random.choice([1, 2, 3, 4, 5])
print(random)  // [random element from the list]

// Select a random element from a list of strings
random = random.choice(['a', 'b', 'c', 'd', 'e'])
print(random)  // [random element from the list]
```

# random.shuffle

The `random.shuffle` function shuffles the elements of a list in place.

#### Syntax

```tea
random.shuffle(seq)
```

#### Parameters

-   `seq`: A list of elements to shuffle.

#### Return Value

None. The list is shuffled in place.

#### Example

```tea
import random

// Shuffle a list of integers
var numbers = [1, 2, 3, 4, 5]
random.shuffle(numbers)
print(numbers)  // [shuffled list of integers]

// Shuffle a list of strings
var letters = ['a', 'b', 'c', 'd', 'e']
random.shuffle(letters)
print(letters)  // [shuffled list of strings]
```