The `io` module provides input/output functionality, including the ability to read from and write to standard input/output/error streams.

# io.stdin

The `io.stdin` file represents the standard input stream in Teascript. It can be used to read input from the user.

#### Syntax

```tea
io.stdin
```

#### Example

```tea
import io

// Read a line of input from the user
var input_str = io.stdin.readline()
print(input_str)  // [input entered by user]
```

# io.stdout

The `io.stdout` file represents the standard output stream in Teascript. It can be used to print output to the console.

#### Syntax

```tea
io.stdout
```

#### Examples

```tea
import io

// Print a string to the console
io.stdout.write("Hello, world!")  // Hello, world!
```

# io.stderr

The `io.stderr` file represents the standard error stream in Teascript. It can be used to print error messages to the console.

#### Syntax

```tea
io.stderr
```

#### Example

```tea
import io

// Print an error message to the console
io.stderr.write("An error occurred!")  // An error occurred!
```