#pragma once

#include <map>
#include <string>
#include <vector>
#include <core/lexer.h>

typedef int8_t status_t;

typedef enum{
	OK = 0,
	COMMAND_NOT_FOUND,
	INVALID_ARGS,
	WARN_ARGS,
	RUNTIME_ERROR,
	ABORT,
} COMMAND_STATUS;

// where a command sits in the command stream. a flag holds one of these, and so
// does a func, and so does the place the program is at, and a position is a
// value like any other: a word that says how far along the program is
struct command_t{
	value_t line = 0;
	value_t index = 0; // position inside the command stream, set by the parser
	string command = "";
	string file = "";
	vector<vector<TOKEN>> args; // one entry per argument, already split on ',' / ':'

	size_t argc() const { return args.size(); }
};

struct parse_result_t{
	vector<command_t> code;
	bool ok = true;
	string error = "";
	value_t line = 0;
	string file = "";
};

typedef COMMAND_STATUS (*command_fn)(const command_t&);

// every command abas knows about, the name of the key is the name used in the code
extern map<string, command_fn> functions;

parse_result_t parser(const vector<TOKEN>& tokens, const string& file_path);
// 'lib_paths' are extra places to look for an included file, they come after
// the place of the file that does the include
parse_result_t parse_source(const string& code, const string& file_path);
parse_result_t parse_file(const string& file_path, const vector<string>& lib_paths = {});

// 12, 0x1f, 0b1010, 0o17
bool parse_number(const string& s, value_t& out);
string command_to_string(const command_t& cmd);
