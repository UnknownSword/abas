#include <algorithm>
#include <cstdio>
#include <iostream>
#include <core/interp.h>

using namespace std;

abas_vm_t vm;

void vm_reset(){
	vm.stack.clear();
	vm.rets.clear();
	vm.vars.clear();
	vm.funcs.clear();
	vm.flags.clear();
	vm.over.clear();
	vm.loops.clear();
	vm.frames.clear();
	vm.cmp_result = 0;
	vm.pc = 0;
	vm.code_size = 0;
	vm.steps = 0;
	vm.exit_code = 0;
	vm.running = true;
	vm.failed = false;
	vm.error = "";
	vm.error_line = 0;
	vm.file = "";
}

static COMMAND_STATUS fail_at(const command_t& cmd, const string& msg){
	vm.running = false;
	vm.failed = true;
	vm.file = cmd.file;
	vm.error_line = cmd.line;
	vm.error = msg;
	return RUNTIME_ERROR;
}

static COMMAND_STATUS wrong_args(const command_t& cmd, size_t expected){
	return fail_at(cmd, cmd.command + ": expected " + to_string(expected) +
	                              " argument(s), got " + to_string(cmd.args.size()));
}

// ---------------------------------------------------------------- arguments

bool resolve_value(const vector<TOKEN>& arg, abas_value_t& out, string& error){
	if(arg.empty()){
		error = "missing argument";
		return false;
	}
	size_t i = 0;
	bool negate = false;
	if(arg[0].type == OPERATOR && (arg[0].value == "-" || arg[0].value == "+")){
		if(arg.size() < 2){
			error = "operator '" + arg[0].value + "' without a value";
			return false;
		}
		negate = (arg[0].value == "-");
		i = 1;
	}
	if(arg[i].type == OPERATOR && !arg[i].value.empty() && arg[i].value[0] == '$'){
		// de reference, $$$12 is the same as $12
		if(negate){
			error = "cannot use a sign on a pointer";
			return false;
		}
		if(arg.size() != i + 2){
			error = "'" + arg[i].value + "' needs one index after it";
			return false;
		}
		vector<TOKEN> inner(arg.begin() + i + 1, arg.end());
		abas_value_t index;
		if(!resolve_value(inner, index, error))
			return false;
		if(index.kind != V_INT){
			error = "a pointer index must be a number";
			return false;
		}
		if(index.num >= (value_t)vm.stack.size()){
			error = "stack index " + to_string(index.num) + " is out of range, the stack holds " +
			        to_string(vm.stack.size()) + " word(s)";
			return false;
		}
		out.kind = V_INT;
		out.num = vm.stack[index.num];
		out.str = "";
		return true;
	}
	if(arg.size() != i + 1){
		error = "unexpected token '" + arg[i + 1].value + "' in argument";
		return false;
	}
	const TOKEN& token = arg[i];
	switch(token.type){
		case INT: {
			value_t num = 0;
			if(!parse_number(token.value, num)){
				error = "'" + token.value + "' is not a number";
				return false;
			}
			// a sign turns the number the other way up. the most negative number
			// there is comes back as itself, since the bits it has are the bits it
			// keeps
			if(negate)
				num = -num;
			out.kind = V_INT;
			out.num = num;
			out.str = "";
			return true;
		}
		case CHAR:
			if(negate){
				error = "cannot use a sign on a character";
				return false;
			}
			out.kind = V_INT;
			out.num = (value_t)(unsigned char)token.value[0];
			out.str = "";
			return true;
		case STRING:
			if(negate){
				error = "cannot use a sign on a string";
				return false;
			}
			out.kind = V_STR;
			out.num = 0;
			out.str = token.value;
			return true;
		case IDENTIFIER: {
			auto it = vm.vars.find(token.value);
			if(it == vm.vars.end()){
				error = "name '" + token.value + "' has no value yet, give it one with var " +
				        token.value + ", something or mov " + token.value + ", something";
				return false;
			}
			if(negate){
				error = "cannot use a sign on the name '" + token.value + "'";
				return false;
			}
			out = it->second;
			return true;
		}
		default:
			error = "unexpected token '" + token.value + "', a value was expected";
			return false;
	}
}

static bool is_pointer_arg(const vector<TOKEN>& arg){
	return !arg.empty() && arg[0].type == OPERATOR && !arg[0].value.empty() &&
	       arg[0].value[0] == '$';
}

// a slot on the stack takes words. a number is one word, and text is one byte per
// word, the same way push puts it there, so 'mov $3, "hi"' writes 'h' into slot 3
// and 'i' into slot 4 and leaves the stack two words longer
static bool write_pointer(const vector<TOKEN>& arg, const abas_value_t& value, string& error){
	if(arg.size() != 2){
		error = "'" + arg[0].value + "' needs one index after it";
		return false;
	}
	vector<TOKEN> inner(arg.begin() + 1, arg.end());
	abas_value_t index;
	if(!resolve_value(inner, index, error))
		return false;
	if(index.kind != V_INT){
		error = "a pointer index must be a number";
		return false;
	}
	size_t at = (size_t)index.num;
	size_t words = value.kind == V_STR ? value.str.size() : 1;
	if(words == 0)
		return true;
	if(at + words > (size_t)STACK_MAX_WORDS)
		return (error = "slot " + to_string(at) + " and the text behind it do not fit, the stack "
		                       "holds " + to_string(STACK_MAX_WORDS) + " word(s)",
		        false);
	if(at + words > vm.stack.size())
		vm.stack.resize(at + words, 0);
	if(value.kind == V_STR){
		for(size_t i = 0 ; i < value.str.size() ; i++)
			vm.stack[at + i] = (value_t)(unsigned char)value.str[i];
	}
	else
		vm.stack[at] = value.num;
	return true;
}

// writing a name that does not exist yet is how a name comes to life, a program
// may use a name before it was ever declared, 'mov x, 1' is all it takes
bool store_value(const vector<TOKEN>& arg, const abas_value_t& value, string& error){
	if(arg.empty()){
		error = "missing destination";
		return false;
	}
	if(is_pointer_arg(arg))
		return write_pointer(arg, value, error);
	if(arg.size() != 1 || arg[0].type != IDENTIFIER){
		error = "'" + arg[0].value + "' cannot be written to";
		return false;
	}
	vm.vars[arg[0].value] = value;
	return true;
}

bool read_label_name(const command_t& cmd, size_t index, string& name, string& error){
	if(index >= cmd.args.size() || cmd.args[index].empty() ||
	   cmd.args[index].size() != 1 || cmd.args[index][0].type != IDENTIFIER){
		error = cmd.command + ": expected the name of a flag or a func";
		return false;
	}
	name = cmd.args[index][0].value;
	return true;
}

// ------------------------------------------------------------------ output

static void print_value(const abas_value_t& value, ostream& out){
	// every value is a 64 bit word, print shows it the way a person reads it
	if(value.kind == V_STR)
		out << value.str;
	else
		out << value.num;
}

// ---------------------------------------------------------------- commands

// a value is a 64 bit word read as the number a person writes, so it is a signed
// number, a negative one is a negative one, and nothing here has to ask which
// side of zero a word is on before it can be compared with another
//
// the top bit of a word is the sign of it, so a number that comes out of a mul
// too large for a word has not gone missing, it has come round the same way a
// counter with 64 bits on it goes round, and 2 to the 63rd is the largest number
// there is rather than an error
typedef value_t (*binop_t)(value_t a, value_t b, bool& failed);

static value_t op_add(value_t a, value_t b, bool&){ return a + b; }
static value_t op_sub(value_t a, value_t b, bool&){ return a - b; }
static value_t op_mul(value_t a, value_t b, bool&){ return a * b; }
static value_t op_and(value_t a, value_t b, bool&){ return a & b; }
static value_t op_or(value_t a, value_t b, bool&){ return a | b; }
static value_t op_xor(value_t a, value_t b, bool&){ return a ^ b; }
static value_t op_nor(value_t a, value_t b, bool&){ return ~(a | b); }
static value_t op_div(value_t a, value_t b, bool& failed){
	failed = (b == 0);
	return failed ? 0 : a / b;
}
static value_t op_mod(value_t a, value_t b, bool& failed){
	failed = (b == 0);
	return failed ? 0 : a % b;
}

static COMMAND_STATUS binary_command(const command_t& cmd, binop_t op){
	if(cmd.args.size() != 2)
		return wrong_args(cmd, 2);
	abas_value_t dst, src;
	string error;
	if(!resolve_value(cmd.args[0], dst, error) || !resolve_value(cmd.args[1], src, error))
		return fail_at(cmd, cmd.command + ": " + error);
	if(dst.kind == V_STR || src.kind == V_STR)
		return fail_at(cmd, cmd.command + ": works on numbers only");
	bool failed = false;
	value_t result = op(dst.num, src.num, failed);
	if(failed)
		return fail_at(cmd, cmd.command + ": division by zero");
	abas_value_t out;
	out.num = result;
	if(!store_value(cmd.args[0], out, error))
		return fail_at(cmd, cmd.command + ": " + error);
	return OK;
}

// a shift moves bits, and a word here is 64 of them, so the amount is a number
// of bits and has to be 0 to 63. the bits that come in at the end are zeros, and
// a word is shifted as the 64 bits it is, sign bit and all, which is the same
// ground and, or and xor work on. so the sign of the number is only a bit among
// the others here, and a bit that falls off the top is gone and a bit that comes
// in at the bottom is a zero: lsh is a number twice the size, or half the size
// where the size of a word runs out first
static value_t op_lsh(value_t a, value_t b, string& why){
	if(b > 63){
		why = to_string(b) + " is more bits than a word has, a word has 64";
		return 0;
	}
	return a << b;
}

// a number moved down carries its sign into the bits that come in, the way a
// division toward minus infinity does, and rsh has zeros there instead. so the
// sign is taken back off the top after the shift, and the mask is every bit under
// the ones that were moved: 63 bits under one bit moved, 1 bit under 63 moved.
// rsh is therefore not a way of halving a number that may be negative and div is,
// since div rounds toward zero and rsh rounds down
static value_t op_rsh(value_t a, value_t b, string& why){
	if(b > 63){
		why = to_string(b) + " is more bits than a word has, a word has 64";
		return 0;
	}
	if(b == 0)
		return a;
	return (a >> b) & (((value_t)1 << (64 - b)) - 1);
}

// a power of two is the one number a shift cannot make on its own, since a shift
// only ever multiplies by two. so the answer is worked out here rather than by
// shifting.
//
// the loop squares the base and halves the exponent as it goes, which is the
// same trick as writing the exponent out in binary: every bit of the exponent
// that is up takes one copy of the matching power of the base, and those powers
// grow twice as big on each turn
//
//   pow a, 5       a, then a*a, then a*a*a*a, then a*a*a*a*a
//   pow a, 1000    ten turns of squaring and one multiply for each bit of 1000
//
// so a thousand is a handful of multiplies rather than a thousand of them, and
// that is the whole of the speed. ten to the thousandth is a number with a
// thousand digits in it and a word holds nineteen, so it goes round the same way
// mul does: the answer is the true one with as many 2^64s taken off the top as
// it takes to make it fit, and that is not a mistake, it is what a word is
//
// the work is done on the 64 bits of the base as they stand, so a base with the
// sign bit up needs no special care: raising a negative number to an odd power
// leaves the sign bit up and to an even power takes it down, and the multiplies
// see to that on their own
//
// the exponent is a number of whole turns, so a negative one is an error rather
// than a fraction, and anything to the power of zero is one
static value_t op_pow(value_t a, value_t b, string& why){
	if(b == 0)
		return 1;
	if(b < 0){
		b *= -1;
		//why = "the exponent must not be negative, a power of a whole number of turns is a whole number";
		why = "";
		//return 0;
	}
	// the squaring is a multiply of words, and a multiply of words that does not fit
// goes round rather than being undefined, so the answer is always a word and
// always the right one as far as a word can be
	value_t result = 1;
	value_t base = a;
	value_t turns = b;
	while(turns > 0){
		if(turns & 1)
			result *= base;
		turns >>= 1;
		if(turns == 0) // the last square would be of no use to anybody
			break;
		base *= base;
	}
	return result;
}

typedef value_t (*amountop_t)(value_t a, value_t b, string& why);

// lsh, rsh and pow are add and the rest with a first place and an amount, and
// they differ only in what they do with the two
static COMMAND_STATUS amount_command(const command_t& cmd, amountop_t op){
	if(cmd.args.size() != 2)
		return wrong_args(cmd, 2);
	abas_value_t dst, src;
	string error;
	if(!resolve_value(cmd.args[0], dst, error) || !resolve_value(cmd.args[1], src, error))
		return fail_at(cmd, cmd.command + ": " + error);
	if(dst.kind == V_STR || src.kind == V_STR)
		return fail_at(cmd, cmd.command + ": works on numbers only");
	// an amount of bits is a number of bits, so a negative one is a mistake
	// before any work is done with it
	if(cmd.command != "pow" && src.num < 0)
		return fail_at(cmd, cmd.command + ": " + to_string(src.num) +
		                             " is not a number of bits, a word has 64");
	string why;
	value_t result = op(dst.num, src.num, why);
	if(!why.empty())
		return fail_at(cmd, cmd.command + ": " + why);
	abas_value_t out;
	out.num = result;
	if(!store_value(cmd.args[0], out, error))
		return fail_at(cmd, cmd.command + ": " + error);
	return OK;
}

static COMMAND_STATUS cmd_mov(const command_t& cmd){
	if(cmd.args.size() != 2)
		return wrong_args(cmd, 2);
	abas_value_t src;
	string error;
	if(!resolve_value(cmd.args[1], src, error))
		return fail_at(cmd, "mov: " + error);
	if(!store_value(cmd.args[0], src, error))
		return fail_at(cmd, "mov: " + error);
	return OK;
}

// 'var' writes a name down before it is worked on. nothing needs it, any
// command that writes to a name makes it, so a program may leave every var out
// and not one thing changes
// var writes down the names a piece of work is going to need, and gives each of
// them the value 0, so the names of a program are gathered in one place at the top
// of it and every one of them is there to be worked on before it holds anything
// worth working with:
//
//   var a, b, c      a, b and c all hold 0
//
// as many names as are wanted, and every argument of a var is a name. there is no
// value to give here, and that is the whole of it: a name holding something other
// than 0 is a mov that says 0 out loud, so 'var a, 5' is not a shorter mov, the 5
// is not a name, and abas says so rather than making one of the two meanings up.
// a value goes into a name with mov
static COMMAND_STATUS cmd_var(const command_t& cmd){
	if(cmd.args.empty())
		return fail_at(cmd, "var: expected a name or more, var name, name, name");
	abas_value_t zero;
	zero.kind = V_INT;
	zero.num = 0;
	zero.str = "";
	for(size_t i = 0 ; i < cmd.args.size() ; i++){
		const vector<TOKEN>& arg = cmd.args[i];
		if(arg.size() != 1 || arg[0].type != IDENTIFIER)
			return fail_at(cmd, "var: expected a name, every argument of a var is a name, and a value goes into one with mov");
		vm.vars[arg[0].value] = zero;
	}
	return OK;
}

static COMMAND_STATUS cmd_not(const command_t& cmd){
	if(cmd.args.size() != 1)
		return wrong_args(cmd, 1);
	abas_value_t dst;
	string error;
	if(!resolve_value(cmd.args[0], dst, error))
		return fail_at(cmd, "not: " + error);
	if(dst.kind == V_STR)
		return fail_at(cmd, "not: works on numbers only");
	abas_value_t out;
	out.num = ~dst.num;
	if(!store_value(cmd.args[0], out, error))
		return fail_at(cmd, "not: " + error);
	return OK;
}

// inc and dec walk every argument in turn and change each one by the amount they
// were given, so one line does the work of as many as there were names in it:
//   inc a, b, $3    a grows by one, b grows by one and slot 3 grows by one
static COMMAND_STATUS add_by(const command_t& cmd, value_t amount){
	if(cmd.args.empty())
		return fail_at(cmd, cmd.command + ": expected at least one argument");
	for(const vector<TOKEN>& arg : cmd.args){
		abas_value_t dst;
		string error;
		if(!resolve_value(arg, dst, error))
			return fail_at(cmd, cmd.command + ": " + error);
		if(dst.kind == V_STR)
			return fail_at(cmd, cmd.command + ": works on numbers only");
		abas_value_t out;
		out.num = dst.num + amount;
		if(!store_value(arg, out, error))
			return fail_at(cmd, cmd.command + ": " + error);
	}
	return OK;
}

static COMMAND_STATUS cmd_inc(const command_t& cmd){ return add_by(cmd, 1); }
static COMMAND_STATUS cmd_dec(const command_t& cmd){ return add_by(cmd, (value_t)-1); }

// text goes on the stack one byte per word, a number takes a single word
static COMMAND_STATUS push_value(const command_t& cmd, const abas_value_t& value){
	if(value.kind == V_STR){
		if(vm.stack.size() + value.str.size() > (size_t)STACK_MAX_WORDS)
			return fail_at(cmd, cmd.command + ": the stack cannot hold " +
			                   to_string(value.str.size()) + " more word(s), it holds " +
			                   to_string(STACK_MAX_WORDS));
		for(char c : value.str)
			vm.stack.push_back((value_t)(unsigned char)c);
		return OK;
	}
	if(vm.stack.size() + 1 > (size_t)STACK_MAX_WORDS)
		return fail_at(cmd, cmd.command + ": the stack is full, it holds " +
		                   to_string(STACK_MAX_WORDS) + " word(s)");
	vm.stack.push_back(value.num);
	return OK;
}

// push takes any number of arguments and works through them in the order they
// are written, whatever each one of them says:
//
//   push name      the value in that name goes on the top of the stack
//   push $3        the word in slot 3 goes on the top of the stack
//   push $$3       the same word, a second dollar means the same as one
//   push 'a'       the code of the character goes on the stack
//   push "hi"      one byte per word goes on the stack, the last byte on top
//   push 10        the number 10 goes on the stack
//   push a, b, c   three values, in the order they are written
static COMMAND_STATUS cmd_push(const command_t& cmd){
	if(cmd.args.empty())
		return fail_at(cmd, "push: expected at least one argument");
	for(const vector<TOKEN>& arg : cmd.args){
		abas_value_t value;
		string error;
		if(!resolve_value(arg, value, error))
			return fail_at(cmd, "push: " + error);
		COMMAND_STATUS status = push_value(cmd, value);
		if(status != OK)
			return status;
	}
	return OK;
}

// pop is the way back, and it takes any number of arguments as well:
//
//   pop name        the top of the stack goes into that name, one word off
//   pop $3          the top of the stack goes into slot 3, one word off
//   pop 10          the number on top of the stack goes into that name
//   pop a, b, c     three words off the stack, the top into a, the one under it
//                   into b and the one under that into c
//
// so one line can empty as much of the stack as the caller cares to name, and
// 'pop n' on a name and 'pop 10' on a number are the same kind of command
static COMMAND_STATUS cmd_pop(const command_t& cmd){
	if(cmd.args.empty())
		return fail_at(cmd, "pop: expected at least one argument");
	for(const vector<TOKEN>& arg : cmd.args){
		if(vm.stack.empty())
			return fail_at(cmd, "pop: the stack is empty");
		value_t value = vm.stack.back();
		vm.stack.pop_back();
		abas_value_t out;
		out.num = value;
		string error;
		if(!store_value(arg, out, error))
			return fail_at(cmd, "pop: " + error);
	}
	return OK;
}

// rpush and rpop are push and pop with the arguments written the other way
// round, and nothing else. push puts the first argument on the bottom of the
// stack and pop hands the top of it to the first name written, so the two
// together walk the stack from one end. rpush and rpop walk it from the other:
//
//   push a, b, c   a goes on the bottom and c on the top
//   pop  a, b, c   c comes off into a, b into b and a into c
//   rpush a, b, c  the same as push c, b, a, so a is the one on top
//   rpop  a, b, c  the same as pop c, b, a, so the top comes off into c
//
// the values themselves are not touched. a func that reads what it was given off
// the stack in the order its names are written is then called with one rpush and
// one rpop, which is what they are for
static COMMAND_STATUS cmd_rpush(const command_t& cmd){
	command_t flipped = cmd;
	reverse(flipped.args.begin(), flipped.args.end());
	return cmd_push(flipped);
}

static COMMAND_STATUS cmd_rpop(const command_t& cmd){
	command_t flipped = cmd;
	reverse(flipped.args.begin(), flipped.args.end());
	return cmd_pop(flipped);
}

// 'grow value' moves the top of the stack by the amount that value says, so a
// positive number makes room and a negative number takes room back. it is the
// one command that changes how long the stack is without touching a single word
// of what is on it, and that is how a function reserves scratch and hands it
// back again:
//   grow 4       four words go on the stack, all of them 0
//   grow -4      those four words come off again
//   grow n       as many words as n says, and n may be negative
// a stack cannot go below empty, so taking more words off than are there leaves
// it empty rather than stopping with an error
static COMMAND_STATUS cmd_grow(const command_t& cmd){
	if(cmd.args.size() != 1)
		return wrong_args(cmd, 1);
	abas_value_t value;
	string error;
	if(!resolve_value(cmd.args[0], value, error))
		return fail_at(cmd, "grow: " + error);
	if(value.kind == V_STR)
		return fail_at(cmd, "grow: expected a number, got text");
	size_t size = vm.stack.size();
	value_t by = value.num;
	if(by >= 0){
		if(by > STACK_MAX_WORDS - (value_t)size)
			return fail_at(cmd, "grow: the stack cannot hold " + to_string(by) +
			                   " more word(s), it holds " + to_string(STACK_MAX_WORDS));
		vm.stack.resize(size + (size_t)by, 0);
		return OK;
	}
	// the size of a negative amount
	value_t take = -by;
	vm.stack.resize(take >= (value_t)size ? 0 : size - (size_t)take);
	return OK;
}

static COMMAND_STATUS cmd_gsp(const command_t& cmd){
	if(cmd.args.size() != 1)
		return wrong_args(cmd, 1);
	abas_value_t out;
	out.num = (value_t)vm.stack.size();
	string error;
	if(!store_value(cmd.args[0], out, error))
		return fail_at(cmd, "gsp: " + error);
	return OK;
}

static COMMAND_STATUS cmd_ssp(const command_t& cmd){
	if(cmd.args.size() != 1)
		return wrong_args(cmd, 1);
	abas_value_t value;
	string error;
	if(!resolve_value(cmd.args[0], value, error))
		return fail_at(cmd, "ssp: " + error);
	if(value.kind == V_STR)
		return fail_at(cmd, "ssp: expected a number, got text");
	if(value.num > STACK_MAX_WORDS)
		return fail_at(cmd, "ssp: " + to_string(value.num) + " is too big, the stack holds at most " +
		                   to_string(STACK_MAX_WORDS) + " word(s)");
	vm.stack.resize(value.num, 0);
	return OK;
}

// the four commands that throw things away, so a program that wants a clean
// slate can have one without ending. each of them clears one piece of what the
// program has made so far, and clear does all of them at once
//
// clv, clf and clear are written with no arguments to throw all of their names
// away, and written with names to throw away only those. a name that is not there
// to be thrown away is not a mistake, it is already clear, so nothing is said
// about it. a func and a flag are one kind of name to clf and to clear, so a
// name given to either goes out of both lists whatever it was in

// reads the arguments as names, and does it for every one of them before any of
// them is thrown away, so a mistake in the last name of a line does not leave the
// first few of them gone
static bool names_given(const command_t& cmd, vector<string>& names, string& error){
	for(const vector<TOKEN>& arg : cmd.args){
		if(arg.size() != 1 || arg[0].type != IDENTIFIER){
			error = (arg.empty() ? "<missing>" : arg[0].value) + " is not a name";
			return false;
		}
		names.push_back(arg[0].value);
	}
	return true;
}

// 'cls' is 'ssp 0' under another name, the stack goes back to being empty. it
// takes no arguments, since there is one stack and no name in it to name
static COMMAND_STATUS cmd_cls(const command_t& cmd){
	if(!cmd.args.empty())
		return fail_at(cmd, "cls: takes no arguments");
	vm.stack.clear();
	return OK;
}

// 'clv' throws every name away, so a name that is read after it has no value
// yet and has to be given one again. given the names of some of them it throws
// away only those, and leaves the rest of the names of the program alone
static COMMAND_STATUS cmd_clv(const command_t& cmd){
	vector<string> names;
	string error;
	if(!names_given(cmd, names, error))
		return fail_at(cmd, "clv: " + error);
	if(names.empty()){
		vm.vars.clear();
		return OK;
	}
	for(const string& name : names)
		vm.vars.erase(name);
	return OK;
}

// 'clf' throws the names of the funcs and of the flags away. the funcs themselves
// are still in the command stream, but nothing knows them, so a call or a jump to
// a name that is no longer in either list stops with the name not being a flag
// and not a func. given the names of some of them it throws away only those, and
// a name given to it is a name of a func or of a flag as far as it is concerned,
// so it goes out of both lists either way
static COMMAND_STATUS cmd_clf(const command_t& cmd){
	vector<string> names;
	string error;
	if(!names_given(cmd, names, error))
		return fail_at(cmd, "clf: " + error);
	if(names.empty()){
		vm.funcs.clear();
		vm.flags.clear();
		return OK;
	}
	for(const string& name : names){
		vm.funcs.erase(name);
		vm.flags.erase(name);
	}
	return OK;
}

// 'clear' is all three of them at once: the stack, the names, and the names of
// the funcs and of the flags. the stack of return addresses is not touched, since
// that is not something the program made, and a call that is running still has to
// be able to come back. given the names of some of them it throws away only those,
// out of all three lists, and one name that is in none of them simply goes
static COMMAND_STATUS cmd_clear(const command_t& cmd){
	vector<string> names;
	string error;
	if(!names_given(cmd, names, error))
		return fail_at(cmd, "clear: " + error);
	vm.stack.clear();
	if(names.empty()){
		vm.vars.clear();
		vm.funcs.clear();
		vm.flags.clear();
		return OK;
	}
	for(const string& name : names){
		vm.vars.erase(name);
		vm.funcs.erase(name);
		vm.flags.erase(name);
	}
	return OK;
}

static COMMAND_STATUS cmd_gcp(const command_t& cmd){
	if(cmd.args.size() != 1)
		return wrong_args(cmd, 1);
	abas_value_t out;
	out.num = (value_t)cmd.index;
	string error;
	if(!store_value(cmd.args[0], out, error))
		return fail_at(cmd, "gcp: " + error);
	return OK;
}

static COMMAND_STATUS cmd_get(const command_t& cmd){
	if(cmd.args.size() != 1)
		return wrong_args(cmd, 1);
	int c = cin.get();
	if(c == EOF)
		return fail_at(cmd, "get: there is nothing left to read");
	abas_value_t out;
	out.num = (value_t)(unsigned char)c;
	string error;
	if(!store_value(cmd.args[0], out, error))
		return fail_at(cmd, "get: " + error);
	return OK;
}

static COMMAND_STATUS cmd_put(const command_t& cmd, ostream& out, const string& name){
	if(cmd.args.size() != 1)
		return wrong_args(cmd, 1);
	abas_value_t value;
	string error;
	if(!resolve_value(cmd.args[0], value, error))
		return fail_at(cmd, name + ": " + error);
	if(value.kind == V_STR)
		out << value.str;
	else
		out.put((char)(unsigned char)(value.num & 0xFF));
	out.flush();
	return OK;
}

static COMMAND_STATUS cmd_put_stdout(const command_t& cmd){ return cmd_put(cmd, cout, "put"); }
static COMMAND_STATUS cmd_err(const command_t& cmd){ return cmd_put(cmd, cerr, "err"); }

// 'print' takes any number of arguments and writes each one in the order it is
// written, a number in base ten and text as it stands. there is no separator
// between them and no newline at the end, since a number is not a thing that can
// be measured in words and a line is better said by the program than by print:
//
//   print a, b, c   three values, one after another, and nothing else
//   print "hi"      the text
//   print 42        the number 42
//   print "\n"      a line end
//   print           nothing at all
//
// a character is a number, so the two ways of writing a line end are not the
// same thing: print "\n" writes the line end itself and print '\n' writes the
// number 10
static COMMAND_STATUS cmd_print(const command_t& cmd){
	for(const vector<TOKEN>& arg : cmd.args){
		abas_value_t value;
		string error;
		if(!resolve_value(arg, value, error))
			return fail_at(cmd, "print: " + error);
		print_value(value, cout);
	}
	cout.flush();
	return OK;
}

// 'func name' opens a body and 'endfunc' closes it, so the body of a func is
// every command between the two and nothing else. A func the program walks into on
// its own is a definition nobody asked for, so it steps over its own body and
// carries on behind it, which is what lets a file of nothing but funcs, a library,
// sit at the top of a program.
static COMMAND_STATUS cmd_func(const command_t& cmd){
	if(cmd.args.size() != 1)
		return wrong_args(cmd, 1);
	if(cmd.args[0].size() != 1 || cmd.args[0][0].type != IDENTIFIER)
		return fail_at(cmd, "func: the argument must be a name");
	auto over = vm.over.find(cmd.index);
	if(over != vm.over.end())
		vm.pc = over->second;
	return OK;
}

// 'flag name' does nothing else. it saves the place the program is at under that
// name, and a jump to the name sets the place the program is at to it, which
// lands on the flag itself. the flag runs again as it passes it, saves the same
// place once more, and carries on into the line under it, so a loop is a flag
// and the jump that goes back to it.
//
// a name may stand in front of as many flags as a program likes. the last one in
// the program is the one a jump goes to, and the ones before it are simply
// forgotten
//
// the names are read out of the whole stream before the program runs, so that a
// jump may go forward to a flag that is still ahead of it, and a flag run again
// on a later turn of a loop finds its name already there. running a flag is
// therefore nothing at all beyond being a name in the stream that a jump can
// reach: the stream decides, and not the order the program happens to run in
static COMMAND_STATUS cmd_flag(const command_t& cmd){
	if(cmd.args.size() != 1)
		return wrong_args(cmd, 1);
	if(cmd.args[0].size() != 1 || cmd.args[0][0].type != IDENTIFIER)
		return fail_at(cmd, "flag: the argument must be a name");
	return OK;
}

// where a jump and a call land. a flag is a place in the stream and a func is a
// body, so the two are not the same, and a name that is both goes to the flag. a
// name that is neither is a mistake
static bool target_of(const string& name, value_t& out){
	auto flag = vm.flags.find(name);
	if(flag != vm.flags.end()){
		out = flag->second;
		return true;
	}
	auto func = vm.funcs.find(name);
	if(func != vm.funcs.end()){
		out = func->second + 1; // the body of a func starts under the func
		return true;
	}
	return false;
}

// a call is a jump that saves where it was made, and it is made on the name of a
// func or on the name of a flag, whichever the program finds the name under first
// and whichever the program finds easier to read. a flag is only a name for a
// place in the stream, so a call to a flag lands on the flag itself and the
// stream carries on under it, and a body that is jumped into runs the same way
//
// the difference is what comes back. a jump to a func leaves the program to carry
// on under the endfunc of the body, so the body has to leave by a jump of its
// own. a call saves the place it was made, so the body comes back to the line
// under the call with a ret or an endfunc, and the difference is a name for a
// place and a body is the only one there is between them
static COMMAND_STATUS cmd_call(const command_t& cmd){
	if(cmd.args.size() != 1)
		return wrong_args(cmd, 1);
	string name;
	string error;
	if(!read_label_name(cmd, 0, name, error))
		return fail_at(cmd, error);
	value_t at = 0;
	if(!target_of(name, at))
		return fail_at(cmd, "call: '" + name + "' is not a flag and not a func");
	if(vm.max_call_depth > 0 && (value_t)vm.frames.size() >= vm.max_call_depth)
		return fail_at(cmd, "call: " + to_string(vm.max_call_depth) +
		                   " functions deep, that is the limit --function-depth set");
	// the place the call was made goes on the stack of return addresses, where
	// no command can reach it. the one stack is left exactly as the caller had it
	vm.rets.push_back((value_t)(cmd.index + 1));
	vm.frames.push_back(name);
	vm.pc = at;
	return OK;
}

// comes back from a call. the saved place comes off the stack of return
// addresses and the command pointer is set to it, so the program carries on
// under the line the call was made on. the values a ret is written with are
// pushed first, in the order they are written, and whatever the body left on the
// one stack stays there, so a func can build something for the caller and hand
// it over a word at a time
static COMMAND_STATUS leave_call(const command_t& cmd, const string& name){
	if(vm.frames.empty())
		return fail_at(cmd, name + ": there is no call to return from");
	if(vm.rets.empty())
		return fail_at(cmd, name + ": the return address is not there");
	value_t address = vm.rets.back();
	// a call on the last line of a program saves the line under it, and there is
	// no line under it. that is the one place a return address may sit past the
	// last command, and landing on it is how the program says it is done
	if(address > (value_t)vm.code_size)
		return fail_at(cmd, name + ": the return address " + to_string(address) +
		                   " is not a command");
	string error;
	for(const vector<TOKEN>& arg : cmd.args){
		abas_value_t value;
		if(!resolve_value(arg, value, error))
			return fail_at(cmd, name + ": " + error);
		COMMAND_STATUS status = push_value(cmd, value);
		if(status != OK)
			return status;
	}
	vm.rets.pop_back();
	vm.frames.pop_back();
	vm.pc = (value_t)address;
	return OK;
}

static COMMAND_STATUS cmd_ret(const command_t& cmd){ return leave_call(cmd, "ret"); }

// 'endfunc' closes a body and gives nothing back. it is a ret with nothing written
// after it, so the caller pops nothing and the stack is left as the body left it
static COMMAND_STATUS cmd_endfunc(const command_t& cmd){
	if(!cmd.args.empty())
		return fail_at(cmd, "endfunc: takes no arguments");
	return leave_call(cmd, "endfunc");
}

// a loop is a flag and a jump under a name of its own. 'loop' is the flag a
// continue lands back on and 'endloop' is the jump that walks out from under the
// body, so neither of them does anything when it runs and the body is simply the
// commands between the two. what goes round is the continue, and a continue with
// no way out of the loop in any of its turns runs for ever. the two places a loop
// is jumped to are worked out from the whole stream before the program runs, so a
// loop may sit inside a func and inside another loop and the break or the
// continue still belongs to the one it was written in.
//
//   loop              a continue lands back here, the head of the body
//     ...
//   continue          go back to the loop this line is in
//   break             go to the command under the endloop
//   endloop           the tail of the body, and then the next command
//
// either end of a loop may carry a name, and a name there is a flag as well, so a
// jump reaches a loop from anywhere in the program under the same rule as
// anything else: the last name written for a place is the one a jump goes to
//
//   loop top          a jump to top lands on the head of the body
//   endloop out       a jump to out lands under the endloop, where a break goes
static COMMAND_STATUS named_end(const command_t& cmd){
	if(cmd.args.empty())
		return OK;
	if(cmd.args.size() > 1)
		return fail_at(cmd, cmd.command + ": takes one name or nothing at all, got " +
		                   to_string(cmd.args.size()) + " argument(s)");
	if(cmd.args[0].size() != 1 || cmd.args[0][0].type != IDENTIFIER)
		return fail_at(cmd, cmd.command + ": the argument must be a name");
	return OK;
}

static COMMAND_STATUS cmd_loop(const command_t& cmd){ return named_end(cmd); }
static COMMAND_STATUS cmd_endloop(const command_t& cmd){ return named_end(cmd); }

static COMMAND_STATUS jump_out_of_loop(const command_t& cmd, bool to_head){
	if(!cmd.args.empty())
		return fail_at(cmd, cmd.command + ": takes no arguments");
	auto it = vm.loops.find(cmd.index);
	if(it == vm.loops.end())
		return fail_at(cmd, cmd.command + ": there is no loop around it");
	vm.pc = to_head ? it->second.head : it->second.tail;
	return OK;
}

static COMMAND_STATUS cmd_continue(const command_t& cmd){ return jump_out_of_loop(cmd, true); }
static COMMAND_STATUS cmd_break(const command_t& cmd){ return jump_out_of_loop(cmd, false); }

static COMMAND_STATUS cmd_cmp(const command_t& cmd){
	if(cmd.args.size() != 2)
		return wrong_args(cmd, 2);
	abas_value_t a, b;
	string error;
	if(!resolve_value(cmd.args[0], a, error) || !resolve_value(cmd.args[1], b, error))
		return fail_at(cmd, "cmp: " + error);
	int result = 0;
	if(a.kind == V_STR && b.kind == V_STR){
		if(a.str < b.str)
			result = -1;
		else if(a.str > b.str)
			result = 1;
	}
	else{
		value_t x = a.num;
		value_t y = b.num;
		if(x < y)
			result = -1;
		else if(x > y)
			result = 1;
	}
	vm.cmp_result = result;
	return OK;
}

static COMMAND_STATUS jump_to_label(const command_t& cmd, bool take_it){
	if(!take_it)
		return OK;
	string name;
	string error;
	if(!read_label_name(cmd, 0, name, error))
		return fail_at(cmd, error);
	value_t at = 0;
	if(!target_of(name, at))
		return fail_at(cmd, cmd.command + ": '" + name +
		                             "' is not a flag and not a func");
	vm.pc = at;
	return OK;
}

static COMMAND_STATUS cmd_jmp(const command_t& cmd){ return jump_to_label(cmd, true); }

static COMMAND_STATUS cmd_je(const command_t& cmd){ return jump_to_label(cmd, vm.cmp_result == 0); }
static COMMAND_STATUS cmd_jne(const command_t& cmd){ return jump_to_label(cmd, vm.cmp_result != 0); }
static COMMAND_STATUS cmd_jl(const command_t& cmd){ return jump_to_label(cmd, vm.cmp_result < 0); }
static COMMAND_STATUS cmd_jg(const command_t& cmd){ return jump_to_label(cmd, vm.cmp_result > 0); }
static COMMAND_STATUS cmd_jle(const command_t& cmd){ return jump_to_label(cmd, vm.cmp_result <= 0); }
static COMMAND_STATUS cmd_jge(const command_t& cmd){ return jump_to_label(cmd, vm.cmp_result >= 0); }

static COMMAND_STATUS cmd_exit(const command_t& cmd){
	if(cmd.args.size() != 1)
		return wrong_args(cmd, 1);
	abas_value_t value;
	string error;
	if(!resolve_value(cmd.args[0], value, error))
		return fail_at(cmd, "exit: " + error);
	if(value.kind == V_STR)
		return fail_at(cmd, "exit: expected a number, got text");
	vm.exit_code = (int)(value.num & 0xFF);
	vm.running = false;
	return ABORT;
}

static COMMAND_STATUS cmd_include(const command_t& cmd){
	return fail_at(cmd, "include: files are already loaded, this line came from nowhere");
}

map<string, command_fn> functions = {
	{ "include", cmd_include },

	{ "var", cmd_var },
	{ "mov", cmd_mov },

	{ "or", [](const command_t& c){ return binary_command(c, op_or); } },
	{ "and", [](const command_t& c){ return binary_command(c, op_and); } },
	{ "xor", [](const command_t& c){ return binary_command(c, op_xor); } },
	{ "nor", [](const command_t& c){ return binary_command(c, op_nor); } },
	{ "not", cmd_not },

	{ "add", [](const command_t& c){ return binary_command(c, op_add); } },
	{ "sub", [](const command_t& c){ return binary_command(c, op_sub); } },
	{ "mul", [](const command_t& c){ return binary_command(c, op_mul); } },
	{ "div", [](const command_t& c){ return binary_command(c, op_div); } },
	{ "mod", [](const command_t& c){ return binary_command(c, op_mod); } },
	{ "pow", [](const command_t& c){ return amount_command(c, op_pow); } },
	{ "lsh", [](const command_t& c){ return amount_command(c, op_lsh); } },
	{ "rsh", [](const command_t& c){ return amount_command(c, op_rsh); } },
	{ "inc", cmd_inc },
	{ "dec", cmd_dec },

	{ "push", cmd_push },
	{ "rpush", cmd_rpush },
	{ "pop", cmd_pop },
	{ "rpop", cmd_rpop },
	{ "gsp", cmd_gsp },
	{ "ssp", cmd_ssp },
	{ "grow", cmd_grow },
	{ "gcp", cmd_gcp },

	{ "cls", cmd_cls },
	{ "clv", cmd_clv },
	{ "clf", cmd_clf },
	{ "clear", cmd_clear },

	{ "get", cmd_get },
	{ "put", cmd_put_stdout },
	{ "err", cmd_err },
	{ "print", cmd_print },

	{ "flag", cmd_flag },
	{ "func", cmd_func },
	{ "endfunc", cmd_endfunc },
	{ "loop", cmd_loop },
	{ "endloop", cmd_endloop },
	{ "continue", cmd_continue },
	{ "break", cmd_break },
	{ "cmp", cmd_cmp },
	{ "jmp", cmd_jmp },
	{ "je", cmd_je },
	{ "jne", cmd_jne },
	{ "jl", cmd_jl },
	{ "jg", cmd_jg },
	{ "jle", cmd_jle },
	{ "jge", cmd_jge },
	{ "call", cmd_call },
	{ "ret", cmd_ret },

	{ "exit", cmd_exit },
};

// ------------------------------------------------------------------- runner

static bool build_error(const command_t& cmd, const string& msg){
	fail_at(cmd, msg);
	return false;
}

// these commands walk to a flag or to a func, and only a name that one of the
// two holds can be walked to
static bool jumps_to_label(const string& command){
	return command == "call" || command == "jmp" || command == "je" ||
	       command == "jne" || command == "jl" || command == "jg" ||
	       command == "jle" || command == "jge";
}

// reads the funcs and the flags out of the command stream before the first
// command runs, that way a jump can also go forward to something that is still
// ahead in the stream.
//
// Every func is closed by an endfunc and every loop by an endloop, so a stack of
// the blocks that are open says where the body of each one ends, and what the
// program carries on at when it walks into a func is the command under that
// endfunc. A stack is what makes a func inside a func work, and it is also what
// says a body is unclosed: the stack is not empty when the commands run out. An
// endfunc with nothing open under it has closed a body that was never opened,
// and the same goes for an endloop closing a func or for a loop with no endloop.
//
// A break and a continue want the two ends of the loop they were written in, so
// every one of them is remembered while the stream is read and is told those two
// places when its own endloop goes past. That is what says a break belongs to
// the loop around it rather than to the loop the program happens to be walking
// when it gets there, and it is why a loop may sit inside a loop and inside a
// func.
//
// A name may stand in front of as many flags as a program likes, and the last
// one in the program is the one a jump goes to, the earlier ones being
// forgotten. The same goes for a name in front of more than one func.
struct open_block_t{
	value_t index = 0;   // the 'func' or the 'loop' itself
	string name = "";              // the name of a func, empty for a loop
	bool is_loop = false;
	vector<size_t> jumps;          // the breaks and the continues of a loop
};

// a name in front of a flag is a name in front of a place, and so is a name in
// front of a loop or in front of an endloop: the first is the head of a body and
// the second the command under its tail, which is where a break goes. all three
// are read here, in the order they are written, so the last name written for a
// place is the one a jump reaches.
//
// a func and a flag have to be named, a loop and an endloop need not be, so the
// answer is three ways and not two: NAMED, UNNAMED and BAD
typedef enum{ NAMED = 0, UNNAMED, BAD_NAME } name_status_t;

static name_status_t read_block_name(const command_t& cmd, string& out){
	if(cmd.args.empty())
		return UNNAMED;
	if(cmd.args.size() > 1)
		return build_error(cmd, cmd.command + ": expected one name, got " +
		                       to_string(cmd.args.size()) + " argument(s)"),
		       BAD_NAME;
	if(cmd.args[0].size() != 1 || cmd.args[0][0].type != IDENTIFIER)
		return build_error(cmd, cmd.command + ": the argument must be a name"), BAD_NAME;
	out = cmd.args[0][0].value;
	return NAMED;
}

static bool build_tables(const vector<command_t>& code){
	vector<open_block_t> open;
	for(size_t i = 0 ; i < code.size() ; i++){
		const command_t& cmd = code[i];
		if(cmd.command == "func"){
			string name;
			if(read_block_name(cmd, name) != NAMED)
				return build_error(cmd, "func: expected the name of a func");
			vm.funcs[name] = cmd.index;
			open_block_t block;
			block.index = cmd.index;
			block.name = name;
			open.push_back(block);
			continue;
		}
		if(cmd.command == "loop"){
			open_block_t block;
			block.index = cmd.index;
			block.is_loop = true;
			open.push_back(block);
			continue;
		}
		if(cmd.command == "endfunc"){
			if(open.empty() || open.back().is_loop)
				return build_error(cmd, "endfunc: there is no func above it that is still open");
			vm.over[open.back().index] = (value_t)(i + 1);
			open.pop_back();
			continue;
		}
		if(cmd.command == "endloop"){
			if(open.empty() || !open.back().is_loop)
				return build_error(cmd, "endloop: there is no loop above it that is still open");
			loop_t loop;
			loop.head = open.back().index;
			loop.tail = (value_t)(i + 1);
			for(size_t at : open.back().jumps)
				vm.loops[code[at].index] = loop;
			open.pop_back();
			continue;
		}
		if(cmd.command != "break" && cmd.command != "continue")
			continue;
		if(open.empty() || !open.back().is_loop)
			return build_error(cmd, cmd.command + ": there is no loop around it");
		open.back().jumps.push_back(i);
	}
	if(!open.empty()){
		const command_t& cmd = code[open.back().index];
		if(open.back().is_loop)
			return build_error(cmd, "loop has no endloop");
		return build_error(cmd, "func '" + cmd.args[0][0].value + "' has no endfunc");
	}

	for(size_t i = 0 ; i < code.size() ; i++){
		const command_t& cmd = code[i];
		bool is_flag = cmd.command == "flag";
		if(!is_flag && cmd.command != "loop" && cmd.command != "endloop")
			continue;
		string name;
		name_status_t status = read_block_name(cmd, name);
		if(status == BAD_NAME)
			return false;
		if(status == UNNAMED){
			// a loop and an endloop are written without a name as often as not,
			// a flag is nothing but a name so an empty one says nothing
			if(is_flag)
				return build_error(cmd, "flag: expected the name of a flag, got no arguments");
			continue;
		}
		vm.flags[name] = (cmd.command == "endloop") ? (value_t)(i + 1) : cmd.index;
	}

	for(const command_t& cmd : code){
		if(!jumps_to_label(cmd.command))
			continue;
		string name, error;
		if(!read_label_name(cmd, 0, name, error))
			return build_error(cmd, cmd.command + ": " + error);
		// a call may be made on the name of a func or on the name of a flag,
		// and a jump goes to the same two
		if(vm.flags.count(name) == 0 && vm.funcs.count(name) == 0)
			return build_error(cmd, cmd.command + ": '" + name +
			                             "' is not a flag and not a func");
	}
	return true;
}

static bool check_command(const command_t& cmd){
	if(functions.count(cmd.command))
		return true;
	fail_at(cmd, "unknown command '" + cmd.command + "'");
	return false;
}

int abas_check(vector<command_t>& code, const string& file){
	vm_reset();
	vm.file = file;
	vm.code_size = (value_t)code.size();
	if(!build_tables(code))
		return 1;
	for(const command_t& cmd : code)
		if(!check_command(cmd))
			return 1;
	return 0;
}

int abas_run(vector<command_t>& code, const string& file){
	vm_reset();
	vm.file = file;
	vm.code_size = (value_t)code.size();

	if(!build_tables(code))
		return 1;

	while(vm.running){
		if(vm.pc < 0 || vm.pc >= vm.code_size)
			break;
		command_t& cmd = code[vm.pc];
		if(!check_command(cmd))
			break;
		value_t here = vm.pc;
		COMMAND_STATUS status = functions[cmd.command](cmd);
		vm.steps++;
		if(status == RUNTIME_ERROR || status == ABORT)
			break;
		if(vm.pc == here)
			vm.pc++;
		if(vm.max_steps > 0 && vm.steps >= vm.max_steps){
			vm.running = false;
			vm.failed = true;
			vm.file = cmd.file;
			vm.error_line = cmd.line;
			vm.error = "the program ran " + to_string(vm.steps) + " steps, that is the limit";
			break;
		}
	}

	// the program can only stop this way while a function was still running,
	// its body ran out before it came back to the call that came in
	if(!vm.failed && !vm.frames.empty()){
		vm.failed = true;
		vm.file = code.empty() ? file : code.back().file;
		vm.error_line = code.empty() ? 0 : code.back().line;
		vm.error = "func '" + vm.frames.back() + "' reached the end of the program without a " +
		           "ret or an endfunc";
	}

	if(vm.failed)
		return 1;
	return vm.exit_code;
}
