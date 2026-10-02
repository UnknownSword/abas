#pragma once

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <core/parser.h>

typedef enum{
	V_INT = 0, // a number, or a char code
	V_STR,     // text
} VALUEKIND;

// every abas value is a 64 bit word, text lives next to it
struct abas_value_t{
	VALUEKIND kind = V_INT;
	value_t num = 0;
	string str = "";
};

// var writes down the names a piece of work is going to need and gives every one
// of them the value 0, as many names as are wanted on the one line
//
//   var a, b, c      a, b and c all hold 0
//
// a name does not have to exist before it is used: the first command that writes
// to a name makes it, so nothing needs a var and a program without one runs the
// same. what a var is for is the names of a piece of work gathered in one place
// at the top of it, each of them there to be worked on before it holds anything
// worth working with
//
// every argument of a var is a name and nothing else. a var takes no value, since
// a name holding something other than 0 is a mov that says 0 out loud, so
// 'var a, 5' is not a shorter mov: the 5 is not a name, and abas says so rather
// than making one of the two meanings up
//
//   var a            a holds 0
//   mov a, 5         a holds 5
//   mov a, "text"    a holds text
//   mov a, b         a holds whatever b holds
//
// a name that is read before anything has written to it is an error, so a var
// that comes too late is not a var at all. a name read after a clv is a name
// that has to be made again

// a func is a block of commands and a name of its own:
//
//   func name          the body of the func is every command under this line
//   ...
//   endfunc            the body ends here, and the func gives nothing back
//
// every func has to be closed by an endfunc, so the body of every func is known
// before a single command runs, and a program can be checked without running
// it. a func may be written inside another func, and a stack of the funcs that
// are open says which endfunc closes which func. the commands that stand outside
// every func are the main code, and those run from the top down
//
// a call enters a func and saves the place it was made, and a ret or an endfunc
// comes back to it. a ret gives back whatever it is written with, an endfunc
// gives nothing back, and nothing at all is a thing to give:
//   ret                 gives nothing back
//   ret value           pushes value for the caller
//   ret a, b, c         pushes all three, in the order they are written
// the values a func gives back go onto the one stack, so a caller takes them
// off with pop, one word for each value the func returned.
//
//   call name           the func runs, its return address goes on a hidden
//                       stack of its own that no abas command can see
//   pop result          result holds what it returned
//
// a call as the last line of a program is nothing special: the place it comes
// back to is the end of the stream, and the program is over when the func gets
// there
//
// the return address of a call does not live on the one stack. that is what
// lets a function push as much as it likes, give a value back, and leave
// whatever it likes on the one stack for the caller. a function that used the
// one stack should still give its scratch back with ssp, or cls, before it
// returns, because the stack belongs to the program rather than to the function
// that is running.
//
// a func the program walks into on its own is a definition nobody asked for, so
// it steps over its own body and carries on behind it. that is what lets a file
// of nothing but funcs, a library, sit at the top of a program and be included
// there.
//
// a flag is a name for a place in the stream, and nothing more than that:
//
//   flag name           the place the program is at is saved under that name
//
// every flag in the stream is named before the program runs, so a jump reaches
// a flag whichever way it faces, and neither clv nor clf nor clear has anything
// to do with them. only the stream itself says what the names are
//
// a jump goes to a flag or to a func, whichever the name points to at that place
// in the program (a flag is found before a func, so a name that is both goes to
// the flag). a call does the same, and it saves the place it was made to come
// back to, so a call may be made on the name of a flag as well as on the name of
// a func. the difference is only whether the body comes back with a ret or an
// endfunc, or leaves by a jump of its own
//
// a name may stand in front of as many
// flags as a program likes, the last one in the program being the one a jump
// goes to. that is what a loop wants, since every block it walks through can
// have a name of its own:
//
//   flag loop           the jump below walks back here, round and round
//   ...
//   jmp loop
//
// and a block of a program hands control to another block with a jump and
// carries on behind it, which is the same thing a call does without the coming
// back. a jump to a func runs its body from the first line, and the body has to
// leave by a jump of its own rather than by a ret or an endfunc, since neither of
// those has a call to come back from
//
// a loop is a flag and a jump written out under a name of their own, so the four
// commands below are the same shape with the names filled in:
//   loop             the head of the body, a continue lands back on it
//     ...
//   continue         go back to the loop this one is in
//   break            go to the command under the endloop
//   endloop          the tail of the body, the two lines above close no block
//
// neither loop nor endloop does anything when it runs, so the body is simply the
// commands between the two and the program carries on under the endloop when it
// reaches it. what makes a loop go round is a continue in the body, and a
// continue with no way out of the loop in any of its turns runs for ever. that is
// the whole of a loop, and 06_flow writes the same thing longhand with a flag for
// the head of the body, a jump back to it and a jump out of it.
//
// every loop has to be closed by an endloop before the program runs, and a break
// or a continue with no loop around it is an error, also before the program runs.
// loops may sit inside one another and inside a func, and a break belongs to the
// loop it is written in rather than to the one the program happens to be walking
// when it gets there
//
// either end of a loop may carry a name, and a name there is a flag as well, so
// a jump reaches a loop from anywhere in the program under the rule that governs
// every other name: the last one written for a place is the one a jump goes to
//   loop top         a jump to top lands on the head of the body
//   endloop out      a jump to out lands under the endloop, where a break goes
//
// the one stack grows and shrinks under push and pop, which move a value each,
// and under grow, which moves no value at all and only changes how long the
// stack is:
//   grow 10         ten words go on the stack, all of them 0
//   grow -10        those ten words come off again
//   grow n          as many words as n says, and n may be negative
// a stack cannot go below empty, so a grow that asks for fewer words than
// there are leaves it empty rather than stopping with an error. grow is how a
// function reserves scratch and hands it back
//
// rpush and rpop are push and pop with the arguments written the other way
// round, and nothing else. push puts the first argument on the bottom of the
// stack and pop hands the top of the stack to the first name written, so the two
// together walk the stack from one end. rpush and rpop walk it from the other:
//   push a, b, c     a on the bottom, c on the top
//   pop  a, b, c     c goes into a, b into b and a into c
//   rpush a, b, c    the same as push c, b, a, so a is on the top
//   rpop  a, b, c    the same as pop c, b, a, so the top goes into c
// a func that reads what it was given off the stack in the order its names are
// written is called with one rpush and one rpop rather than with the names turned
// about by hand
//
// lsh, rsh and pow are the commands that work on the bits of a word rather than
// on what the number says. each of them reads the first place, reads an amount
// out of the second, and writes the answer back into the first place, so a name,
// a pointer such as $3 and a number of them are all first places
//   lsh a, 3      a times two, three times over, the bits moved up and zeros in
//   rsh a, 3      a divided by eight, the bits moved down and zeros in
//   pow a, 10     a to the tenth
//
// a shift is a multiplication or a division by a power of two without the
// multiply, which is the same work and none of the cost, and pow is the one of
// the three that a shift cannot do at all, since a shift only ever multiplies by
// two. pow squares the base and halves the exponent as it goes, so it is as many
// multiplies as the exponent has bits rather than as many as the exponent is
// worth, and ten to the thousandth is a handful of them rather than a thousand
//
// a shift works on the 64 bits of a word as they stand, sign bit and all, which
// is the same ground and, or and xor work on. rsh therefore fills with zeros and
// not with the sign, so rsh is not a way of halving a number that may be
// negative: -8 rsh 1 is a large positive number, since the low bit is gone and
// nothing came in at the top, while -8 div 2 is -4. rsh rounds down and div
// rounds toward zero, which is the whole difference between them. the amount of
// a shift is a number of bits and has to be between 0 and 63
//
// the exponent of pow is a number of whole turns, so a negative one is an error
// rather than a fraction, and anything to the power of zero is one. a power too
// large for a word is not an error, and goes round the same way mul does: the
// answer is the true one with as many 2^64s taken off the top as it takes to make
// it fit, so 2 to the 63rd is -2^63, which is the one size a word reaches and
// reaches as a negative number
//
// print takes any number of arguments as well and writes each one in the order
// it is written, a number in base ten and text as it stands. it writes no
// newline of its own, so 'print "\n"' is how a line ends and 'print' on its own
// writes nothing at all. a character is a number, so 'print "\n"' writes the
// line end and 'print '\n'' writes the number 10
//
// nothing limits how deep a program may call itself unless the caller of abas
// asks for a limit with --function-depth
// a loop is found in the command stream before the program runs, and these are
// the two places a jump out of it or back to its head lands
struct loop_t{
	value_t head = 0; // the 'loop' itself, where a continue lands
	value_t tail = 0; // the command under the 'endloop', where a break lands
};

struct abas_vm_t{
	vector<value_t> stack;         // the one and only stack, index 0 is the bottom
	vector<value_t> rets;          // return addresses, hidden from every command
	map<string, abas_value_t> vars;
	map<string, value_t> funcs;  // name of a func -> the func itself
	map<string, value_t> flags;  // name of a flag -> the place it stands on
	map<value_t, value_t> over; // the func on a command -> the command behind its body
	map<value_t, loop_t> loops;   // a break or a continue -> the loop it belongs to
	vector<string> frames;          // the functions that are running right now
	int cmp_result = 0;             // -1 : a < b, 0 : a == b, 1 : a > b
	value_t pc = 0;        // the command that runs
	value_t code_size = 0; // how many commands the program has
	value_t steps = 0;
	value_t max_steps = 0;      // 0 means no limit
	value_t max_call_depth = 0; // 0 means no limit
	int exit_code = 0;
	bool running = true;
	bool failed = false;
	string error = "";
	value_t error_line = 0;
	string file = "";
};

extern abas_vm_t vm;

void vm_reset();

// argument handling, shared by the commands
bool resolve_value(const vector<TOKEN>& arg, abas_value_t& out, string& error);
bool store_value(const vector<TOKEN>& arg, const abas_value_t& value, string& error);
bool read_label_name(const command_t& cmd, size_t index, string& name, string& error);

// runs the command stream, returns the exit code of the program
int abas_run(vector<command_t>& code, const string& file);
// looks at a program without running it, 0 when the program is sound
int abas_check(vector<command_t>& code, const string& file);
// the words the one stack may hold
static const value_t STACK_MAX_WORDS = (value_t) INT64_MAX;
