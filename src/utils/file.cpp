#include <utils/file.h>

string filetostr(string file_path){
	string s = "";
	ifstream file(file_path);
	if(!file.is_open()){
		return "";
	}
	string line = "";
	while(getline(file, line)){
		s += line + "\n";
	}
	file.close();
	return s;
}

string filedir(string file_path){
	size_t pos = file_path.find_last_of('/');
	if(pos == string::npos)
		return "";
	return file_path.substr(0, pos + 1);
}
