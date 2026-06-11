#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

using namespace std;

// Helper to trim whitespace (spaces, tabs, CR, LF)
string trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    return (start == string::npos) ? "" : s.substr(start, end - start + 1);
}

const int C_ARITHMETIC = 0;
const int C_PUSH = 1;
const int C_POP = 2;

int commandType(const string& cmd) {
    if (cmd == "push") return C_PUSH;
    if (cmd == "pop") return C_POP;
    return C_ARITHMETIC;
}

bool isArithmetic(const string& op) {
    return op == "add" || op == "sub" || op == "neg" ||
           op == "eq" || op == "gt" || op == "lt" ||
           op == "and" || op == "or" || op == "not";
}

// Write assembly for arithmetic commands (valid Hack assembly)
string writeArithmetic(const string& cmd) {
    static int labelNum = 0;
    ostringstream out;

    if (cmd == "add" || cmd == "sub" || cmd == "and" || cmd == "or") {
        // Pop y into D, x is at SP-1 in M
        out << "@SP\n"
            << "AM=M-1\n"   // SP--; A = SP
            << "D=M\n"      // D = y
            << "A=A-1\n"    // A = SP-1 (x)
            ;
        if (cmd == "add") out << "M=M+D\n";
        else if (cmd == "sub") out << "M=M-D\n";
        else if (cmd == "and") out << "M=M&D\n";
        else out << "M=M|D\n";
    } else if (cmd == "neg") {
        out << "@SP\n"
            << "A=M-1\n"
            << "M=-M\n";
    } else if (cmd == "not") {
        out << "@SP\n"
            << "A=M-1\n"
            << "M=!M\n";
    } else if (cmd == "eq" || cmd == "gt" || cmd == "lt") {
        string trueLabel = "TRUE_" + to_string(labelNum);
        string endLabel  = "END_"   + to_string(labelNum++);
        // Compute x - y in D (where y is top)
        out << "@SP\n"
            << "AM=M-1\n"   // SP--; A=SP
            << "D=M\n"      // D = y
            << "A=A-1\n"    // A = SP-1 (x)
            << "D=M-D\n"    // D = x - y
            ;
        if (cmd == "eq") out << "@" << trueLabel << "\nD;JEQ\n";
        else if (cmd == "gt") out << "@" << trueLabel << "\nD;JGT\n";
        else /*lt*/      out << "@" << trueLabel << "\nD;JLT\n";

        // false case: set top to 0
        out << "@SP\n"
            << "A=M-1\n"
            << "M=0\n"
            << "@" << endLabel << "\n"
            << "0;JMP\n"
            // true case: set top to -1
            << "(" << trueLabel << ")\n"
            << "@SP\n"
            << "A=M-1\n"
            << "M=-1\n"
            << "(" << endLabel << ")\n";
    }

    return out.str();
}

string segmentBase(const string& segment) {
    if(segment == "local")    return "LCL";
    if(segment == "argument") return "ARG";
    if(segment == "this")     return "THIS";
    if(segment == "that")     return "THAT";
    return "";
}

// Fully-correct push/pop assembly generator (no A=M+D)
string writePushPop(int type, const string& segment, int index) {
    ostringstream out;

    if (type == C_PUSH) {
        if (segment == "constant") {
            out << "@" << index << "\n"
                << "D=A\n"
                << "@SP\n"
                << "A=M\n"
                << "M=D\n"
                << "@SP\n"
                << "M=M+1\n";
        } else if (segment == "static") {
            out << "@static." << index << "\n"
                << "D=M\n"
                << "@SP\n"
                << "A=M\n"
                << "M=D\n"
                << "@SP\n"
                << "M=M+1\n";
        } else if (segment == "temp") {
            out << "@" << (5 + index) << "\n"
                << "D=M\n"
                << "@SP\n"
                << "A=M\n"
                << "M=D\n"
                << "@SP\n"
                << "M=M+1\n";
        } else if (segment == "pointer") {
            out << ((index == 0) ? "@THIS\n" : "@THAT\n")
                << "D=M\n"
                << "@SP\n"
                << "A=M\n"
                << "M=D\n"
                << "@SP\n"
                << "M=M+1\n";
        } else {
            // local, argument, this, that
            // Compute effective address into D: D = base + index
            out << "@" << index << "\n"
                << "D=A\n"
                << "@" << segmentBase(segment) << "\n"
                << "D=M+D\n"   // D = base + index (address)
                << "A=D\n"
                << "D=M\n"     // D = *address
                << "@SP\n"
                << "A=M\n"
                << "M=D\n"
                << "@SP\n"
                << "M=M+1\n";
        }
    } else { // C_POP
        if (segment == "static") {
            out << "@SP\n"
                << "AM=M-1\n"
                << "D=M\n"
                << "@static." << index << "\n"
                << "M=D\n";
        } else if (segment == "temp") {
            out << "@SP\n"
                << "AM=M-1\n"
                << "D=M\n"
                << "@" << (5 + index) << "\n"
                << "M=D\n";
        } else if (segment == "pointer") {
            out << "@SP\n"
                << "AM=M-1\n"
                << "D=M\n"
                << ((index == 0) ? "@THIS\n" : "@THAT\n")
                << "M=D\n";
        } else {
            // local, argument, this, that
            // Compute effective address and store it in R13, then pop into *R13
            out << "@" << index << "\n"
                << "D=A\n"
                << "@" << segmentBase(segment) << "\n"
                << "D=M+D\n"   // D = base + index (address)
                << "@R13\n"
                << "M=D\n"     // R13 = target address
                << "@SP\n"
                << "AM=M-1\n"
                << "D=M\n"     // D = popped value
                << "@R13\n"
                << "A=M\n"
                << "M=D\n";
        }
    }

    return out.str();
}

string asmFileName(const string& vmFileName) {
    size_t pos = vmFileName.rfind(".vm");
    if (pos != string::npos && pos == vmFileName.length() - 3) {
        return vmFileName.substr(0, pos) + ".asm";
    } else {
        return vmFileName + ".asm";
    }
}

int main(int argc, char* argv[]) {
    if(argc != 2) {
        cerr << "Usage: " << argv[0] << " input.vm\n";
        return 1;
    }

    string inputFileName = argv[1];
    string outputFileName = asmFileName(inputFileName);

    ifstream infile(inputFileName);
    ofstream outfile(outputFileName);
    if(!infile.is_open()) {
        cerr << "Error opening input file " << inputFileName << "\n";
        return 1;
    }
    if(!outfile.is_open()) {
        cerr << "Error opening output file " << outputFileName << "\n";
        return 1;
    }

    string line;
    while(getline(infile, line)) {
        // Remove inline comments
        size_t commentPos = line.find("//");
        if (commentPos != string::npos) line = line.substr(0, commentPos);

        line = trim(line);
        if(line.empty()) continue;

        istringstream iss(line);
        string cmd;
        iss >> cmd;

        if(cmd.empty()) continue;

        if(isArithmetic(cmd)) {
            outfile << writeArithmetic(cmd);
        } else if(cmd == "push" || cmd == "pop") {
            string arg1, arg2;
            if (!(iss >> arg1 >> arg2)) {
                cerr << "Malformed " << cmd << " command in input: '" << line << "'\n";
                continue;
            }
            // validate arg2 is numeric (non-negative integer)
            bool ok = !arg2.empty() && (all_of(arg2.begin(), arg2.end(), ::isdigit));
            if (!ok) {
                cerr << "Invalid index in command: '" << line << "'\n";
                continue;
            }
            int idx = stoi(arg2);
            outfile << writePushPop(commandType(cmd), arg1, idx);
        } else {
            cerr << "Unknown command: '" << cmd << "' (line: " << line << ")\n";
        }
    }

    // End infinite loop
    outfile << "(END)\n@END\n0;JMP\n";

    infile.close();
    outfile.close();

    cout << "Created " << outputFileName << " from " << inputFileName << endl;
    return 0;
}
