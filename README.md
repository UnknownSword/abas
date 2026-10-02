# abas

**this language created for my friend abas**

> a little stack language: a program is a list of commands, abas runs them from
> the top down and stops when the commands run out

```abas
mov greeting, "hello "
mov who, "world"

print greeting
print who
print "\n"
```

```
hello world
```

abas is small on purpose. Every value is one 64 bit word, every name is a word,
and there is one stack that every command shares. There are no objects, no
blocks, no heap and no garbage collector: what a program makes, it pushes, and
what it does not want any more, it pops or clears away.

---

## Contents

- [Build](#build)
- [Run](#run)
- [Command line](#command-line)
- [A tour of the language](#a-tour-of-the-language)
  - [Values](#values)
  - [Names](#names)
  - [Text and characters](#text-and-characters)
  - [Arithmetic](#arithmetic)
  - [The stack](#the-stack)
  - [Jumps, flags and loops](#jumps-flags-and-loops)
  - [Functions](#functions)
  - [Reading and writing](#reading-and-writing)
  - [Throwing things away](#throwing-things-away)
- [Command reference](#command-reference)
- [The standard library](#the-standard-library)
- [Tutorials](#tutorials)
- [Tests](#tests)
- [Project layout](#project-layout)

---

## Build

abas needs a C++ compiler, `make`, and nothing else. There are no third party
libraries.

```sh
make build
```

The binary lands in `bin/main`. The build turns on `-fsanitize=address,undefined`
and `-fwrapv` on every run, so a program that goes out of bounds or divides by
zero says so while you watch, and a number that does not fit in a word comes
back round rather than being undefined:

```sh
make run     # build, then run test/cases/01_hello.abas
make test    # build, then run every case
make clean   # take build/ and bin/ away again
```

## Run

```sh
./bin/main codes/seed.abas              # run a file
./bin/main -I . totor/5_stack.abas      # run one, with "." as the include path
./bin/main -I . a.abas b.abas           # run several, one after the other
```

Files that end in `.abas` are abas programs. `lib/` holds the small library that
the tutorials and the test cases include, so pass `-I .` when you use it.

## Command line

```
usage: ./bin/main [options] <file.abas> [file2.abas ...]

  -t     only print the tokens of every file
  -p     only print the commands of every file
  -c     only check every file, do not run it
  -s N   stop a program after N commands, no limit without it
  -d N   stop a program that calls itself N deep, no limit without it
  -I D   look for included files in D, may be given more than once
  -v     print how many commands ran
  -l     list every command abas knows
  -h     this text
```

`-s` and `-d` are the two that matter when a program will not stop: they bound
how long it may run and how deep it may call itself, and the program is handed
back to the shell with a message saying which limit it reached. Both have a long
form as well, `--step-limit N` and `--function-depth N`.

`-c` checks a program without running it, which is worth knowing because abas
finds every unmatched `func`, every unclosed `loop` and every `break` outside a
loop before a single command runs.

## A tour of the language

### Values

A value is a whole number, or text. A number is kept as a signed 64 bit word, so
it goes up to `9223372036854775807` and down to `-9223372036854775808`.

```abas
mov ten, 42            # base ten
mov bits, 0b1011       # base two
mov eight, 0o17        # base eight
mov sixteen, 0xff      # base sixteen, the letters are not case sensitive
mov minus, -0x10       # a minus sign works in front of all of them
```

A number that comes out too big for a word does not stop the program: it goes
round, the same way it does in C, so `2 to the 63rd` is `-2^63`.

### Names

A name holds a value. Nothing has to exist before it is used, the first command
that writes to a name makes it.

```abas
var a, b, c        # a, b and c all hold 0
mov a, 5           # a holds 5
mov b, "text"      # b holds text
mov c, a           # c holds whatever a holds
```

`var` is optional and takes names only, nothing else. `var a, 5` is not a short
`mov`, and abas says so rather than making one of the two meanings up. A name
that is read before anything has written to it is an error, so a `var` that comes
too late is not a `var` at all.

### Text and characters

Text is written between double quotes, and a character is written between single
quotes, where it is a number like any other: `'A'` is 65, `'0'` is 48 and `' '`
is 32.

```abas
print "a tab ->\t|end\n"
mov c, 'A'          # c holds 65
print c, "\n"       # 65
```

The escapes are `\n \t \v \r \a \b \f \0 \\ \' \"`. A backslash in front of
anything else is that character itself, so `"\q"` is a backslash and a `q`.
A character cannot hold `\n`, a line end is a line end: the code of a newline is
`10`, and a program compares against `10`.

`print` writes a whole number in base ten and `put` writes one character, which
is what turns a number into text.

### Arithmetic

Every arithmetic command writes its answer back into the first place it is given,
and a place may be a name or a slot of the stack.

```abas
add a, 3           # a = a + 3
sub a, 3           # a = a - 3
mul a, b           # a = a * b
div a, b           # a = a / b, rounds toward zero
mod a, b           # a = a % b
pow a, 10          # a = a to the 10th
lsh a, 3           # the bits moved up, zeros in
rsh a, 3           # the bits moved down, zeros in
not a              # the bits turned about
and a, b           # or a, b    xor a, b    nor a, b
inc a              # a = a + 1
dec a              # a = a - 1
```

The three that work on the bits rather than on what the number says are `lsh`,
`rsh` and `pow`. A shift is a multiply or a divide by a power of two without the
multiply, so it is the same work and none of the cost; the amount of a shift is
a number of bits between 0 and 63.

A shift works on the 64 bits of a word as they stand, sign bit and all, so `rsh`
fills with zeros and not with the sign. That is the whole difference between the
two: `-8 rsh 1` is a large positive number, while `-8 div 2` is `-4`.

### The stack

There is one stack, and it belongs to the program rather than to whatever is
running on it.

```abas
grow 4             # four words go on the stack, all of them 0
grow -4            # and they come off again
push a, b, c       # a on the bottom, c on the top
pop  x, y, z       # c into x, b into y, a into z
rpush a, b, c      # the same as push c, b, a, so a is on the top
rpop  a, b, c      # the same as pop c, b, a, so the top goes into c
```

`rpush` and `rpop` are `push` and `pop` with the names written the other way
round, and nothing else. A function that reads what it was given in the order it
wrote its names is called with one `rpush` and one `rpop` rather than with the
names turned about by hand.

A stack cannot go below empty, so a `grow` that asks for fewer words than there
are leaves it empty rather than stopping with an error. That is how a function
reserves scratch and hands it back: `gsp sp` to save the pointer, `grow n` for
room, and `ssp sp` on the way out.

The stack can also be reached by number, and `$0` is the bottom of it:

```abas
mov $0, "hello"    # the bytes go in, in order
ssp 0              # the stack is empty again
gsp n              # n holds how long the stack is
gcp p              # p holds the place of the gcp itself
```

### Jumps, flags and loops

A flag is a name for a place in the stream, and nothing more than that. Every
flag is named before the program runs, so a jump reaches a flag whichever way it
faces.

```abas
flag start
    ...
    jmp start
```

A jump goes to a flag or to a func, whichever the name points to at that place
in the program, and a flag is found before a func. `cmp` sets the result that
the jumps after it look at.

```abas
cmp a, b           # -1 if a < b, 0 if a == b, 1 if a > b
jmp name           # always
je  name           # equal
jne name           # not equal
jl  name           # less
jg  name           # greater
jle name           # less or equal
jge name           # greater or equal
```

A name may stand in front of as many places as a program likes, and the last one
in the program is the one a jump goes to. A loop is a flag and a jump written
out under a name of their own, so these four commands are that same shape:

```abas
loop top           # a jump to top lands on the head of the body
    ...
    continue       # go back to the head of this loop
    break          # go to the command under the endloop
endloop out        # a jump to out lands under the endloop, where a break goes
```

Neither `loop` nor `endloop` does anything when it runs, so the program simply
carries on under the `endloop` when it reaches it. What makes a loop go round is
a `continue`, and a `continue` with no way out of the loop in any of its turns
runs for ever. Loops may sit inside one another and inside a function, and a
`break` belongs to the loop it is written in rather than to the one the program
happens to be walking through when it gets there.

### Functions

```abas
func name          # the body is every command under this line
    ...
endfunc            # the body ends here, and gives nothing back
```

Every function has to be closed by an `endfunc`, so the body of every one of them
is known before a single command runs and a program can be checked without
running it. A function may be written inside another one.

```abas
call name          # the function runs
pop  result        # result holds what it returned
```

A `ret` gives back whatever it is written with and an `endfunc` gives nothing
back, so nothing at all is a thing to give:

```abas
ret                # gives nothing back
ret value          # pushes value for the caller
ret a, b, c        # pushes all three, in the order they are written
```

The return address of a `call` does not live on the one stack, which is what
lets a function push as much as it likes, give a value back, and leave whatever
it likes on the stack for the caller. A function that used the stack should still
give its scratch back with `ssp` or `cls` before it returns, since the stack
belongs to the program.

A function the program walks into on its own is a definition nobody asked for, so
it steps over its own body and carries on behind it. That is what lets a file of
nothing but functions, a library, sit at the top of a program and be included
there.

### Reading and writing

```abas
print "text", 42, "\n"    # every argument, in the order written
put 'A'                   # one character, the lowest byte of a value
err "something went wrong\n"
get c                     # one character back, into a name
exit 0                    # stop, with a code for the shell
```

`print` writes no newline of its own, so `print "\n"` is how a line ends and
`print` on its own writes nothing at all.

### Throwing things away

A program that wants a clean slate can have one without ending.

```abas
cls             # the stack goes back to being empty, the same as 'ssp 0'
clv             # every name goes away, and has to be made again to be read
clf             # the names of the funcs and of the flags go away
clear           # all three at once
clv a, b        # given names, only those go away
clf a, b
clear a, b      # a name given to clear goes out of all three lists
```

A name that is not there to be thrown away is not a mistake, it is already clear.
The stack of return addresses is never touched by any of them, since that is not
something the program made and a call that is running still has to come back.

## Command reference

| | |
|---|---|
| **making** | |
| `var a, b, c` | gather names in one place, each of them 0 |
| `mov a, value` | a holds value |
| `include "a.abas", "b.abas"` | paste the commands of other files in, before anything runs |
| **bits** | |
| `and a, b` | or `or`, `xor`, `nor` |
| `not a` | the bits turned about |
| `lsh a, n` | `rsh` the same shape, the amount is 0 to 63 |
| **numbers** | |
| `add a, b` | `sub`, `mul`, `div`, `mod` |
| `pow a, n` | a to the nth, a whole number of turns |
| `inc a` | `dec` |
| **the stack** | |
| `grow n` | n words on, or off when n is negative |
| `push a, b, c` | a on the bottom, c on the top |
| `pop a, b, c` | c into a, b into b, a into c |
| `rpush a, b, c` | the other way round from `push` |
| `rpop a, b, c` | the other way round from `pop` |
| `gsp n` | n holds how long the stack is |
| `ssp n` | the stack is n words long |
| `gcp p` | p holds the place of the `gcp` itself |
| `$0` | the bottom of the stack, a place like any other |
| **clearing** | |
| `cls` | the stack goes back to being empty |
| `clv names` | the names go away |
| `clf names` | the names of the funcs and flags go away |
| `clear names` | all three at once |
| **writing and reading** | |
| `print args...` | every argument, in order, numbers in base ten |
| `put value` | one character |
| `err args...` | the same as print, to the error output |
| `get name` | one character in |
| `exit code` | stop, with a code for the shell |
| **jumping** | |
| `flag name` | name this place |
| `cmp a, b` | -1, 0 or 1 |
| `jmp name` | always |
| `je name` | `jne`, `jl`, `jg`, `jle`, `jge` |
| **blocks** | |
| `loop name` | the head of a body |
| `continue` | back to the head of this loop |
| `break` | under the `endloop` |
| `endloop name` | the tail of a body |
| **functions** | |
| `func name` | the body is every command under it |
| `endfunc` | the body ends here, gives nothing back |
| `call name` | run a function, come back after it |
| `ret values...` | give values back to the caller |

## The standard library

`lib/` is written in abas, so it is also the way the language is meant to be
read.

| | |
|---|---|
| `lib/putchar.abas` | `mov pc, 'A'` then `call putchar` |
| `lib/printnum.abas` | prints the number in `pn`, digit by digit, the hard way |
| `lib/printstr.abas` | prints `ps_len` bytes of the stack starting at `ps_at` |
| `lib/strlen.abas` | counts the words from `sl_at` up to the first one that is 0, and returns the count |
| `lib/readline.abas` | reads a line, and returns its length |

## Tutorials

`totor/` holds sixteen lessons, named in the order to read them, and every one of
them is a whole program that runs.

```sh
for f in totor/*.abas; do ./bin/main -I . "$f"; done
./totor/run.sh                              # run them all and report what failed
echo hello | ./bin/main -I . totor/9_io.abas   # two of them read stdin
```

```
0_welcome      1_comment      2_vars          3_numbers
4_arithmetic   5_stack        6_flow          7_functions
8_text         9_io           10_text_words   11_include
12_stdlib      13_programs    14_errors       15_recursion
```

`codes/` holds programs that were written to get something working: `loop.abas`,
`segment.abas`, `seed.abas` and the rest.

## Tests

`test/cases/` holds fourteen cases, each one a program, the output it is expected
to print in a `.out` file beside it, and optionally the options it needs in a
`.args` file and the code it should stop with in a `.exit` file.

```sh
make test                              # run every case
./test/run.sh ./bin/main 04_flow       # only the cases whose name holds 04_flow
./test/run.sh ./bin/main --bless       # write the .out files from this run
```

A run that fails leaves what it printed and a diff in `test/failures/`.

## Project layout

```
main.cpp            the command line: options, and running a file
include/core/       lexer.h, parser.h, interp.h
src/core/           lexer.cpp, parser.cpp, interp.cpp
src/utils/          file.cpp, opening a file
lib/                the standard library, written in abas
codes/              programs written to get something working
totor/              the sixteen lessons
test/run.sh         the test runner
test/cases/         the cases and what they are expected to print
```