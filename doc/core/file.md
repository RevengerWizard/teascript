# File

The `File` type in Teascript represents an object that provides a stream of bytes to a file on the computer's file system. It allows you to read from and write to files, as well as perform various other operations such as seeking to a specific position within the file or checking its current position.

#### Properties

-   `File.closed`: a boolean indicating whether the file is closed or not.
-   `File.path`: a string representing the path to the file on the file system.
-   `File.type`: a string indicating how the file was open.

#### Example

```tea
var f = open("example.txt", "r")
print(f.path)  // "example.txt"
print(f.type)   // "r"
f.close()
print(f.closed)  // true
```

---

## File.write

The `File.write` method writes the given string to the file.

#### Syntax

```tea
File.write(data)
```

#### Parameters

-   `data`: the data to be written to the file.

#### Return Value

The number of bytes written.

#### Example

```tea
var f = open("example.txt", "w")
var num_bytes = f.write("Hello, world!")
print(num_bytes)  // 13
f.close()
```

---

## File.writeline

The `File.writeline` method writes the given string to the file, followed by a newline character.

#### Syntax

```tea
File.writeline(data)
```

#### Parameters

-   `data`: the data to be written to the file.

#### Return Value

The number of bytes written.

#### Example

```tea
var f = open("example.txt", "w")
var num_bytes = f.write("Hello, world!")
print(num_bytes)  // 14
f.close()
```

---

## File.read

The `File.read` method reads and returns the specified number of bytes from the file. If `size` is not specified, it reads and returns all the remaining bytes.

#### Syntax

```tea
File.read(size)
```

#### Parameters

-   `size`: the number of bytes to be read from the file.

#### Return Value

A string containing the read bytes.

#### Example

```tea
var f = open("example.txt", "r")
var content = f.read()
print(content)  // the entire file
f.close()

f = open("example.txt", "r")
var first_line = f.read(5)
print(first_line)  // the first 5 bytes of the file
f.close()
```

---

## File.readline

The `File.readline` method reads and returns a single line from the file.

#### Syntax

```tea
File.readline()
```

#### Parameters

None.

#### Return Value

A string containing the current line.

#### Example

```tea
var f = open("example.txt", "r")
var first_line = f.readline()
print(first_line)  // the first line of the file
f.close()
```

---

## File.seek

The `File.seek` method

#### Syntax

```tea
File.seek()
```

#### Parameters

None.

#### Return Value

None.

#### Example

```tea
var f = open("example.txt", "r")
var first_line = f.readline()
print(first_line)  // the first line of the file
f.close()
```

---

## File.close

The `File.close` method closes a current opened file. If the file is already closed, it gives and error.

#### Syntax

```tea
File.close()
```

#### Parameters

None.

#### Return Value

None.

#### Example

```tea
var f = open("example.txt")
f.close()
f.close()   // Error
```