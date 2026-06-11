//assembler.cpp
#include <bits/stdc++.h>
using namespace std;

// Trim whitespace
static inline string trim(const string &s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// Remove comments and whitespace
string stripLine(const string &raw) {
    string s = raw;
    auto pos = s.find("//");
    if (pos != string::npos) s = s.substr(0, pos);
    return trim(s);
}

// Convert int to binary string of fixed width
string toBinary(int value, int bits) {
    string out(bits, '0');
    for (int i = bits - 1; i >= 0; --i) {
        out[i] = (value & 1) ? '1' : '0';
        value >>= 1;
    }
    return out;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " file.asm\n";
        return 1;
    }

    string inPath = argv[1];
    // Change extension .asm → .hack
    string outPath = inPath.substr(0, inPath.find_last_of('.')) + ".hack";

    ifstream fin(inPath);
    if (!fin) {
        cerr << "Cannot open input file: " << inPath << "\n";
        return 2;
    }
    ofstream fout(outPath);
    if (!fout) {
        cerr << "Cannot open output file: " << outPath << "\n";
        return 3;
    }

    // Predefined symbols
    unordered_map<string,int> sym;
    for (int i = 0; i <= 15; ++i) sym["R" + to_string(i)] = i;
    sym["SP"]=0; sym["LCL"]=1; sym["ARG"]=2; sym["THIS"]=3; sym["THAT"]=4;
    sym["SCREEN"]=16384; sym["KBD"]=24576;

    // comp, dest, jump tables
    unordered_map<string,string> comp = {
        {"0","0101010"},{"1","0111111"},{"-1","0111010"},{"D","0001100"},{"A","0110000"},
        {"!D","0001101"},{"!A","0110001"},{"-D","0001111"},{"-A","0110011"},{"D+1","0011111"},
        {"A+1","0110111"},{"D-1","0001110"},{"A-1","0110010"},{"D+A","0000010"},{"D-A","0010011"},
        {"A-D","0000111"},{"D&A","0000000"},{"D|A","0010101"},{"M","1110000"},{"!M","1110001"},
        {"-M","1110011"},{"M+1","1110111"},{"M-1","1110010"},{"D+M","1000010"},{"D-M","1010011"},
        {"M-D","1000111"},{"D&M","1000000"},{"D|M","1010101"}
    };
    unordered_map<string,string> dest = {
        {"","000"},{"M","001"},{"D","010"},{"MD","011"},{"A","100"},{"AM","101"},{"AD","110"},{"AMD","111"}
    };
    unordered_map<string,string> jump = {
        {"","000"},{"JGT","001"},{"JEQ","010"},{"JGE","011"},{"JLT","100"},{"JNE","101"},{"JLE","110"},{"JMP","111"}
    };

    // Read all lines
    vector<string> lines;
    string raw;
    while (getline(fin, raw)) {
        string s = stripLine(raw);
        if (!s.empty()) lines.push_back(s);
    }
    fin.close();

    // Pass 1: collect labels
    int romAddr = 0;
    for (auto &line : lines) {
        if (line.front() == '(' && line.back() == ')') {
            string label = line.substr(1, line.size()-2);
            sym[label] = romAddr;
        } else {
            ++romAddr;
        }
    }

    // Pass 2: translate and write to .hack file
    int nextVar = 16;
    for (auto &line : lines) {
        if (line.front() == '(' && line.back() == ')') continue; // skip label

        if (line.front() == '@') {
            // A-instruction
            string symbol = line.substr(1);
            int value = 0;
            bool isNumber = !symbol.empty() && isdigit(symbol[0]);
            if (isNumber) {
                value = stoi(symbol);
            } else {
                if (sym.find(symbol) == sym.end()) sym[symbol] = nextVar++;
                value = sym[symbol];
            }
            fout << "0" + toBinary(value, 15) << "\n";
        } else {
            // C-instruction
            string destPart="", compPart="", jumpPart="";
            auto eqpos = line.find('=');
            auto sempos = line.find(';');

            if (eqpos != string::npos) {
                destPart = trim(line.substr(0, eqpos));
                compPart = (sempos != string::npos) ?
                    trim(line.substr(eqpos+1, sempos-eqpos-1)) : trim(line.substr(eqpos+1));
            } else {
                compPart = (sempos != string::npos) ? trim(line.substr(0, sempos)) : trim(line);
            }
            if (sempos != string::npos) jumpPart = trim(line.substr(sempos+1));

            string compBits = comp[compPart];
            string destBits = dest[destPart];
            string jumpBits = jump[jumpPart];

            fout << "111" + compBits + destBits + jumpBits << "\n";
        }
    }

    fout.close();
    cout << "Assembled " << inPath << " -> " << outPath << "\n";
    return 0;
}

