#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <core/interp.h>
#include <core/lexer.h>
#include <core/parser.h>
using namespace std;

static void usage(const char* prog){
	cout << "abas, a little stack language\n\n";
	cout << "usage: " << prog << " [options] <file.abas> [file2.abas ...]\n\n";
	cout << "  -t     only print the tokens of every file\n";
	cout << "  -p     only print the commands of every file\n";
	cout << "  -c     only check every file, do not run it\n";
	cout << "  -s N   stop a program after N commands, no limit without it\n";
	cout << "  -d N   stop a program that calls itself N deep, no limit without it\n";
	cout << "  -I D   look for included files in D, may be given more than once\n";
	cout << "  -v     print how many commands ran\n";
	cout << "  -l     list every command abas knows\n";
	cout << "  -h     this text\n";
	cout << "\n";
	cout << "  -s may also be written --step-limit N\n";
	cout << "  -d may also be written --function-depth N\n";
}

static void print_vm_error(){
	cerr << vm.file;
	if(vm.error_line > 0)
		cerr << ":" << vm.error_line;
	cerr << ": error: " << vm.error << endl;
}

static void print_tokens(const string& path){
	abas_file_t file;
	if(!file.open(path)){
		cerr << path << ": error: cannot open file" << endl;
		return;
	}
	vector<TOKEN> tokens = lexer(file);
	for(const TOKEN& token : tokens)
		printf("LINE %ld: TOKEN TYPE: %16s | TOKEN VALUE: %s\n", (long)token.line,
		       tokentype_name(token.type).c_str(), parse_string(token.value).c_str());
}

static int run_file(const string& path, const vector<string>& lib_paths, bool check_only,
                    bool verbose){
	parse_result_t parsed = parse_file(path, lib_paths);
	if(!parsed.ok){
		cerr << parsed.file;
		if(parsed.line > 0)
			cerr << ":" << parsed.line;
		cerr << ": error: " << parsed.error << endl;
		return 1;
	}
	if(abas_check(parsed.code, path) != 0){
		print_vm_error();
		return 1;
	}
	if(check_only){
		cout << path << ": ok, " << parsed.code.size() << " command(s)" << endl;
		return 0;
	}

	int code = abas_run(parsed.code, path);
	if(vm.failed){
		print_vm_error();
		return 1;
	}
	if(verbose)
		cout << path << ": " << vm.steps << " command(s) ran" << endl;
	return code;
}

int main(int argc, const char** argv){
	bool dump_tokens = false;
	bool dump_parsed = false;
	bool check_only = false;
	bool verbose = false;
	vector<string> files;
	vector<string> lib_paths;
	value_t max_steps = 0;
	value_t max_call_depth = 0;

	for(int i = 1 ; i < argc ; i++){
		string arg = argv[i];
		if(arg == "-h" || arg == "--help"){
			usage(argv[0]);
			return 0;
		}
		if(arg == "-l" || arg == "--list"){
			for(const auto& entry : functions)
				cout << entry.first << endl;
			return 0;
		}
		if(arg == "-t"){ dump_tokens = true; continue; }
		if(arg == "-p"){ dump_parsed = true; continue; }
		if(arg == "-c"){ check_only = true; continue; }
		if(arg == "-v"){ verbose = true; continue; }
		if(arg == "-I"){
			if(i + 1 >= argc){
				cerr << "error: -I needs a directory" << endl;
				return 1;
			}
			lib_paths.push_back(argv[++i]);
			continue;
		}
		if(arg == "-s" || arg == "--step-limit"){
			if(i + 1 >= argc){
				cerr << "error: " << arg << " needs a number" << endl;
				return 1;
			}
			max_steps = (value_t)stoll(argv[++i]);
			continue;
		}
		if(arg == "-d" || arg == "--function-depth"){
			if(i + 1 >= argc){
				cerr << "error: " << arg << " needs a number" << endl;
				return 1;
			}
			max_call_depth = (value_t)stoll(argv[++i]);
			continue;
		}
		if(!arg.empty() && arg[0] == '-'){
			cerr << "error: unknown option " << arg << endl;
			usage(argv[0]);
			return 1;
		}
		files.push_back(arg);
	}

	if(files.empty()){
		usage(argv[0]);
		return 0;
	}

	vm.max_steps = max_steps;
	vm.max_call_depth = max_call_depth;
	int last = 0;
	for(const string& path : files){
		if(!filesystem::exists(path)){
			cerr << path << ": error: file not found" << endl;
			last = 1;
			continue;
		}
		if(dump_tokens){
			print_tokens(path);
			continue;
		}
		if(dump_parsed){
			parse_result_t parsed = parse_file(path, lib_paths);
			if(!parsed.ok){
				cerr << parsed.file;
				if(parsed.line > 0)
					cerr << ":" << parsed.line;
				cerr << ": error: " << parsed.error << endl;
				last = 1;
				continue;
			}
			for(const command_t& cmd : parsed.code)
				cout << "[" << cmd.index << "] " << parsed.file << ":" << cmd.line << ": "
				     << command_to_string(cmd) << endl;
			continue;
		}
		last = run_file(path, lib_paths, check_only, verbose);
	}
	return last;
}
