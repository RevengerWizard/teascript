The `string` type in Teascript represents a sequence of characters, and is used to represent text in your Teascript programs. You can create a string in Teascript by enclosing a sequence of characters in quotation marks, either single or double, or by using the `string` class. You can use various methods to manipulate and work with strings, such as string searching and manipulation.

#### Properties

-   `string.len`: the length of a string, in characters.

#### Example

```tea
var my_string = "Hello, world!" 
print(my_string.len)  // 13
```

# string.upper

The `string.upper` method returns a new string with all the characters in the original string converted to uppercase.

#### Syntax

```tea
string.upper()
```

#### Parameters

None.

#### Return Value

A new string with all the characters in the original string converted to uppercase.

#### Example

```tea
var my_string = "Hello, world!"
my_string = my_string.upper()
print(my_string)  // "HELLO, WORLD!"
```

# string.lower

The `string.lower` method returns a new string with all the characters in the original string converted to lowercase.

#### Syntax

```tea
string.lower()
```

#### Parameters

None.

#### Return Value

A new string with all the characters in the original string converted to lowercase.

#### Example

```tea
var my_string = "Hello, world!" 
my_string = my_string.lower() 
print(my_string)  // "hello, world!"
```

# string.reverse

The `string.reverse` method returns a new string with the characters in the original string reversed in order.

#### Syntax

```tea
string.reverse()
```

#### Parameters

None.

#### Return Value

A new string with the characters in the original string reversed in order.

#### Example

```tea
var my_string = "Hello, world!"
my_string = my_string.reverse()
print(my_string)  // "!dlrow ,olleH"
```

# string.title

The `string.title` method returns a new string with the first character of each word in the original string converted to uppercase, and the rest of the characters converted to lowercase.

#### Syntax

```tea
string.title()
```

#### Parameters

None.

#### Return Value

A new string with the first character of each word in the original string converted to uppercase, and the rest of the characters converted to lowercase.

#### Example

```tea
var my_string = "hello, world!" 
my_string = my_string.title() 
print(my_string)  // "Hello, World!"
```

# string.split

The `string.split` method returns a list of strings created by splitting the original string at each occurrence of a specified separator.

#### Syntax

```tea
string.split(separator, maxsplit)
```

#### Parameters

-   `separator`: A string that specifies the character(s) to use as the separator. If `separator` is not specified, the string is split on any white space.
-   `maxsplit`: An number that specifies the maximum number of splits to perform. If `maxsplit` is not specified, the string is split into as many substrings as possible.

#### Return Value

A list of strings created by splitting the original string at each occurrence of the specified `separator`.

#### Example

```tea
var my_string = "Hello, world! How are you today?"
my_list = my_string.split() 
print(my_list)  // ["Hello,", "world!", "How", "are", "you", "today?"]
```

In the example above, the `string.split` method is used to split the `my_string` string into a list of substrings, using white space as the separator.

# string.contains

The `string.contains` method checks whether the specified string is found within the original string

#### Syntax

```tea
string.contains(substring)
```

#### Parameters

-   `substring`: The string to search for within the original string.

#### Return Value

`true` if `substring` is found within the original string, and `false` otherwise.

#### Example

```tea
var my_string = "Hello, world!" 
var result = my_string.contains("world") 
print(result)  // true

result = my_string.contains("foo") 
print(result)  // false
```

# string.startswith

The `string.startswith` method checks whether the original string starts with the specified string.

#### Syntax

```tea
string.startswith(prefix)
```

#### Parameters

-   `prefix`: The string to search for at the start of the original string.

#### Return Value

`true` if the original string starts with `prefix`, and `false` otherwise.

#### Example

```tea
var my_string = "Hello, world!"
var result = my_string.startswith("Hello")
print(result)  // true 

result = my_string.startswith("foo") 
print(result)  // false
```

# string.endswith

The `string.endswith` method returns `true` if the original string ends with the specified string, and `false` otherwise.

#### Syntax

```tea
string.endswith(suffix)
```

#### Parameters

-   `suffix`: The string to search for at the end of the original string.

#### Return Value

`true` if the original string ends with `suffix`, and `false` otherwise.

#### Example

```tea
var my_string = "Hello, world!"
var result = my_string.endswith("world!")
print(result)  // true

var result = my_string.endswith("foo")
print(result)  // false
```

# string.leftstrip

The `string.leftstrip` strips the string of any leading white space.

#### Syntax

```tea
string.leftstrip()
```

#### Parameters

None

#### Return Value

A copy of the original string with leading white space removed.

#### Example

```tea
var my_string = "   Hello, world!"
my_string = my_string.leftstrip()
print(new_string)  // "Hello, world!"
```

# string.rightstrip

The `string.rightstrip`  strips the string of any trailin white spaces.

#### Syntax

```tea
string.rightstrip()
```

#### Parameters

None

#### Return Value

A copy of the original string with trailing white space removed.

#### Example

```tea
var my_string = "Hello, world!   "
my_string = my_string.rightstrip()
print(new_string)  // "Hello, world!"
```

# string.strip

The `string.strip` strips the string of any leading and trailing white spaces.

#### Syntax

`string.strip()`

#### Parameters

None

#### Return Value

A copy of the original string with leading and trailing white space removed.

#### Example

```tea
var my_string = "   Hello, world!   "
my_string = my_string.strip()
print(new_string)  // "Hello, world!"
```

# string.count

The `string.count` method counts the number of occurrences of a specified string within the original string.

#### Syntax

```tea
string.count(substring)
```

#### Parameters

-   `substring`: The string to search for within the original string.

#### Return Value

The number of occurrences of `substring` within the original string.

#### Example

```tea
var my_string = "Hello, world! Hello, world!" 
var result = my_string.count("Hello")
print(result)  // 2
result = my_string.count("foo") 
print(result)  // 0
```

# string.find

The `string.find` finds the index of the first occurrence of a specified string within the original string, or -1 if the string is not found.

#### Syntax

`string.find(substring)`

### Parameters

-   `substring`: The string to search for within the original string.

#### Return Value

The index of the first occurrence of `substring` within the original string, or -1 if the string is not found.

#### Example

```tea
var my_string = "Hello, world!" 
var result = my_string.find("world") 
print(result)  // 7  
result = my_string.find("foo") 
print(result)  // -1
```

# string.replace

The `string.replace` method replaces all the occurences of a string from the original string.

#### Syntax

```tea
string.replace(old_string, new_string)
```

#### Parameters

-   `old_string`: The string to search for within the original string.
-   `new_string`: The string to replace `old_string` with in the new string.

#### Return Value

A copy of the original string with all occurrences of `old_string` replaced with `new_string`.

#### Example

```tea
var my_string = "Hello, world!"
my_string = my_string.replace("world", "foo")
print(new_string)  // "Hello, foo!"
```