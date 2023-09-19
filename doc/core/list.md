The `list` type is a mutable sequence type that allows you to store and manipulate a collection of values. It is similar to arrays in other programming languages. Lists are created using square brackets, and elements can be added, removed, or modified using various methods.

#### Properties

-   `list.len`: the number of elements in a list.

#### Example

```tea
var my_list = [1, 2, 3, 4]
print(my_list.len)  // 4
```

# list.add

The `list.add` method adds an element to the end of a list.

#### Syntax

```tea
list.add(element)
```

#### Parameters

-   `element`: The element to add to the end of the list.

#### Return Value

None.

#### Example

```tea
var my_list = [1, 2, 3]
my_list.add(4)
print(my_list)  // [1, 2, 3, 4]
```

# list.remove

The `list.remove` method removes the first occurrence of a value from a list. If the value is not found in the list, an error is raised.

#### Syntax

```tea
list.remove(value)
```

#### Parameters

-   `value`: The value to remove from the list.

#### Return Value

None.

#### Example

```tea
var my_list = [1, 2, 3, 4, 2]
my_list.remove(2)
print(my_list)  // [1, 3, 4, 2]
```

# list.delete

The `list.delete` method removes the element at a specified index from a list. If the index is out of range, an error is raised.

#### Syntax

```tea
list.delete(index)
```

#### Parameters

-   `index`: The index of the element to remove from the list.

#### Return Value

None.

#### Example

```tea
var my_list = [1, 2, 3, 4]
my_list.delete(1)
print(my_list)  // [1, 3, 4]
```

# list.clear

The `list.clear` method removes all elements from a list.

#### Syntax

```tea
list.clear()
```

#### Parameters

None.

#### Return Value

None.

#### Example

```tea
var my_list = [1, 2, 3, 4]
my_list.clear()
print(my_list)  // Output: []
```

# list.insert

The `list.insert` method inserts an element at a specified index in a list. If the index is out of range, an error is raised.

#### Syntax

```tea
list.insert(index, element)
```

#### Parameters

-   `index`: The index at which to insert the element.
-   `element`: The element to insert into the list.

#### Return Value

None.

#### Example

```tea
var my_list = [1, 2, 3]
my_list.insert(1, 4)
print(my_list)  // [1, 4, 2, 3]
```

# list.extend

The `list.extend` method adds all elements of an iterable to the end of a list.

#### Syntax

```tea
list.extend(iterable)
```

#### Parameters

-   `iterable`: The iterable whose elements to add to the end of the list.

#### Return Value

None.

#### Example

```tea
var my_list = [1, 2, 3]
my_list.extend([4, 5, 6])
print(my_list)  // [1, 2, 3, 4, 5, 6]
```

# list.reverse

The `list.reverse` method reverses the order of elements in a list.

#### Syntax

```tea
list.reverse()
```

#### Parameters

None.

#### Return Value

None.

#### Example

```tea
var my_list = [1, 2, 3, 4]
my_list.reverse()
print(my_list)  // [4, 3, 2, 1]
```

# list.contains

The `list.contains` checks whether an element is present in a list or not.

#### Syntax

```tea
list.contains(element)
```

#### Parameters

-   `element`: The element to search for in the list.

#### Return Value

A boolean indicating whether the element is present in the list.

#### Example

```tea
var my_list = [1, 2, 3, 4]
print(my_list.contains(2))  // true
print(my_list.contains(5))  // false
```

# list.count

The `list.contains` checks whether an element is present in a list or not.

#### Syntax

```tea
list.count(element)
```

#### Parameters

-   `element`: The element to count in the list.

#### Return Value

The number of times the element appears in the list.

#### Example

```tea
var my_list = [1, 2, 2, 3, 3, 3, 4]
print(my_list.count(2))  // 2
print(my_list.count(3))  // 3
print(my_list.count(4))  // 1
print(my_list.count(5))  // 0
```

# list.swap

The `list.contains` checks whether an element is present in a list or not.

#### Syntax

```tea
list.swap(index1, index2)
```

#### Parameters

-   `index1`: The index of the first element to swap.
-   `index2`: The index of the second element to swap.

#### Return Value

None.

#### Example

```tea
var my_list = [1, 2, 3, 4]
my_list.swap(0, 3)
print(my_list)  // [4, 2, 3, 1]
```

# list.fill

The `list.fill` method fills a list with a given value.

#### Syntax

```tea
list.fill(value)
```

#### Parameters

-   `value`: The value to fill the list with.

#### Return Value

None.

#### Example

```tea
var my_list = [1, 2, 3]
my_list.fill(0)
print(my_list)  // [0, 0, 0]
```

# list.sort

The `list.sort` method sorts the elements of a list in ascending order.

#### Syntax

```tea
list.sort()
```

#### Parameters

None.

#### Return Value

None.

#### Example

```tea
var my_list = [3, 2, 1]
my_list.sort()
print(my_list)  // [1, 2, 3]
```

# list.index

The `list.index` method returns the index of the first occurrence of a given value in a list. If the value is not found in the list, an error is raised.

#### Syntax

```tea
list.index(value)
```

#### Parameters

-   `value`: The value to search for in the list.

#### Return Value

The index of the first occurrence of `value` in the list.

#### Example

```tea
var my_list = [1, 2, 3, 2]
index = my_list.index(2)
print(index)  // 1
```

# list.join

The `list.join` method returns a string obtained by concatenating the elements of a list, using a separator string.

#### Syntax

```tea
list.join(separator)
```

#### Parameters

-   `separator`: The string to use as a separator between the elements of the list.

#### Return Value

A string obtained by concatenating the elements of the list, using `separator` as a separator.

#### Example

```tea
var my_list = ['a', 'b', 'c']
result = my_list.join(', ')
print(result)  // 'a, b, c'
```

# list.copy

The `list.copy` method returns a shallow copy of a list. A shallow copy is a new list that contains references to the elements of the original list, rather than copies of the elements themselves.

#### Syntax

```tea
list.copy()
```

#### Parameters

None.

#### Return Value

A shallow copy of the list.

#### Example

```tea
var my_list = [1, 2, 3]
copy = my_list.copy()
print(copy)  // [1, 2, 3]
```