# Teascript

A nice and calm dynamically typed programming language. 

It tries to mix the familiarity of Javascript, the simplicity of Python and the ease of use of Lua's C API.

## Building

Teascript doesn't have any external dependencies, you'll need just gcc, make and cmake

```bash
git clone https://github.com/RevengerWizard/teascript && cd teascript
cmake .
make
```

Once it's been compiled, you should now be able to access `tea` within your console. Let's write our simple first program:

```js
print("Hello World!")
```

Now that you've done that, run it using `tea`, followed by the name of the file ending with a `.tea` extension:

```bash
tea hello.tea
```

## License

Licenced under GNU General Public License, Version 2, June 1991. [Copy of the license can be found here](https://github.com/RevengerWizard/teascript/blob/master/LICENSE)