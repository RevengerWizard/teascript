# sys

The `sys` module provides access to system-specific parameters and functions.

---

## sys.argv

The `sys.argv` value is a list of strings that contains the command line arguments passed to the program. The first element of the list is the name of the script itself, and the remaining elements are the arguments passed to the script.

#### Example

```tea
import sys

// Print the command line arguments
print(sys.argv)

// Print the first command line argument (the script name)
print(sys.argv[0])

// Print the second command line argument (if it exists)
if(sys.argv.len > 1)
    print(sys.argv[1])
```

---

## sys.version

The `sys.version` value is a map that contains information about the version of Teascript in use. It includes the following key-value pairs:

-   `major`: The major version number of Teascript.
-   `minor`: The minor version number of Teascript.
-   `patch`: The patch version number of Teascript.

#### Example

```tea
import sys

// Print the version of Teascript in use
print(sys.version)

// Print the major version number of Teascript
print(sys.version['major'])
```

---

## sys.byteorder

The `sys.byteorder` value is a string that indicates the byte order of the system. It is either 'little' or 'big'.

#### Example

```tea
import sys

// Print the byte order of the system
print(sys.byteorder)

// Check if the system has little endian byte order
if(sys.byteorder == 'little')
    print('System has little endian byte order')
else
    print('System has big endian byte order')
```

---

## sys.exit

The `sys.exit` function exits the program with an optional exit status code.

#### Syntax

```tea
sys.exit(status = 0)
```

#### Parameters

-   `status`: The exit status code. The default value is 0.

#### Return Value

This function does not return a value.

#### Example

```tea
import sys

// Exit the program with a successful status code
sys.exit(0)

// Exit the program with a failure status code
sys.exit(1)
```