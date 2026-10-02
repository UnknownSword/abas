#include <filesystem>
#include <iostream>
#include <core/parser.h>
#include <utils/file.h>

using namespace std;

static const int MAX_INCLUDE_DEPTH = 32;

static parse_result_t make_error(const string& msg, const string& file, value_t line){
	parse_result_t res;
	res.ok = false;
	res.error = msg;
	res.file = file;
	res.line = line;
	return res;
}

bool parse_number(const string& s, value_t& out){
	if(s.empty())
		return false;
	size_t i = 0;
	int base = 10;
	if(s.size() > 2 && s[0] == '0'){
		switch(s[1]){
			case 'x': case 'X': base = 16; i = 2; break;
			case 'b': case 'B': base = 2;  i = 2; break;
			case 'o': case 'O': base = 8;  i = 2; break;
			default: break;
		}
	}
	if(i >= s.size())
		return false;
	// a number is read into a word a digit at a time, and a number written with
	// more digits than one takes has not made a mistake, it has gone round the way
	// a number in a word goes. 0x and 0b go the same way
	value_t number = 0;
	for(; i < s.size() ; i++){
		char c = s[i];
		int digit;
		if(c >= '0' && c <= '9')
			digit = c - '0';
		else if(c >= 'a' && c <= 'f')
			digit = c - 'a' + 10;
		else if(c >= 'A' && c <= 'F')
			digit = c - 'A' + 10;
		else
			return false;
		if(digit >= base)
			return false;
		number = number * (value_t)base + (value_t)digit;
	}
	out = number;
	return true;
}

static bool is_argument_separator(const TOKEN& token){
	return token.type == DELIMITERS && (token.value == "," || token.value == ":");
}

static void reindex(vector<command_t>& code){
	for(size_t i = 0 ; i < code.size() ; i++)
		code[i].index = (value_t)i;
}

parse_result_t parser(const vector<TOKEN>& tokens, const string& file_path){
	parse_result_t res;
	res.file = file_path;

	command_t current;
	bool opened = false;
	for(const TOKEN& token : tokens){
		switch(token.type){
			case WHITESPACE:
			case COMMENT:
			case COMMENT_ONELINE:
			case COMMENT_MULLINE:
				break;
			case ERROR:
				return make_error(token.value, file_path, token.line);
			case ENDLINE:
				if(opened){
					res.code.push_back(current);
					opened = false;
				}
				break;
			case IDENTIFIER:
				if(!opened){
					current = command_t();
					current.line = token.line;
					current.file = file_path;
					current.command = token.value;
					opened = true;
					break; // the name of the command is not an argument
				}
				if(current.args.empty())
					current.args.push_back(vector<TOKEN>());
				current.args.back().push_back(token);
				break;
			default:
				if(!opened)
					return make_error("unexpected token '" + token.value +
					                   "', expected a command", file_path, token.line);
				if(is_argument_separator(token)){
					current.args.push_back(vector<TOKEN>());
					break;
				}
				if(current.args.empty())
					current.args.push_back(vector<TOKEN>());
				current.args.back().push_back(token);
				break;
		}
	}
	if(opened)
		res.code.push_back(current);
	reindex(res.code);
	return res;
}

// find the path of an included file, relative to the file that includes it
static bool resolve_include_path(const string& wanted, const string& from_file,
                                 const vector<string>& lib_paths, string& out){
	vector<string> tries;
	string dir = filedir(from_file);
	if(!dir.empty())
		tries.push_back(dir + wanted);
	for(const string& path : lib_paths){
		if(path.empty())
			continue;
		string prefix = path;
		if(prefix.back() != '/')
			prefix += "/";
		tries.push_back(prefix + wanted);
	}
	tries.push_back(wanted);
	error_code ec;
	for(const string& candidate : tries){
		if(filesystem::exists(candidate, ec) && filesystem::is_regular_file(candidate, ec)){
			out = candidate;
			return true;
		}
	}
	return false;
}

static parse_result_t parse_code(const string& code, const string& file_path,
                                 vector<string>& open_files, const vector<string>& lib_paths,
                                 int depth);

static parse_result_t parse_file_rec(const string& file_path, vector<string>& open_files,
                                     const vector<string>& lib_paths, int depth){
	if(depth > MAX_INCLUDE_DEPTH)
		return make_error("too many nested includes (limit is " + to_string(MAX_INCLUDE_DEPTH) + ")",
		                  file_path, 0);

	string real_path = file_path;
	error_code ec;
	filesystem::path canonical_path = filesystem::weakly_canonical(file_path, ec);
	if(!ec)
		real_path = canonical_path.string();
	for(const string& open : open_files){
		if(open == real_path)
			return make_error("circular include of '" + file_path + "'", file_path, 0);
	}

	abas_file_t file;
	if(!file.open(file_path))
		return make_error("cannot open file", file_path, 0);

	open_files.push_back(real_path);
	parse_result_t res = parse_code(file.code, file_path, open_files, lib_paths, depth);
	open_files.pop_back();
	return res;
}

// 'include' is a compile time thing, the commands of the included file are pasted
// into the command stream before anything runs
static parse_result_t expand_includes(vector<command_t>& code, vector<string>& open_files,
                                      const vector<string>& lib_paths, int depth){
	vector<command_t> result;
	for(const command_t& cmd : code){
		if(cmd.command != "include"){
			result.push_back(cmd);
			continue;
		}
		if(cmd.args.empty())
			return make_error("include: no file given", cmd.file, cmd.line);
		for(const vector<TOKEN>& arg : cmd.args){
			if(arg.empty() || arg[0].type != STRING)
				return make_error("include: argument must be a file name in quotes",
				                  cmd.file, cmd.line);
			string path;
			if(!resolve_include_path(arg[0].value, cmd.file, lib_paths, path))
				return make_error("include: cannot find file '" + arg[0].value + "'",
				                  cmd.file, cmd.line);
			parse_result_t sub = parse_file_rec(path, open_files, lib_paths, depth + 1);
			if(!sub.ok){
				parse_result_t err = sub;
				err.error = sub.error + " (included from " + cmd.file + " line " +
				            to_string(cmd.line) + ")";
				return err;
			}
			for(const command_t& sub_cmd : sub.code)
				result.push_back(sub_cmd);
		}
	}
	code = result;
	return parse_result_t();
}

static parse_result_t parse_code(const string& code, const string& file_path,
                                 vector<string>& open_files, const vector<string>& lib_paths,
                                 int depth){
	abas_file_t file;
	file.load(code, file_path);

	parse_result_t res = parser(lexer(file), file_path);
	if(!res.ok)
		return res;

	parse_result_t expanded = expand_includes(res.code, open_files, lib_paths, depth);
	if(!expanded.ok)
		return expanded;

	reindex(res.code);
	return res;
}

parse_result_t parse_source(const string& code, const string& file_path){
	vector<string> open_files;
	return parse_code(code, file_path, open_files, {}, 0);
}

parse_result_t parse_file(const string& file_path, const vector<string>& lib_paths){
	vector<string> open_files;
	return parse_file_rec(file_path, open_files, lib_paths, 0);
}

string command_to_string(const command_t& cmd){
	string res = cmd.command;
	for(const vector<TOKEN>& arg : cmd.args){
		res += " ";
		if(arg.empty()){
			res += "<missing>";
			continue;
		}
		for(size_t i = 0 ; i < arg.size() ; i++){
			if(i)
				res += " ";
			if(arg[i].type == STRING)
				res += "\"" + parse_string(arg[i].value) + "\"";
			else
				res += arg[i].value;
		}
	}
	return res;
}
