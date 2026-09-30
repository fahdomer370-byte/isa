# ISA

ISA is a small experimental programming language with a source format (`.isa`), a compiled bytecode format (`.isac`), a source runtime (`isa`), and a compiler (`isac`).

## Commands

```sh
isa main.isa
isac main.isa
isa main.isac
```

`isa main.isa` executes the original source directly. It does not create a `.isac` file.

`isac main.isa` creates `main.isac` next to the source and leaves `main.isa` unchanged.

`isa main.isac` executes ISA bytecode.

## Current 0.1 language/runtime

Implemented:

- `txt("...");`
- variables with `!=`
- `sys.input("...")`
- `print.file("path");`
- `W != 200.width;` / `H != 100.height;`
- `while.true { ... }` with `end!`
- basic `if.name == "value" { ... };`
- multiline comments `<~~ ... $~~>`
- Android-specific statements are accepted but reported as host-specific when running in Termux CLI

The language is intentionally small in this first public package. GUI widgets, Android overlays, URL imports, and a larger parser/runtime can be added without changing the command model.

## Bytecode

`.isac` uses a custom binary header beginning with:

```text
00 69 73 61 00 00 02
```

Instruction records are binary-framed and can contain ISA strings. This is encoding/bytecode, not encryption.

## License

MIT. See `LICENSE`.
