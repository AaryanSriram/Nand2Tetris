#include <bits/stdc++.h>
using namespace std;

class Tokenizer {
    public:
    vector <string> tokens;
    unordered_set<string> keywords = {
        "class", "constructor", "function", "method", "field", "static", 
        "var", "int", "char", "boolean", "void", "true", "false", "this", 
        "null", "while", "let", "do", "if", "else", "return"
    };
    unordered_set<char> symbols = {
        '{', '}', '(', ')', '[', ']', '.', ',', ';',
        '+', '-', '*', '/', '%', '&', '|', '<', '>', '=', '~'
    };
    Tokenizer(const string &input)
    {
        tokenize(input);
    }
    string remLineComments(const string &input)
    {
        string res;
        bool check = false;
        for(auto i = 0; i<input.size(); i++)
        {
            if(!check && (i+1)<input.size() && input[i]=='/' && input[i+1]=='/')
            {
                check = true;
                i++;
            }
            else if(check && input[i]=='\n')
            {
                res+=input[i];
                check = false;
            }
            else if(!check)
            {
                res+=input[i];
            }
        }
        return res;
    }
    string remParComments(const string &input)
    {
        string res;
        bool check = false;
        for(auto i = 0; i<input.size(); i++)
        {
            if(!check && (i+2)<input.size() && input[i]=='/' 
                && input[i+1]=='*' && input[i+2]=='*')
            {
                check = true;
                i+=2;
            }
            else if(check && (i+1)<input.size() && input[i]=='*'
                && input[i+1]=='/')
            {
                check = false;
                i++;
            }
            else if(!check)
            {
                res+=input[i];
            }
        }
        return res;
    }
    string remComments(const string &input)
    {
        string temp = remLineComments(input);
        return remParComments(temp);
    }
    void tokenize(const string &input)
    {
        string s = remComments(input);
        auto i = 0;
        auto n = s.size();
        while(i<n)
        {
            if(isspace(s[i]))
            {
                i++;
                continue;
            }
            if(s[i] == '"')
            {
                auto j = i+1;
                while (j < n && s[j] != '"') j++;
                tokens.push_back(s.substr(i, j - i + 1));
                i = j + 1;
            }
            else if(symbols.count(s[i]))
            {
                string tem(1,s[i]);
                tokens.push_back(tem);
                i++;
            }
            else
            {
                auto j = i;
                while (j < n && !isspace(s[j]) && !symbols.count(s[j])) 
                {
                    j++;
                }
                tokens.push_back(s.substr(i, j - i));
                i = j;
            }
        }
    }
    string tokenType(string &t) 
    {
        if (keywords.count(t)) return "keyword";
        if (t.size() == 1 && symbols.count(t[0])) return "symbol";
        if (isdigit(t[0])) return "integerConstant";
        if (t.size() >= 2 && t.front() == '"' && t.back() == '"')
        {
            t = t.substr(1, t.size()-2);
            return "stringConstant";
        }
        return "identifier";
    }
    vector<string> getTokens() const { return tokens; }
};
int main() 
{
    string filename;
    cin >> filename;

    ifstream fin(filename);
    if (!fin) {
        cerr << "File not found!\n";
        return 1;
    }

    stringstream buffer;
    buffer << fin.rdbuf();
    fin.close();

    Tokenizer tokenizer(buffer.str());
    ofstream out("out.xml");
    out << "<tokens>\n";
    for (auto &t : tokenizer.getTokens()) {
        string k = tokenizer.tokenType(t); 
        if(t=="<") out << "<" << k << "> " << "&lt;"
         << " </" << k << ">\n";
        else if(t==">")out << "<" << k << "> " << "&gt;"
         << " </" << k << ">\n";
        else if(t=="&")out << "<" << k << "> " << "&amp;"
         << " </" << k << ">\n";
        else out << "<" << k << "> " << t
         << " </" << k << ">\n";
    }
    out << "</tokens>\n";
    return 0;
}