#pragma once

#include <cctype>
#include <cstdint>
#include <fstream>
#include <map>
#include <string>
#include <vector>
using namespace std;

typedef int64_t value_t;

typedef enum{
	UNKNOWN_TYPE = 0,
	ERROR,

	IDENTIFIER,
	ENDLINE,
	WHITESPACE,
	DELIMITERS,

	COMMENT,
	COMMENT_ONELINE,
	COMMENT_MULLINE,

	INT,
	STRING,
	CHAR,

	OPERATOR,

} TOKENTYPE;

struct TOKEN{
	TOKENTYPE type = UNKNOWN_TYPE;
	value_t line = 0;
	string value = "";
};

extern map<TOKENTYPE, string> tokentype_names; // for debug
extern map<char, TOKENTYPE> token_char;
//extern const char* keywords[64];

// safe lookup, never inserts into the map (that was a source of bugs)
TOKENTYPE chartype(char c);
string tokentype_name(TOKENTYPE type);

struct abas_file_t{
	string code;
	string path;
	size_t index;
	size_t line;
	bool eof(){
		return index >= code.size();
	}
	// returns EOF (-1) at the end of the file, and keeps track of the line
	int getc(){
		if(eof())
			return EOF;
		char c = code[index++];
		if(c == '\n')
			line++;
		return (unsigned char)c;
	}
	void ungetc(){
		if(index == 0)
			return;
		index--;
		if(code[index] == '\n')
			line--;
	}
	int peekc(){
		if(eof())
			return EOF;
		return (unsigned char)code[index];
	}
	bool open(string file_path);
	void load(const string& src, string file_path = "<memory>"){
		code = src;
		path = file_path;
		index = 0;
		line = 1;
	}
	void skip_ws(){
		while(!eof()){
			char c = this->getc();
			if(chartype(c) != WHITESPACE){
				this->ungetc();
				break;
			}
		}
	}
};

vector<TOKEN> lexer(abas_file_t& file);
string parse_string(string s);

