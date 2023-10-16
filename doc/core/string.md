# String

The `String` type in Teascript represents a sequence of characters, and is used to represent text in your Teascript programs. You can create a string in Teascript by enclosing a sequence of characters in quotation marks, either single or double, or by using the `string` class. You can use various methods to manipulate and work with strings, such as string searching and manipulation.

#### Properties

-   `String.len`: the length of a string, in characters.

#### Example

```tea
var my_string = "Hello, world!"
print(my_string.len)  // 13
```

---

## String.upper

The `String.upper` method returns a new string with all the characters in the original string converted to uppercase.

#### Syntax

```tea
String.upper()
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

---

## String.lower

The `String.lower` method returns a new string with all the characters in the original string converted to lowercase.

#### Syntax

```tea
String.lower()
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

---

## String.reverse

The `String.reverse` method returns a new string with the characters in the original string reversed in order.

#### Syntax

```tea
String.reverse()
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

---

## String.title

The `String.title` method returns a new string with the first character of each word in the original string converted to uppercase, and the rest of the characters converted to lowercase.

#### Syntax

```tea
String.title()
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

---

## String.split

The `String.split` method returns a list of strings created by splitting the original string at each occurrence of a specified separator.

#### Syntax

```tea
String.split(separator, maxsplit)
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

---

## String.contains

The `String.contains` method checks whether the specified string is found within the original string

#### Syntax

```tea
String.contains(substring)
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

---

## String.startswith

The `String.startswith` method checks whether the original string starts with the specified string.

#### Syntax

```tea
String.startswith(prefix)
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

---

## String.endswith

The `String.endswith` method returns `true` if the original string ends with the specified string, and `false` otherwise.

#### Syntax

```tea
String.endswith(suffix)
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

---

## String.leftstrip

The `String.leftstrip` strips the string of any leading white space.

#### Syntax

```tea
String.leftstrip()
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

---

## String.rightstrip

The `String.rightstrip`  strips the string of any trailin white spaces.

#### Syntax

```tea
String.rightstrip()
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

---

## String.strip

The `String.strip` strips the string of any leading and trailing white spaces.

#### Syntax

`String.strip()`

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

---

## String.count

The `String.count` method counts the number of occurrences of a specified string within the original string.

#### Syntax

```tea
String.count(substring)
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

---

## String.find

The `String.find` finds the index of the first occurrence of a specified string within the original string, or -1 if the string is not found.

#### Syntax

`String.find(substring)`

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

---

## String.replace

The `String.replace` method replaces all the occurences of a string from the original string.

#### Syntax

```tea
String.replace(old_string, new_string)
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