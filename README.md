# Teascript

Teascript is a simple, multi-paradigm scripting language for embedding and standalone use

You can try it now [inside your browser](https://revengerwizard.github.io/wasm-tea)

## Building

Teascript doesn't have any external dependencies, you'll need just gcc and make

```bash
git clone https://github.com/RevengerWizard/teascript && cd teascript
make
```

Once it's been compiled, you should now be able to access the program `tea` from within your terminal console. Let's write our first simple program:

```tea
print("Hello World!")
```

Now that you've done that, run it using `tea`, followed by the name of the file ending with the `.tea` extension:

```bash
tea hello.tea
```

## License

Licenced under MIT License. [Copy of the license can be found here](https://github.com/RevengerWizard/teascript/blob/master/LICENSE)