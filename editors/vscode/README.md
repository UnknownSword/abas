# Abas for Visual Studio Code

Syntax highlighting and basic language configuration for the
[Abas](https://github.com/UnknownSword/abas) programming language.

The extension is intentionally small. It ships a TextMate grammar and a
language configuration and nothing else: no language server, no formatter, no
debugger. It needs no custom theme, so the colours come from whatever theme is
active.

## What it provides

- Language id `abas`, registered for files ending in `.abas`.
- A TextMate grammar (`syntaxes/abas.tmLanguage.json`, scope `source.abas`) that
  follows the Abas lexer in `src/core/lexer.cpp` and the command table in
  `src/core/interp.cpp`:
  - line comments (`# ...`) and block comments (`## ... ##`),
  - double quoted strings with escapes, single quoted character literals,
  - whole number literals in base ten, `0x` hex, `0b` binary and `0o` octal,
  - the 52 commands of the interpreter as keywords,
  - `$` stack pointers (`$name`, `$3`, `$$$name`),
  - operators, separators and brackets,
  - `TODO`, `FIXME`, `XXX` and `NOTE` inside comments.
- A language configuration (`language-configuration.json`) with the comment
  characters, brackets, auto closing and surrounding pairs, and simple
  indentation rules for `func`/`loop` blocks.

Abas has no boolean or null literals, no floating point numbers and no type
keywords, so the grammar does not pretend there are any.

## Install

### From this repository with the install script

From the root of the Abas checkout:

```sh
./scripts/install-editors.sh --vscode
```

The script builds and installs the extension when a VS Code command line tool
(`code`, `code-insiders` or `codium`) is found, and copies the extension into
the editor's extensions directory otherwise. Removal is symmetric:

```sh
./scripts/uninstall-editors.sh --vscode
```

### Manually, by packaging a VSIX

The extension as it is in this directory is a complete, packageable extension.
From this directory:

```sh
npx @vscode/vsce package
code --install-extension abas-0.1.0.vsix
```

(`vsce` needs to be available; `npm install -g @vscode/vsce` installs it.)

### Manually, without any tooling

Copy this directory into the extensions directory of the editor, using the
`publisher.name-version` folder name the editor expects:

```sh
cp -R . ~/.vscode/extensions/unknownsword.abas-0.1.0
```

Use `~/.vscode-insiders/extensions` for VS Code Insiders and
`~/.vscode-oss/extensions` for VSCodium. Restart or reload the editor window
afterwards.

## Test

1. Open any `.abas` file from `codes/`, `test/cases/` or `totor/`.
2. Check that commands are coloured as keywords, strings and comments are
   coloured, numbers stand out, and `$` pointers are visible.
3. In the command palette, run **Developer: Inspect Editor Tokens and Scopes**
   and confirm the scope names (`keyword.control.abas`, `string.quoted.double.abas`,
   `comment.line.number-sign.abas`, and so on).

To exercise the grammar during development without installing it, open this
directory as a folder in VS Code and press `F5`; a second editor window starts
with the extension loaded.

## Package

To produce a distributable `.vsix`:

```sh
npx @vscode/vsce package
```

To see the file list that would be packaged:

```sh
npx @vscode/vsce ls
```

## Layout

```
editors/vscode/
├── package.json                  extension manifest (contributes the language)
├── language-configuration.json   comments, brackets, indentation
├── syntaxes/
│   └── abas.tmLanguage.json      TextMate grammar
├── .vscodeignore                 files left out of the packaged .vsix
└── README.md
```
