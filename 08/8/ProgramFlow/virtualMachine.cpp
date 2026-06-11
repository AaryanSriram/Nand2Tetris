#include <bits/stdc++.h>
#include <fstream>
#include <filesystem>
using namespace std;

string trim(const string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    if (start == string::npos)
        return "";
    return s.substr(start, end - start + 1);
}

string remove_comments(const string &s) {
    int comment_index = s.find("//");
    if (comment_index == string::npos)
        return s;
    return s.substr(0, comment_index);
}

string arithmetic_basic(string &cmd) {
    unordered_map<string, string> operations = {
        {"add", "@SP\nAM=M-1\nD=M\n@SP\nAM=M-1\nM=M+D\n@SP\nM=M+1\n"},
        {"sub", "@SP\nAM=M-1\nD=M\n@SP\nAM=M-1\nM=M-D\n@SP\nM=M+1\n"},
        {"and", "@SP\nAM=M-1\nD=M\n@SP\nAM=M-1\nM=M&D\n@SP\nM=M+1\n"},
        {"or",  "@SP\nAM=M-1\nD=M\n@SP\nAM=M-1\nM=M|D\n@SP\nM=M+1\n"},
        {"neg", "@SP\nAM=M-1\nM=-M\n@SP\nM=M+1\n"},
        {"not", "@SP\nAM=M-1\nM=!M\n@SP\nM=M+1\n"}
    };
    return operations[cmd];
}

string arithmetic_compare(string &cmd, int label_counter) {
    string jump_type;
    if (cmd == "eq") jump_type = "JEQ";
    else if (cmd == "gt") jump_type = "JGT";
    else jump_type = "JLT";

    string true_label = cmd + "TRUE" + to_string(label_counter);
    string end_label = cmd + "END" + to_string(label_counter);

    string result =
        "@SP\nAM=M-1\nD=M\n"
        "@SP\nAM=M-1\nD=M-D\n"
        "@" + true_label + "\nD;" + jump_type + "\n"
        "D=0\n@" + end_label + "\n0;JMP\n"
        "(" + true_label + ")\nD=-1\n"
        "(" + end_label + ")\n"
        "@SP\nA=M\nM=D\n@SP\nM=M+1\n";

    return result;
}

string push_command(string &segment, string &index, string &filename) {
    unordered_map<string, string> segment_map = {
        {"this", "THIS"}, {"that", "THAT"}, {"local", "LCL"}, {"argument", "ARG"}
    };

    string result;
    if (segment == "constant") {
        result = "@" + index + "\nD=A\n@SP\nA=M\nM=D\n@SP\nM=M+1\n";
    } 
    else if (segment == "static") {
        result = "@" + filename + "." + index + "\nD=M\n@SP\nA=M\nM=D\n@SP\nM=M+1\n";
    } 
    else if (segment == "temp") {
        result = "@" + to_string(5 + stoi(index)) + "\nD=M\n@SP\nA=M\nM=D\n@SP\nM=M+1\n";
    } 
    else if (segment == "pointer") {
        result = (index == "0") ?
            "@THIS\nD=M\n@SP\nA=M\nM=D\n@SP\nM=M+1\n" :
            "@THAT\nD=M\n@SP\nA=M\nM=D\n@SP\nM=M+1\n";
    } 
    else {
        result =
            "@" + index + "\nD=A\n@" + segment_map[segment] + "\n"
            "D=M+D\nA=D\nD=M\n@SP\nA=M\nM=D\n@SP\nM=M+1\n";
    }
    return result;
}

string pop_command(string &segment, string &index, string &filename) {
    unordered_map<string, string> segment_map = {
        {"this", "THIS"}, {"that", "THAT"}, {"local", "LCL"}, {"argument", "ARG"}
    };

    string result;
    if (segment == "static") {
        result = "@SP\nAM=M-1\nD=M\n@" + filename + "." + index + "\nM=D\n";
    } 
    else if (segment == "temp") {
        result = "@SP\nAM=M-1\nD=M\n@" + to_string(5 + stoi(index)) + "\nM=D\n";
    } 
    else if (segment == "pointer") {
        result = (index == "0") ?
            "@SP\nAM=M-1\nD=M\n@THIS\nM=D\n" :
            "@SP\nAM=M-1\nD=M\n@THAT\nM=D\n";
    } 
    else {
        result =
            "@" + index + "\nD=A\n@" + segment_map[segment] + "\nD=M+D\n"
            "@R13\nM=D\n@SP\nAM=M-1\nD=M\n@R13\nA=M\nM=D\n";
    }
    return result;
}

string program_flow(const string &cmd, const string &func_name, const string &label) {
    string full_label = func_name + "." + label;
    if (cmd == "label") 
        return "(" + full_label + ")\n";
    else if (cmd == "goto") 
        return "@" + full_label + "\n0;JMP\n";
    else 
        return "@SP\nAM=M-1\nD=M\n@" + full_label + "\nD;JNE\n";
}

string function_command(const string &cmd, const string &func_name, const string &label, const string &index, int call_counter) {
    string result;
    string return_label = label + ".ret" + to_string(call_counter);

    if (cmd == "call") {
        string call_label = label;
        result +=
            "@" + return_label + "\nD=A\n@SP\nA=M\nM=D\n@SP\nM=M+1\n"
            "@LCL\nD=M\n@SP\nA=M\nM=D\n@SP\nM=M+1\n"
            "@ARG\nD=M\n@SP\nA=M\nM=D\n@SP\nM=M+1\n"
            "@THIS\nD=M\n@SP\nA=M\nM=D\n@SP\nM=M+1\n"
            "@THAT\nD=M\n@SP\nA=M\nM=D\n@SP\nM=M+1\n"
            "@SP\nD=M\n@" + to_string(5 + stoi(index)) + "\nD=D-A\n@ARG\nM=D\n"
            "@SP\nD=M\n@LCL\nM=D\n"
            "@" + call_label + "\n0;JMP\n"
            "(" + return_label + ")\n";
    } 
    else if (cmd == "function") {
        result = "(" + func_name + ")\n";
        int local_vars = stoi(index);
        for (int i = 0; i < local_vars; ++i) {
            result += "@0\nD=A\n@SP\nA=M\nM=D\n@SP\nM=M+1\n";
        }
    }
    return result;
}

string function_return() {
    string result;
    result +=
        "@LCL\nD=M\n@R13\nM=D\n"
        "@5\nA=D-A\nD=M\n@R14\nM=D\n"
        "@SP\nAM=M-1\nD=M\n@ARG\nA=M\nM=D\n"
        "@ARG\nD=M+1\n@SP\nM=D\n"
        "@R13\nAM=M-1\nD=M\n@THAT\nM=D\n"
        "@R13\nAM=M-1\nD=M\n@THIS\nM=D\n"
        "@R13\nAM=M-1\nD=M\n@ARG\nM=D\n"
        "@R13\nAM=M-1\nD=M\n@LCL\nM=D\n"
        "@R14\nA=M\n0;JMP\n";
    return result;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        cout << "Insufficient arguments" << endl;
        return 1;
    }

    namespace fs = std::filesystem;
    fs::path input_path = argv[1];
    vector<fs::path> vm_files;

    if (fs::is_directory(input_path)) {
        for (const auto &entry : fs::directory_iterator(input_path)) {
            if (entry.path().extension() == ".vm")
                vm_files.push_back(entry.path());
        }
    } 
    else if (fs::is_regular_file(input_path)) {
        if (input_path.extension() == ".vm")
            vm_files.push_back(input_path);
        else {
            cout << "Input file is not a .vm file" << endl;
            return 1;
        }
    } 
    else {
        cout << "Invalid input path" << endl;
        return 1;
    }

    string output_file;
    if (fs::is_directory(input_path))
        output_file = input_path.filename().string() + ".asm";
    else
        output_file = input_path.replace_extension(".asm").filename().string();

    ofstream fout(output_file);
    int label_counter = 0, call_counter = 0;
    bool has_sys_init = false;

    for (const auto &file : vm_files) {
        if (file.filename() == "Sys.vm") {
            has_sys_init = true;
            break;
        }
    }

    if (has_sys_init) {
        fout << "@256\nD=A\n@SP\nM=D\n";
        string sys_call = function_command("call", "Sys.init", "Sys.init", "0", call_counter++);
        fout << sys_call;
    }

    for (auto &file : vm_files) {
        ifstream fin(file);
        if (!fin.is_open()) {
            cout << "Error opening input file" << endl;
            return 1;
        }

        string file_path = file.string();
        int slash_pos = file_path.find('/');
        string file_name = file_path.substr(slash_pos + 1);
        int dot_pos = file_name.find('.');

        string current_func = "", line;
        while (getline(fin, line)) {
            line = remove_comments(line);
            line = trim(line);
            if (line.empty()) continue;

            int space_pos = line.find(' ');
            string command = line.substr(0, space_pos);
            string result;

            if (space_pos == string::npos && command != "return") {
                if (command != "eq" && command != "gt" && command != "lt")
                    result = arithmetic_basic(command);
                else
                    result = arithmetic_compare(command, label_counter++);
            } 
            else {
                string rest = line.substr(space_pos + 1);
                string base_name = file_name.substr(0, dot_pos);

                if (command == "push" || command == "pop") {
                    int space2 = rest.find(' ');
                    string segment = rest.substr(0, space2);
                    string index = rest.substr(space2 + 1);
                    result = (command == "push")
                                 ? push_command(segment, index, base_name)
                                 : pop_command(segment, index, base_name);
                } 
                else if (command == "label" || command == "goto" || command == "if-goto") {
                    result = program_flow(command, current_func, rest);
                } 
                else if (command == "function" || command == "call") {
                    int space2 = rest.find(' ');
                    string label = rest.substr(0, space2);
                    string index = rest.substr(space2 + 1);
                    if (command == "call") call_counter++;
                    if (command == "function") current_func = label;
                    result = function_command(command, current_func, label, index, call_counter);
                } 
                else if (command == "return") {
                    result = function_return();
                }
            }

            fout << result;
        }
        fin.close();
    }

    fout << "(END)\n@END\n0;JMP\n";
    fout.close();
}