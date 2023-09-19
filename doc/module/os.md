The `os` module provides functions and values for interacting with the operating system.

# os.getenv

The `os.getenv` function gets the value of an environment variable.

#### Syntax

```tea
os.getenv(name)
```

#### Parameters

-   `name`: The name of the environment variable.

#### Return Value

The value of the environment variable, or `null` if the environment variable does not exist.

#### Example

```tea
import os

// Get the value of the HOME environment variable
var home_dir = os.getenv('HOME')
print(home_dir)  // [value of the HOME environment variable]

// Get the value of an undefined environment variable
var undefined_var = os.getenv('UNDEFINED_VAR')
print(undefined_var)  // null
```

# os.setenv

The `os.setenv` function sets the value of an environment variable.

#### Syntax

```tea
os.setenv(name, value)
```

#### Parameters

-   `name`: The name of the environment variable.
-   `value`: The value to set for the environment variable.

#### Return Value

None.

#### Example

```tea
import os

// Set the value of the MY_VAR environment variable
os.setenv('MY_VAR', 'value')

// Get the value of the MY_VAR environment variable
var my_var = os.getenv('MY_VAR')
print(my_var)  // 'value'
```

# os.system

The `os.system` function executes a system command.

#### Syntax

```tea
os.system(command)
```

#### Parameters

-   `command`: The system command to execute.

#### Return Value

The exit status code of the command.

#### Example

```tea
import os

// Execute a system command
var exit_code = os.system('ls')
print(exit_code)  // [exit status code of the 'ls' command]

// Execute a system command and check the exit status
exit_code = os.system('ls non-existent-dir')
if(exit_code != 0)
    print('Command failed')
```

# os.name

The `os.name` value is a string that indicates the name of the operating system. It is 'windows' for Windows, 'unix' for Unix-like systems, 'macOS' for macOS, 'linux' for Linux, 'freeBSD' for FreeBSD, or 'other' if it was not able to determine the operating system.

#### Example

```tea
import os

// Print the name of the operating system
print(os.name)

// Check if the operating system is Unix-like
if(os.name in ['unix', 'macOS', 'linux', 'freeBSD'])
    print('Operating system is Unix-like')
else
    print('Operating system is not Unix-like')

// Check if the operating system is Windows
if(os.name == 'windows')
    print('Operating system is Windows')
```

# os.env

The `os.env` value is a map that contains the environment variables of the operating system. Note: adding new keys to `os.env` does not add the new keys to the env variables. Use `os.setenv` instead.

#### Example

```tea
import os

// Print all environment variables
print(os.env)

// Get the value of the HOME environment variable
var home_dir = os.env['HOME']
print(home_dir)

// Set the value of the MY_VAR environment variable
os.setenv('MY_VAR', 'value')

// Get the value of the MY_VAR environment variable
var my_var = os.env['MY_VAR']
print(my_var)  // Output: 'value'
```
