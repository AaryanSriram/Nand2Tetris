#include <bits/stdc++.h>
using namespace std;

enum TokenType { KEYWORD = 0, SYM = 1, NUM = 2, STR = 3, ID = 4, ERROR = 5 };
enum Kind { STATIC_K = 0, FIELD_K = 1, ARG_K = 2, VAR_K = 3, NONE_K = 4 };

const string SYMBOLS = "{}()[].,;+-*/&|<>=~";

const unordered_set<string> KEYWORDS = {
    "class", "method", "function", "constructor", "int", "boolean",
    "char", "void", "var", "static", "field", "let", "do", "if",
    "else", "while", "return", "true", "false", "null", "this"
};

const unordered_map<string,int> KWD_TO_KIND = { {"static", STATIC_K}, {"field", FIELD_K} };

const unordered_map<string,string> VM_CMDS = {
    {"+", "add"}, {"-", "sub"}, {"*", "call Math.multiply 2"}, {"/", "call Math.divide 2"},
    {"<", "lt"}, {">", "gt"}, {"=", "eq"}, {"&", "and"}, {"|", "or"}
};

const unordered_map<string,string> VM_UNARY_CMDS = { {"-", "neg"}, {"~", "not"} };

const unordered_map<int,string> SEGMENTS = {
    {STATIC_K, "static"},
    {FIELD_K, "this"},
    {ARG_K, "argument"},
    {VAR_K, "local"},
    {-1, "ERROR"}
};

const int TEMP_RETURN = 0;
const int TEMP_ARRAY = 1;

class Compiler {
private:
    ofstream _outfile;
    string _lines;
    vector<pair<int,string>> _tokens;
    int _token_type;
    string _cur_val;

    unordered_map<string, tuple<string,int,int>> class_symbols;
    unordered_map<string, tuple<string,int,int>> subroutine_symbols;
    unordered_map<int, unordered_map<string, tuple<string,int,int>>*> symbols;
    unordered_map<int,int> index_map;

    string _cur_class;
    string _cur_subroutine;
    int label_num;

    regex _comment_re;
    string _sym_re_str;
    string _num_re_str;
    string _str_re_str;
    string _id_re_str;
    regex _word_re;

    static string escape_for_char_class(const string &s) {
        string out;
        for(char c: s) {
            if (c == '\\' || c == ']' || c == '-' || c == '^')
                out.push_back('\\');
            out.push_back(c);
        }
        return out;
    }

    void openout(const string &path) {
        filesystem::path p(path);
        filesystem::path outdir = p.parent_path() / "output";
        filesystem::create_directories(outdir);
        string fname = p.filename().string();
        if (fname.size() >= 5 && fname.substr(fname.size()-5) == ".jack") {
            fname = fname.substr(0, fname.size()-5) + ".vm";
        } else {
            fname = fname + ".vm";
        }
        filesystem::path outfile = outdir / fname;
        _outfile.open(outfile.string());
    }

    void closeout() {
        if (_outfile.is_open()) _outfile.close();
    }

    void write_vm_cmd(const string &cmd, const string &arg1 = "", int arg2 = INT_MIN) {
        _outfile << cmd;
        if (!arg1.empty()) {
            _outfile << " " << arg1;
        }
        if (arg2 != INT_MIN) {
            _outfile << " " << arg2;
        }
        _outfile << "\n";
    }

    bool has_more_tokens() {
        return !_tokens.empty();
    }

    pair<int,string> advance_tok() {
        if (has_more_tokens()) {
            auto t = _tokens.back();
            _tokens.pop_back();
            _token_type = t.first;
            _cur_val = t.second;
            return t;
        } else {
            _token_type = ERROR;
            _cur_val.clear();
            return {ERROR, string()};
        }
    }

    pair<int,string> peek() {
        if (has_more_tokens()) {
            return _tokens.back();
        } else {
            return {ERROR, string()};
        }
    }

    vector<pair<int,string>> _tokenize(const string &lines) {
        string code = regex_replace(lines, _comment_re, string(""));
        vector<pair<int,string>> out;
        auto begin = sregex_iterator(code.begin(), code.end(), _word_re);
        auto end_it = sregex_iterator();
        for(auto it = begin; it != end_it; ++it) {
            string word = (*it).str();
            out.push_back(_token(word));
        }
        return out;
    }

    static bool _is_match(const string &re_str, const string &word) {
        try {
            regex r(re_str);
            return regex_match(word, r);
        } catch(...) {
            return false;
        }
    }

    pair<int,string> _token(const string &token_string) {
        if (KEYWORDS.find(token_string) != KEYWORDS.end()) {
            return {KEYWORD, token_string};
        } else if (regex_match(token_string, regex(_sym_re_str))) {
            return {SYM, token_string};
        } else if (regex_match(token_string, regex(_num_re_str))) {
            return {NUM, token_string};
        } else if (regex_match(token_string, regex(_str_re_str))) {
            string s = token_string.substr(1, token_string.size() - 2);
            return {STR, s};
        } else if (regex_match(token_string, regex(_id_re_str))) {
            return {ID, token_string};
        } else {
            return {ERROR, token_string};
        }
    }

    void start_subroutine() {
        subroutine_symbols.clear();
        index_map[ARG_K] = 0;
        index_map[VAR_K] = 0;
    }

    void define_symbol(const string &name, const string &var_type, int kind) {
        auto &m = *symbols[kind];
        m[name] = make_tuple(var_type, kind, index_map[kind]);
        index_map[kind] += 1;
    }

    int var_count(int kind) {
        int cnt = 0;
        for (auto &p : *symbols[kind]) {
            int k = get<1>(p.second);
            if (k == kind) cnt++;
        }
        return cnt;
    }

    string type_of(const string &name) {
        auto t = lookup(name);
        return get<0>(t);
    }

    int kind_of(const string &name) {
        auto t = lookup(name);
        return get<1>(t);
    }

    int index_of(const string &name) {
        auto t = lookup(name);
        return get<2>(t);
    }

    tuple<string,int,int> lookup(const string &name) {
        if (subroutine_symbols.find(name) != subroutine_symbols.end()) {
            return subroutine_symbols[name];
        } else if (class_symbols.find(name) != class_symbols.end()) {
            return class_symbols[name];
        } else {
            return make_tuple(string(), -1, -1);
        }
    }

    void vm_push_variable(const string &name) {
        auto t = lookup(name);
        string var_type = get<0>(t);
        int var_kind = get<1>(t);
        int var_index = get<2>(t);
        if (var_kind != -1) {
            auto it = SEGMENTS.find(var_kind);
            string seg = (it != SEGMENTS.end()) ? it->second : "ERROR";
            write_vm_cmd("push", seg, var_index);
        }
    }

    void vm_pop_variable(const string &name) {
        auto t = lookup(name);
        string var_type = get<0>(t);
        int var_kind = get<1>(t);
        int var_index = get<2>(t);
        if (var_kind != -1) {
            auto it = SEGMENTS.find(var_kind);
            string seg = (it != SEGMENTS.end()) ? it->second : "ERROR";
            write_vm_cmd("pop", seg, var_index);
        }
    }

    void compile_class() {
        advance_tok();
        _cur_class = advance_tok().second;
        advance_tok();

        auto pr = peek();
        while (pr.first == KEYWORD && (pr.second == "static" || pr.second == "field")) {
            compile_class_var_dec();
            pr = peek();
        }

        pr = peek();
        while (pr.first == KEYWORD && (pr.second == "constructor" || pr.second == "function" || pr.second == "method")) {
            compile_subroutine();
            pr = peek();
        }

        advance_tok();
    }

    void compile_class_var_dec() {
        auto t = advance_tok();
        int kind = KWD_TO_KIND.at(t.second);
        _compile_dec(kind);
    }

    void _compile_dec(int kind) {
        string var_type = compile_type();
        string name = advance_tok().second;
        define_symbol(name, var_type, kind);
        while (peek().first == SYM && peek().second == ",") {
            advance_tok();
            name = advance_tok().second;
            define_symbol(name, var_type, kind);
        }
        advance_tok();
    }

    string compile_void_or_type() {
        auto pr = peek();
        if (pr.first == KEYWORD && pr.second == "void") {
            return advance_tok().second;
        } else {
            return compile_type();
        }
    }

    string compile_type() {
        auto pr = peek();
        if (pr.first == ID || (pr.first == KEYWORD && (pr.second == "int" || pr.second == "char" || pr.second == "boolean"))) {
            return advance_tok().second;
        }
        return string();
    }

    void compile_subroutine() {
        auto t = advance_tok();
        string subroutine_type = t.second;
        string return_type = compile_void_or_type();
        _cur_subroutine = advance_tok().second;
        start_subroutine();
        if (subroutine_type == "method") {
            define_symbol("this", _cur_class, ARG_K);
        }
        advance_tok();
        compile_parameter_list();
        advance_tok();
        compile_subroutine_body(subroutine_type);
    }

    void compile_parameter_list() {
        auto pr = peek();
        if (pr.first == ID || (pr.first == KEYWORD && (pr.second == "int" || pr.second == "char" || pr.second == "boolean"))) {
            compile_parameter();
            while (peek().first == SYM && peek().second == ",") {
                advance_tok();
                compile_parameter();
            }
        }
    }

    void compile_parameter() {
        auto pr = peek();
        if (pr.first == ID || (pr.first == KEYWORD && (pr.second == "int" || pr.second == "char" || pr.second == "boolean"))) {
            string param_type = compile_type();
            string name = advance_tok().second;
            define_symbol(name, param_type, ARG_K);
        }
    }

    void compile_subroutine_body(const string &subroutine_type) {
        advance_tok();
        auto pr = peek();
        while (pr.first == KEYWORD && pr.second == "var") {
            compile_var_dec();
            pr = peek();
        }
        write_func_decl(subroutine_type);
        compile_statements();
        advance_tok();
    }

    void write_func_decl(const string &subroutine_type) {
        string func_name = _cur_class + "." + _cur_subroutine;
        write_vm_cmd("function", func_name, var_count(VAR_K));
        if (subroutine_type == "method") {
            write_vm_cmd("push", "argument", 0);
            write_vm_cmd("pop", "pointer", 0);
        } else if (subroutine_type == "constructor") {
            write_vm_cmd("push", "constant", var_count(FIELD_K));
            write_vm_cmd("call", "Memory.alloc", 1);
            write_vm_cmd("pop", "pointer", 0);
        }
    }

    void compile_var_dec() {
        advance_tok();
        _compile_dec(VAR_K);
    }

    void compile_statements() {
        auto pr = peek();
        while (pr.first == KEYWORD && (pr.second == "let" || pr.second == "if" || pr.second == "while" || pr.second == "do" || pr.second == "return")) {
            if (pr.second == "let") compile_let();
            else if (pr.second == "if") compile_if();
            else if (pr.second == "while") compile_while();
            else if (pr.second == "do") compile_do();
            else if (pr.second == "return") compile_return();
            pr = peek();
        }
    }

    void compile_let() {
        advance_tok();
        string name = advance_tok().second;
        bool is_array = (peek().first == SYM && peek().second == "[");
        if (is_array) {
            vm_push_variable(name);
            advance_tok();
            compile_expression();
            advance_tok();
            write_vm_cmd("add");
        }
        advance_tok();
        compile_expression();
        advance_tok();
        if (is_array) {
            write_vm_cmd("pop", "temp", TEMP_ARRAY);
            write_vm_cmd("pop", "pointer", 1);
            write_vm_cmd("push", "temp", TEMP_ARRAY);
            write_vm_cmd("pop", "that", 0);
        } else {
            vm_pop_variable(name);
        }
    }

    void compile_if() {
        advance_tok();
        label_num += 1;
        string notif_label = "label" + to_string(label_num);
        label_num += 1;
        string end_label = "label" + to_string(label_num);
        advance_tok();
        compile_expression();
        advance_tok();
        write_vm_cmd("not");
        write_vm_cmd("if-goto", notif_label);
        advance_tok();
        compile_statements();
        advance_tok();
        write_vm_cmd("goto", end_label);
        write_vm_cmd("label", notif_label);
        auto pr = peek();
        if (pr.first == KEYWORD && pr.second == "else") {
            advance_tok();
            advance_tok();
            compile_statements();
            advance_tok();
        }
        write_vm_cmd("label", end_label);
    }

    void compile_while() {
        advance_tok();
        label_num += 1;
        string top_label = "label" + to_string(label_num);
        label_num += 1;
        string notif_label = "label" + to_string(label_num);
        write_vm_cmd("label", top_label);
        advance_tok();
        compile_expression();
        advance_tok();
        write_vm_cmd("not");
        write_vm_cmd("if-goto", notif_label);
        advance_tok();
        compile_statements();
        advance_tok();
        write_vm_cmd("goto", top_label);
        write_vm_cmd("label", notif_label);
    }

    void compile_do() {
        advance_tok();
        string name = advance_tok().second;
        compile_subroutine_call(name);
        write_vm_cmd("pop", "temp", TEMP_RETURN);
        advance_tok();
    }

    void compile_return() {
        advance_tok();
        if (!(peek().first == SYM && peek().second == ";")) {
            compile_expression();
        } else {
            write_vm_cmd("push", "constant", 0);
        }
        advance_tok();
        write_vm_cmd("return");
    }

    void compile_expression() {
        compile_term();
        auto pr = peek();
        while (pr.first == SYM && string("+-*/&|<>=.").find(pr.second) != string::npos) {
            string op = advance_tok().second;
            compile_term();
            auto it = VM_CMDS.find(op);
            if (it != VM_CMDS.end()) {
                // some VM_CMDS entries contain spaces (call ...)
                string cmd = it->second;
                // if cmd contains spaces, write whole string as command and no additional args
                // but original logic in Python wrote cmd as single token possibly containing spaces;
                // replicate by splitting first word as cmd and passing rest as raw (but to preserve identical output, print cmd string as a whole line)
                _outfile << cmd << "\n";
            }
            pr = peek();
        }
    }

    void compile_term() {
        auto pr = peek();
        bool is_const = (pr.first == NUM || pr.first == STR || (pr.first == KEYWORD && (pr.second == "true" || pr.second == "false" || pr.second == "null" || pr.second == "this")));
        if (is_const) {
            compile_const();
        } else if (pr.first == SYM && pr.second == "(") {
            advance_tok();
            compile_expression();
            advance_tok();
        } else if (pr.first == SYM && (pr.second == "-" || pr.second == "~")) {
            string op = advance_tok().second;
            compile_term();
            auto it = VM_UNARY_CMDS.find(op);
            if (it != VM_UNARY_CMDS.end()) write_vm_cmd(it->second);
        } else if (pr.first == ID) {
            string name = advance_tok().second;
            auto pr2 = peek();
            if (pr2.first == SYM && pr2.second == "[") {
                compile_array_subscript(name);
            } else if (pr2.first == SYM && (pr2.second == "(" || pr2.second == ".")) {
                compile_subroutine_call(name);
            } else {
                vm_push_variable(name);
            }
        }
    }

    void compile_const() {
        auto t = advance_tok();
        int tok = t.first;
        string val = t.second;
        if (tok == NUM) {
            write_vm_cmd("push", "constant", stoi(val));
        } else if (tok == STR) {
            write_vm_cmd("push", "constant", (int)val.size());
            write_vm_cmd("call", "String.new", 1);
            for (char c : val) {
                write_vm_cmd("push", "constant", (int)(unsigned char)c);
                write_vm_cmd("call", "String.appendChar", 2);
            }
        } else if (tok == KEYWORD) {
            if (val == "this") {
                write_vm_cmd("push", "pointer", 0);
            } else if (val == "true") {
                write_vm_cmd("push", "constant", 1);
                write_vm_cmd("neg");
            } else {
                write_vm_cmd("push", "constant", 0);
            }
        }
    }

    void compile_array_subscript(const string &name) {
        vm_push_variable(name);
        advance_tok();
        compile_expression();
        advance_tok();
        write_vm_cmd("add");
        write_vm_cmd("pop", "pointer", 1);
        write_vm_cmd("push", "that", 0);
    }

    void compile_subroutine_call(const string &name_initial) {
        string name = name_initial;
        auto t = lookup(name);
        string var_type = get<0>(t);
        int var_kind = get<1>(t);
        int var_index = get<2>(t);
        int num_args = 0;
        if (peek().first == SYM && peek().second == ".") {
            advance_tok();
            string sub_name = advance_tok().second;
            if (var_type == "int" || var_type == "char" || var_type == "boolean" || var_type == "void") {
            } else if (var_type.empty()) {
                name = name + "." + sub_name;
            } else {
                num_args = 1;
                vm_push_variable(name);
                name = type_of(name) + "." + sub_name;
            }
        } else {
            num_args = 1;
            write_vm_cmd("push", "pointer", 0);
            name = _cur_class + "." + name;
        }
        advance_tok();
        num_args += compile_expr_list();
        advance_tok();
        write_vm_cmd("call", name, num_args);
    }

    int compile_expr_list() {
        int num_args = 0;
        auto pr = peek();
        bool is_const = (pr.first == NUM || pr.first == STR || (pr.first == KEYWORD && (pr.second == "true" || pr.second == "false" || pr.second == "null" || pr.second == "this")));
        bool is_term = is_const || pr.first == ID || (pr.first == SYM && (pr.second == "(" || pr.second == "-" || pr.second == "~"));
        if (is_term) {
            compile_expression();
            num_args = 1;
            while (peek().first == SYM && peek().second == ",") {
                advance_tok();
                compile_expression();
                num_args += 1;
            }
        }
        return num_args;
    }

public:
    Compiler(const string &filepath) :
        _token_type(ERROR),
        _comment_re(R"((//[^\n]*\n)|(/\*[\s\S]*?\*/))", regex::ECMAScript),
        _num_re_str(R"(\d+)"),
        _str_re_str(R"("[^"\n]*")"),
        _id_re_str(R"([a-zA-Z_]\w*)"),
        _word_re()
    {
        _sym_re_str = string("^") + "[" + escape_for_char_class(SYMBOLS) + "]" + string("$");
        openout(filepath);
        ifstream infile(filepath);
        string content((istreambuf_iterator<char>(infile)), istreambuf_iterator<char>());
        _lines = content;
        string id = _id_re_str;
        string sym = string("[") + escape_for_char_class(SYMBOLS) + "]";
        string num = _num_re_str;
        string str = _str_re_str;
        string word_pattern = id + "|" + sym + "|" + num + "|" + str;
        _word_re = regex(word_pattern, regex::ECMAScript);
        _tokens = _tokenize(_lines);
        reverse(_tokens.begin(), _tokens.end());
        _cur_val = string();
        class_symbols.clear();
        subroutine_symbols.clear();
        symbols[STATIC_K] = &class_symbols;
        symbols[FIELD_K] = &class_symbols;
        symbols[ARG_K] = &subroutine_symbols;
        symbols[VAR_K] = &subroutine_symbols;
        index_map[STATIC_K] = 0;
        index_map[FIELD_K] = 0;
        index_map[ARG_K] = 0;
        index_map[VAR_K] = 0;
        _cur_class.clear();
        _cur_subroutine.clear();
        label_num = 0;
        compile_class();
        closeout();
    }
};

int main(int argc, char** argv) {
    if (argc == 2) {
        string file_or_dir = argv[1];
        vector<string> infiles;
        if (file_or_dir.size() >= 5 && file_or_dir.substr(file_or_dir.size()-5) == ".jack") {
            infiles.push_back(file_or_dir);
        } else {
            for (auto &p : filesystem::directory_iterator(file_or_dir)) {
                if (!filesystem::is_regular_file(p.path())) continue;
                string s = p.path().string();
                if (s.size() >= 5 && s.substr(s.size()-5) == ".jack") infiles.push_back(s);
            }
        }
        if (!infiles.empty()) {
            for (auto &infile : infiles) {
                Compiler c(infile);
            }
        }
    }
    return 0;
}