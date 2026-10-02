#include <core/lexer.h>
#include <utils/file.h>

map<TOKENTYPE, string> tokentype_names = {
	{ UNKNOWN_TYPE, "UNKNOWN_TYPE" },
	{ ERROR, "ERROR" },

	{ IDENTIFIER, "IDENTIFIER" },
	{ ENDLINE, "ENDLINE" },
	{ WHITESPACE, "WHITESPACE" },
	{ DELIMITERS, "DELIMITERS" },

	{ COMMENT, "COMMENT" },
	{ COMMENT_ONELINE, "COMMENT_ONELINE" },
	{ COMMENT_MULLINE, "COMMENT_MULLINE" },

	{ INT, "INT" },
	{ STRING, "STRING" },
	{ CHAR, "CHAR" },

	{ OPERATOR, "OPERATOR" },
};

map<char, TOKENTYPE> token_char = {
	{';', ENDLINE },
	{'\n', ENDLINE },

	{' ', WHITESPACE },
	{'\t', WHITESPACE },
	{'\v', WHITESPACE },
	{'\r', WHITESPACE },
	{'\f', WHITESPACE },

	{',', DELIMITERS },
	{':', DELIMITERS },
	{'.', DELIMITERS },
	{'(', DELIMITERS },
	{')', DELIMITERS },

	{'#', COMMENT },

	{'0', INT },
	{'1', INT },
	{'2', INT },
	{'3', INT },
	{'4', INT },
	{'5', INT },
	{'6', INT },
	{'7', INT },
	{'8', INT },
	{'9', INT },

	{'\"', STRING },
	{'\'', CHAR },

	{'$', OPERATOR }, // de pointer
	{'+', OPERATOR },
	{'-', OPERATOR },
	{'*', OPERATOR },
	{'/', OPERATOR },
	{'%', OPERATOR },

	{'&', OPERATOR },
	{'|', OPERATOR },
	{'^', OPERATOR },
	{'<', OPERATOR },
	{'>', OPERATOR },
	{'=', OPERATOR },
};

TOKENTYPE chartype(char c){
	auto it = token_char.find(c);
	if(it == token_char.end())
		return UNKNOWN_TYPE;
	return it->second;
}

bool abas_file_t::open(string file_path){
	ifstream file(file_path);
	if(!file.is_open())
		return false;
	file.close();
	load(filetostr(file_path), file_path);
	return true;
}

string tokentype_name(TOKENTYPE type){
	auto it = tokentype_names.find(type);
	if(it == tokentype_names.end())
		return "UNKNOWN_TYPE";
	return it->second;
}

static bool is_ident_char(int c){
	return c != EOF && (isalnum(c) != 0 || c == '_');
}

static int unescape_char(int c){
	switch(c){
		case 'n': return '\n';
		case 't': return '\t';
		case 'v': return '\v';
		case 'a': return '\a';
		case 'b': return '\b';
		case 'f': return '\f';
		case 'r': return '\r';
		case '0': return '\0';
		case '\\': return '\\';
		case '\'': return '\'';
		case '\"': return '\"';
		default: return c;
	}
}

static void append_escaped(string& out, int c){
	out += (char)unescape_char(c);
}

vector<TOKEN> lexer(abas_file_t& file){
	vector<TOKEN> tokens;
	int c = 0;
	while(true){
		file.skip_ws();
		if(file.eof())
			break;

		TOKEN token;
		token.line = file.line;
		c = file.getc();
		if(c == EOF)
			break;
		token.value += (char)c;
		token.type = chartype((char)c);
		if(token.type == UNKNOWN_TYPE && (isalpha(c) != 0 || c == '_'))
			token.type = IDENTIFIER;

		switch(token.type){
			case IDENTIFIER:
				while(!file.eof()){
					c = file.getc();
					if(!is_ident_char(c)){
						file.ungetc();
						break;
					}
					token.value += (char)c;
				}
				break;
			case ENDLINE:
				// collapse every kind of endline into one token
				token.value = "\n";
				while(!file.eof()){
					file.skip_ws();
					c = file.getc();
					if(chartype((char)c) != ENDLINE){
						file.ungetc();
						break;
					}
				}
				break;
			case WHITESPACE:
				file.skip_ws();
				continue;
			case INT:
				while(!file.eof()){
					c = file.getc();
					if(c == EOF || isalnum(c) == 0){
						file.ungetc();
						break;
					}
					token.value += (char)c;
				}
				break;
			case CHAR: {
				token.value = "";
				int value = file.getc();
				if(value == EOF){
					token.type = ERROR;
					token.value = "unterminated character literal";
					break;
				}
				bool escaped = false;
				if(value == '\\'){
					escaped = true;
					value = file.getc();
					if(value == EOF){
						token.type = ERROR;
						token.value = "unterminated character literal";
						break;
					}
					value = unescape_char(value);
				}
				// a newline written straight into a character literal ends the line
				// and leaves the closing quote on the next one. an escaped one is a
				// character like any other, so '\n' holds the code of a line end
				if(!escaped && value == '\n'){
					token.type = ERROR;
					token.value = "a character literal cannot hold a newline";
					break;
				}
				int quote = file.getc();
				if(quote != '\''){
					if(quote != EOF)
						file.ungetc();
					token.type = ERROR;
					token.value = "a character literal must hold exactly one character";
					break;
				}
				token.value = string(1, (char)value);
				break;
			}
			case STRING: {
				token.value = "";
				bool closed = false;
				while(!file.eof()){
					c = file.getc();
					if(c == EOF)
						break;
					if(c == '\\'){
						c = file.getc();
						if(c == EOF)
							break;
						append_escaped(token.value, c);
						continue;
					}
					if(c == '\"'){
						closed = true;
						break;
					}
					token.value += (char)c;
				}
				if(!closed){
					token.type = ERROR;
					token.value = "unterminated string literal";
				}
				break;
			}
			case DELIMITERS:
				// one delimiter per token, so ',' can always be found
				break;
			case COMMENT: {
				int next = file.getc();
				if(next == '#'){
					token.type = COMMENT_MULLINE;
					bool closed = false;
					while(!file.eof()){
						c = file.getc();
						if(c == EOF)
							break;
						if(c == '#'){
							int again = file.getc();
							if(again == '#'){
								closed = true;
								break;
							}
							if(again != EOF)
								file.ungetc();
							continue;
						}
						token.value += (char)c;
					}
					if(!closed){
						token.type = ERROR;
						token.value = "unterminated multi line comment";
					}
				}
				else{
					token.type = COMMENT_ONELINE;
					if(next != EOF)
						file.ungetc();
					// the endline itself is not part of the comment
					while(!file.eof()){
						c = file.getc();
						if(c == EOF || c == '\n'){
							if(c == '\n')
								file.ungetc();
							break;
						}
						token.value += (char)c;
					}
				}
				break;
			}
			case OPERATOR:
				while(!file.eof()){
					c = file.getc();
					if(chartype((char)c) != OPERATOR){
						file.ungetc();
						break;
					}
					token.value += (char)c;
				}
				break;
			default:
				break;
		}
		tokens.push_back(token);
	}
	return tokens;
}

string parse_string(string s){
	string res = "";
	for(size_t i=0 ; i < s.size() ; i++){
		switch(s[i]){
			case '\n':
				res += "\\n";
				break;
			case '\t':
				res += "\\t";
				break;
			case '\v':
				res += "\\v";
				break;
			case '\a':
				res += "\\a";
				break;
			case '\b':
				res += "\\b";
				break;
			case '\f':
				res += "\\f";
				break;
			case '\r':
				res += "\\r";
				break;
			case '\0':
				res += "\\0";
				break;
			case '\\':
				res += "\\\\";
				break;
			default:
				res += s[i];
				break;
		}
	}
	return res;
}
