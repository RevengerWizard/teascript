More advanced options for the REPL should be added

- Output expressions

When writing simple expressions, they should be directly printed in the REPL

```
> 1
1
> type(2)
number
```

- Multi-line strings

Right now we support multi-line strings similarly to Python; double or single quotes to delimit the string, supporting new line chars and the others.

```
var s =
"""
My name is Walter White.
This is my confession.
"""
```

```
> var s = """
... This is a
... multi
... line
... string"""
> s
This is a
multi
line
string
```

- Functions

Another small feature to be added, pretty useful when you want to write long functions that span for multiple lines

```
> function a()
... return 2
>
```

If a function declaration or expression doesn't have left brace, we just accept another line

```
> function a()
... {
...     // it keeps going
...     return 2
... }
>
```

```
> function a() {
...     // it keeps going
...     return 2
... }
>
```

This method could be generalized to accept multi-lines if there's a left brace on the line (on block start). This way, also map declarations would be easier to write

```
> var m = {
... a = 1,
... b = 2,
... c = 3
... }
>
```

- Paste external code which contains new lines inside the REPL without directly executing the content

- New line if there's an '=' inside the input?