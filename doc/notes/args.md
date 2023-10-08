# Function arguments

1.  positional
2.  key
3.  variadic

# Keyword arguments

Python has a great way of handling keyword arguments, which is both useful and versatile.

```python
def f(a, b=1, c=2):
    print(f"{a}, {b}, {c}")

f(1)    # 1, 1, 2
f(1, 1) # 1, 1, 2
f(1, 2, 3)  # 1, 2, 3

f(1, c=2, b=3)  # 1, 3, 2
f(a=12, b=1, c=4)   # 12, 1, 4
```

```tea
function f(a, b=1, c=2)
{
    console.log(a, b, c)
}

f(1)    // 1, 1, 2
f(1, 1) // 1, 1, 2
f(1, 2, 3)  // 1, 2, 3

// totally ignores names and order
f(1, c=2, b=3)  // 1, 2, 3
f(a=12, b=1, c=4)   // 12, 1, 4
```

How could this be implemented in Teascript, in such a way to also allow keyed arguments in a caller? Well, as I found out, it's pretty difficult. Python does some kind of magic trickery, because it not only works with key arguments, but also with positional ones! Surely, the implementation details are pretty complicated and quite slow overall. Python can permit this, but Teascript no. We need to be at least in the range of speed that is in between Lua and Python, a pretty good compromise for the moment.

We want at least these two methods of using keyword arguments:
- you can omit the key values
- you can refer to the key values with their own names, in any order

```tea
function f(a, b=1, c=2)
{
    print("{a}, {b}, {c}")
}

f(1)    // 1, 1, 2
f(1, 1) // 1, 1, 2
f(1, 2, 3)  // 1, 2, 3

f(1, b=2, c=3)  // 1, 2, 3
```

The simpler way to handle this would be to create a dictionary inside the `<function>` object, which will hold name and values of each key. As for the function call, a new `OP_CALL` for handling keys, `OP_CALL_ARGS`. This way, we can be sure, adding this feature won't slow down simpler functions, which don't use or benefit from this addition.

This is really important, because, as in the Crafting Interpreters book is written:
> If functions are slow, everything will be slow!

# Variadic argument

You know, variadic argument, it allows any number of arguments to be inputted in the function. It's present in Lua and Javascript (not sure about Python, at least for the syntax), though a bit differently.
It's usually denotated by 3 dots `...`
It's always set as the last set of arguments in a function call

```lua
function f(...)
    print(...)
end

f(1, 2, 3)  -- 1 2 3
```

```tea
function f(...v)
{
    console.log(v)
}

f(1, 2, 3)  // Array [1, 2, 3]
```

In Javascript, the dots are associated to a function argument name, that under the hood creates an array with the number of values given, so it would be as you were doing:

```tea
function f(v)
{
    console.log(v)
}

f([1, 2, 3])  // Array [1, 2, 3]
```

We'll do this similar way, mostly because handling a special new type of variable name would later become messier to parse and since it will always be used as an array or sort of (looking at Lua), why not doing this directly?

In Teascript, function arguments are in reality local variables which get assigned when the function is called, so in reality we don't really know the name of the arguments, as opposite to perhaps how Python handles it (really slow).