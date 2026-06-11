#include <bits/stdc++.h>
using namespace std;
namespace fs = std::filesystem;

/* ---------- Tokenizer ---------- */
class JackTokenizer {
    vector<string> tokens;
    int i = -1;
public:
    JackTokenizer(istream &in) {
    string src((istreambuf_iterator<char>(in)), {});
    // remove comments (handles // and /* ... */)
    regex comment(R"(//.*|/\*[\s\S]*?\*/)");
    src = regex_replace(src, comment, "");
    // token pattern
    regex token(R"([{}()\[\].,;+\-*/&|<>=~]|\"[^\n\"]*\"|\d+|[A-Za-z_]\w*)");
    for (sregex_iterator it(src.begin(), src.end(), token); it != sregex_iterator(); ++it)
        tokens.push_back((*it)[0]);
    }
    bool hasMore() { return i + 1 < (int)tokens.size(); }
    void advance() { if (hasMore()) i++; }
    string token() { return tokens[i]; }
    string peek() { return (i + 1 < (int)tokens.size() ? tokens[i + 1] : ""); }
    string type() {
        string t = token();
        static unordered_set<string> keywords = {
            "class","constructor","function","method","field","static","var",
            "int","char","boolean","void","true","false","null","this",
            "let","do","if","else","while","return"
        };
        if (keywords.count(t)) return "KEYWORD";
        if (regex_match(t, regex("\\d+"))) return "INT";
        if (t.size()>=2 && t.front()=='\"') return "STRING";
        if (regex_match(t, regex("[A-Za-z_][A-Za-z0-9_]*"))) return "IDENT";
        return "SYMBOL";
    }
};

/* ---------- SymbolTable ---------- */
enum Kind { STATIC, FIELD, ARG, VAR, NONE };
struct SymbolInfo { string type; Kind kind; int index; };

class SymbolTable {
    unordered_map<string, SymbolInfo> classTbl, subTbl;
    int staticIdx=0, fieldIdx=0, argIdx=0, varIdx=0;
public:
    void startSubroutine() { subTbl.clear(); argIdx=varIdx=0; }
    void define(string name,string type,Kind kind) {
        SymbolInfo s{type,kind,indexOf(kind)};
        if (kind==STATIC||kind==FIELD) classTbl[name]=s;
        else subTbl[name]=s;
        inc(kind);
    }
    int varCount(Kind k){int c=0;for(auto&m:{classTbl,subTbl})for(auto &[n,s]:m)if(s.kind==k)c++;return c;}
    Kind kindOf(string n){if(subTbl.count(n))return subTbl[n].kind;if(classTbl.count(n))return classTbl[n].kind;return NONE;}
    string typeOf(string n){if(subTbl.count(n))return subTbl[n].type;if(classTbl.count(n))return classTbl[n].type;return "";}
    int indexOf(string n){if(subTbl.count(n))return subTbl[n].index;if(classTbl.count(n))return classTbl[n].index;return -1;}
private:
    int indexOf(Kind k){if(k==STATIC)return staticIdx;if(k==FIELD)return fieldIdx;if(k==ARG)return argIdx;if(k==VAR)return varIdx;return 0;}
    void inc(Kind k){if(k==STATIC)staticIdx++;if(k==FIELD)fieldIdx++;if(k==ARG)argIdx++;if(k==VAR)varIdx++;}
};

/* ---------- VM Writer ---------- */
class VMWriter {
    ostream &out;
public:
    VMWriter(ostream &o):out(o){}
    void writePush(string seg,int idx){out<<"push "<<seg<<" "<<idx<<"\n";}
    void writePop(string seg,int idx){out<<"pop "<<seg<<" "<<idx<<"\n";}
    void writeArithmetic(string cmd){out<<cmd<<"\n";}
    void writeLabel(string l){out<<"label "<<l<<"\n";}
    void writeGoto(string l){out<<"goto "<<l<<"\n";}
    void writeIf(string l){out<<"if-goto "<<l<<"\n";}
    void writeCall(string n,int a){out<<"call "<<n<<" "<<a<<"\n";}
    void writeFunction(string n,int l){out<<"function "<<n<<" "<<l<<"\n";}
    void writeReturn(){out<<"return\n";}
};

/* ---------- Compilation Engine ---------- */
class CompilationEngine {
    JackTokenizer &tk;
    VMWriter &vm;
    SymbolTable symbols;
    string className;
    int labelCounter=0;
public:
    CompilationEngine(JackTokenizer &t, VMWriter &v):tk(t),vm(v){}
    void compileClass() {
        tk.advance(); // 'class'
        tk.advance(); className = tk.token();
        tk.advance(); // '{'
        tk.advance();
        while (tk.token()=="static"||tk.token()=="field") compileClassVarDec();
        while (tk.token()=="constructor"||tk.token()=="function"||tk.token()=="method")
            compileSubroutine();
    }
private:
    void compileClassVarDec() {
        Kind kind = (tk.token()=="static"?STATIC:FIELD);
        tk.advance(); string type=tk.token();
        do {
            tk.advance(); tk.advance(); // identifier
            symbols.define(tk.token(),type,kind);
            tk.advance();
        } while (tk.token()==",");
        if(tk.token()==";") tk.advance();
    }
    void compileSubroutine() {
        symbols.startSubroutine();
        string subKind=tk.token(); tk.advance();
        tk.advance(); // return type
        string name=tk.token(); tk.advance(); // subroutine name
        tk.advance(); // '('
        compileParameterList();
        tk.advance(); // '{'
        while (tk.token()=="var") compileVarDec();
        int nLocals = symbols.varCount(VAR);
        vm.writeFunction(className+"."+name,nLocals);
        compileStatements();
        tk.advance(); // '}'
    }
    void compileParameterList() {
        while(tk.token()!=")") {
            string type=tk.token(); tk.advance();
            string name=tk.token(); tk.advance();
            symbols.define(name,type,ARG);
            if(tk.token()==",") tk.advance();
        }
    }
    void compileVarDec() {
        tk.advance(); string type=tk.token();
        do {
            tk.advance(); string name=tk.token();
            symbols.define(name,type,VAR);
            tk.advance();
        } while(tk.token()==",");
        tk.advance(); // ';'
    }
    void compileStatements() {
        while(true) {
            string s=tk.token();
            if(s=="let") compileLet();
            else if(s=="if") compileIf();
            else if(s=="while") compileWhile();
            else if(s=="do") compileDo();
            else if(s=="return") compileReturn();
            else break;
        }
    }
    void compileDo() {
        tk.advance(); compileSubCall(); vm.writePop("temp",0); tk.advance(); // ';'
    }
    void compileLet() {
        tk.advance(); string var=tk.token(); tk.advance();
        bool isArray=false;
        if(tk.token()=="["){isArray=true;tk.advance();compileExpression();vm.writePush(kind2seg(symbols.kindOf(var)),symbols.indexOf(var));vm.writeArithmetic("add");tk.advance();}
        tk.advance(); compileExpression();
        if(isArray){vm.writePop("temp",0);vm.writePop("pointer",1);vm.writePush("temp",0);vm.writePop("that",0);}
        else vm.writePop(kind2seg(symbols.kindOf(var)),symbols.indexOf(var));
        tk.advance(); // ';'
    }
    void compileWhile() {
        string L1=label("WHILE_EXP"),L2=label("WHILE_END");
        vm.writeLabel(L1);
        tk.advance(); compileExpression(); vm.writeArithmetic("not"); vm.writeIf(L2);
        tk.advance(); // ')'
        tk.advance(); // '{'
        compileStatements();
        vm.writeGoto(L1); vm.writeLabel(L2);
        tk.advance(); // '}'
    }
    void compileReturn() {
        tk.advance();
        if(tk.token()!=";") compileExpression(); else vm.writePush("constant",0);
        vm.writeReturn(); tk.advance();
    }
    void compileIf() {
        string L1=label("IF_TRUE"),L2=label("IF_FALSE"),L3=label("IF_END");
        tk.advance(); compileExpression(); vm.writeIf(L1); vm.writeGoto(L2); vm.writeLabel(L1);
        tk.advance(); // ')'
        tk.advance(); // '{'
        compileStatements(); tk.advance(); // '}'
        if(tk.token()=="else"){vm.writeGoto(L3);vm.writeLabel(L2);tk.advance();tk.advance();compileStatements();tk.advance();vm.writeLabel(L3);}
        else vm.writeLabel(L2);
    }
    void compileExpression() {
        compileTerm();
        while(strchr("+-*/&|<>=~", tk.token()[0]) || tk.token()=="=") {
            string op=tk.token(); tk.advance(); compileTerm();
            writeOp(op);
        }
    }
    void compileTerm() {
        string t=tk.token();
        if(t=="("){tk.advance();compileExpression();tk.advance();}
        else if(t=="-"){tk.advance();compileTerm();vm.writeArithmetic("neg");}
        else if(t=="~"){tk.advance();compileTerm();vm.writeArithmetic("not");}
        else if(regex_match(t,regex("\\d+"))){vm.writePush("constant",stoi(t));tk.advance();}
        else if(t.size()>=2&&t.front()=='\"'){string s=t.substr(1,t.size()-2);vm.writePush("constant",s.size());vm.writeCall("String.new",1);for(char c:s){vm.writePush("constant",c);vm.writeCall("String.appendChar",2);}tk.advance();}
        else if(t=="true"||t=="false"||t=="null"||t=="this"){
            if(t=="true"){vm.writePush("constant",0);vm.writeArithmetic("not");}
            else if(t=="false"||t=="null")vm.writePush("constant",0);
            else if(t=="this")vm.writePush("pointer",0);
            tk.advance();
        }
        else {
            string name=t; tk.advance();
            if(tk.token()=="["){tk.advance();compileExpression();vm.writePush(kind2seg(symbols.kindOf(name)),symbols.indexOf(name));vm.writeArithmetic("add");vm.writePop("pointer",1);vm.writePush("that",0);tk.advance();}
            else if(tk.token()=="("||tk.token()=="."){compileSubCall(name);}
            else vm.writePush(kind2seg(symbols.kindOf(name)),symbols.indexOf(name));
        }
    }
    void compileSubCall(string first="") {
        string name=first;
        if(name.empty()){name=tk.token();tk.advance();}
        string full;
        int nArgs=0;
        if(tk.token()=="."){tk.advance();string sub=tk.token();tk.advance();
            Kind k=symbols.kindOf(name);
            if(k!=NONE){vm.writePush(kind2seg(k),symbols.indexOf(name));full=symbols.typeOf(name)+"."+sub;nArgs=1;}
            else full=name+"."+sub;
        } else {full=className+"."+name;vm.writePush("pointer",0);nArgs=1;}
        tk.advance(); // '('
        nArgs+=compileExprList();
        vm.writeCall(full,nArgs);
        tk.advance(); // ')'
    }
    int compileExprList() {
        int n=0;
        if(tk.token()!=")"){compileExpression();n++;
            while(tk.token()==","){tk.advance();compileExpression();n++;}}
        return n;
    }
    string label(string base){return base+to_string(labelCounter++);}
    string kind2seg(Kind k){
        if(k==STATIC)return"static";if(k==FIELD)return"this";
        if(k==ARG)return"argument";if(k==VAR)return"local";return"";
    }
    void writeOp(string op){
        if(op=="+")vm.writeArithmetic("add");
        else if(op=="-")vm.writeArithmetic("sub");
        else if(op=="*")vm.writeCall("Math.multiply",2);
        else if(op=="/")vm.writeCall("Math.divide",2);
        else if(op=="&")vm.writeArithmetic("and");
        else if(op=="|")vm.writeArithmetic("or");
        else if(op=="<")vm.writeArithmetic("lt");
        else if(op==">")vm.writeArithmetic("gt");
        else if(op=="=")vm.writeArithmetic("eq");
    }
};

/* ---------- MAIN ---------- */
int main(int argc, char **argv) {
    if(argc!=2){cerr<<"Usage: JackCompiler <file or dir>\n";return 1;}
    string path=argv[1];
    vector<string> files;
    if(fs::is_directory(path))for(auto&p:fs::directory_iterator(path))if(p.path().extension()==".jack")files.push_back(p.path());
    else if(fs::path(path).extension()==".jack")files.push_back(path);
    for(auto &f:files){
        ifstream in(f); ofstream out(fs::path(f).replace_extension(".vm"));
        JackTokenizer tk(in); VMWriter vm(out); CompilationEngine ce(tk,vm);
        ce.compileClass();
        cerr<<"Compiled "<<f<<"\n";
    }
}
