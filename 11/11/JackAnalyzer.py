import sys
import os
import glob
import re

KEYWORD = 0
SYM = 1
NUM = 2
STR = 3
ID = 4
ERROR = 5

STATIC = 0
FIELD = 1
ARG = 2
VAR = 3
NONE = 4

KEYWORDS = {
    'class', 'method', 'function', 'constructor', 'int', 'boolean',
    'char', 'void', 'var', 'static', 'field', 'let', 'do', 'if',
    'else', 'while', 'return', 'true', 'false', 'null', 'this'
}

SYMBOLS = '{}()[].,;+-*/&|<>=~'

KWD_TO_KIND = {'static': STATIC, 'field': FIELD}

VM_CMDS = {'+': 'add', '-': 'sub', '*': 'call Math.multiply 2', '/': 'call Math.divide 2',
           '<': 'lt', '>': 'gt', '=': 'eq', '&': 'and', '|': 'or'}
VM_UNARY_CMDS = {'-': 'neg', '~': 'not'}
SEGMENTS = {
    STATIC: 'static', 
    FIELD: 'this', 
    ARG: 'argument', 
    VAR: 'local', 
    None: 'ERROR'
}

TEMP_RETURN = 0
TEMP_ARRAY = 1

class Compiler:
    
    _comment_re = re.compile(r'//[^\n]*\n|/\*(.*?)\*/', re.MULTILINE | re.DOTALL)
    _sym_re_str = '[' + re.escape(SYMBOLS) + ']'
    _num_re_str = r'\d+'
    _str_re_str = r'"[^"\n]*"'
    _id_re_str = r'[a-zA-Z_]\w*'
    
    _word_re = re.compile(
        _id_re_str + '|' + 
        _sym_re_str + '|' + 
        _num_re_str + '|' + 
        _str_re_str
    )

    def __init__(self, filepath):
        
        self._outfile = None
        self.openout(filepath)
        
        with open(filepath, 'r') as infile:
            self._lines = infile.read()
        self._tokens = self._tokenize(self._lines)
        self._tokens.reverse()
        self._token_type = ERROR
        self._cur_val = 0
        
        self.class_symbols = {}
        self.subroutine_symbols = {}
        self.symbols = {
            STATIC: self.class_symbols,
            FIELD: self.class_symbols,
            ARG: self.subroutine_symbols,
            VAR: self.subroutine_symbols
        }
        self.index = {
            STATIC: 0, 
            FIELD: 0, 
            ARG: 0, 
            VAR: 0
        }
        
        self._cur_class = ""
        self._cur_subroutine = ""
        self.label_num = 0
        
        self.compile_class()
        
        self.closeout()

    def openout(self, path):
        outdir = os.path.join(os.path.dirname(path), 'output')
        file = os.path.join(outdir, os.path.basename(path))
        os.makedirs(outdir, exist_ok=True)
        self._outfile = open(file.replace('.jack', '.vm'), 'w')

    def closeout(self):
        self._outfile.close()

    def write_vm_cmd(self, cmd, arg1=None, arg2=None):
        parts = [cmd]
        if arg1 is not None:
            parts.append(str(arg1))
        if arg2 is not None:
            parts.append(str(arg2))
        self._outfile.write(' '.join(parts) + '\n')
        
    def has_more_tokens(self):
        return self._tokens != []

    def advance(self):
        if self.has_more_tokens():
            self._token_type, self._cur_val = self._tokens.pop()
        else:
            self._token_type, self._cur_val = (ERROR, 0)
        return (self._token_type, self._cur_val)

    def peek(self):
        if self.has_more_tokens():
            return self._tokens[-1]
        else:
            return (ERROR, 0)

    def _tokenize(self, lines):
        code = self._comment_re.sub('', lines)
        words = self._word_re.findall(code)
        return [self._token(word) for word in words]

    @staticmethod
    def _is_match(re_str, word):
        return re.match(re_str, word) is not None

    def _token(self, token_string):
        if token_string in KEYWORDS:
            return (KEYWORD, token_string)
        elif self._is_match(self._sym_re_str, token_string):
            return (SYM, token_string)
        elif self._is_match(self._num_re_str, token_string):
            return (NUM, token_string)
        elif self._is_match(self._str_re_str, token_string):
            return (STR, token_string[1:-1])
        elif self._is_match(self._id_re_str, token_string):
            return (ID, token_string)
        else:
            return (ERROR, token_string)
            
    def start_subroutine(self):
        self.subroutine_symbols.clear()
        self.index[ARG] = self.index[VAR] = 0

    def define(self, name, var_type, kind):
        self.symbols[kind][name] = (var_type, kind, self.index[kind])
        self.index[kind] += 1

    def var_count(self, kind):
        return sum(1 for (t, k, i) in self.symbols[kind].values() if k == kind)

    def type_of(self, name):
        (var_type, kind, index) = self.lookup(name)
        return var_type

    def kind_of(self, name):
        (var_type, kind, index) = self.lookup(name)
        return kind

    def index_of(self, name):
        (var_type, kind, index) = self.lookup(name)
        return index

    def lookup(self, name):
        if name in self.subroutine_symbols:
            return self.subroutine_symbols[name]
        elif name in self.class_symbols:
            return self.class_symbols[name]
        else:
            return (None, None, None)

    def vm_push_variable(self, name):
        (var_type, var_kind, var_index) = self.lookup(name)
        if var_kind is not None:
            self.write_vm_cmd('push', SEGMENTS[var_kind], var_index)

    def vm_pop_variable(self, name):
        (var_type, var_kind, var_index) = self.lookup(name)
        if var_kind is not None:
            self.write_vm_cmd('pop', SEGMENTS[var_kind], var_index)

    def compile_class(self):
        self.advance()
        self._cur_class = self.advance()[1]
        self.advance()

        lextok, lexval = self.peek()
        while lextok == KEYWORD and lexval in ('static', 'field'):
            self.compile_class_var_dec()
            lextok, lexval = self.peek()

        lextok, lexval = self.peek()
        while lextok == KEYWORD and lexval in ('constructor', 'function', 'method'):
            self.compile_subroutine()
            lextok, lexval = self.peek()

        self.advance()

    def compile_class_var_dec(self):
        tok, kwd = self.advance()
        self._compile_dec(KWD_TO_KIND[kwd])

    def _compile_dec(self, kind):
        var_type = self.compile_type()
        name = self.advance()[1]
        self.define(name, var_type, kind)
        
        while self.peek()[0] == SYM and self.peek()[1] == ',':
            self.advance()
            name = self.advance()[1]
            self.define(name, var_type, kind)
            
        self.advance()

    def compile_void_or_type(self):
        lextok, lexval = self.peek()
        if lextok == KEYWORD and lexval == 'void':
            return self.advance()[1]
        else:
            return self.compile_type()

    def compile_type(self):
        lextok, lexval = self.peek()
        if lextok == ID or (lextok == KEYWORD and lexval in ('int', 'char', 'boolean')):
            return self.advance()[1]

    def compile_subroutine(self):
        tok, subroutine_type = self.advance()
        return_type = self.compile_void_or_type()
        self._cur_subroutine = self.advance()[1]
        
        self.start_subroutine()
        if subroutine_type == 'method':
            self.define('this', self._cur_class, ARG)
            
        self.advance()
        self.compile_parameter_list()
        self.advance()
        self.compile_subroutine_body(subroutine_type)

    def compile_parameter_list(self):
        lextok, lexval = self.peek()
        if lextok == ID or (lextok == KEYWORD and lexval in ('int', 'char', 'boolean')):
            self.compile_parameter()
            while self.peek()[0] == SYM and self.peek()[1] == ',':
                self.advance()
                self.compile_parameter()

    def compile_parameter(self):
        lextok, lexval = self.peek()
        if lextok == ID or (lextok == KEYWORD and lexval in ('int', 'char', 'boolean')):
            param_type = self.compile_type()
            name = self.advance()[1]
            self.define(name, param_type, ARG)

    def compile_subroutine_body(self, subroutine_type):
        self.advance()
        
        lextok, lexval = self.peek()
        while lextok == KEYWORD and lexval == 'var':
            self.compile_var_dec()
            lextok, lexval = self.peek()
            
        self.write_func_decl(subroutine_type)
        self.compile_statements()
        self.advance()

    def write_func_decl(self, subroutine_type):
        func_name = f"{self._cur_class}.{self._cur_subroutine}"
        self.write_vm_cmd('function', func_name, self.var_count(VAR))
        
        if subroutine_type == 'method':
            self.write_vm_cmd('push', 'argument', 0)
            self.write_vm_cmd('pop', 'pointer', 0)
        elif subroutine_type == 'constructor':
            self.write_vm_cmd('push', 'constant', self.var_count(FIELD))
            self.write_vm_cmd('call', 'Memory.alloc', 1)
            self.write_vm_cmd('pop', 'pointer', 0)
            
    def compile_var_dec(self):
        self.advance()
        return self._compile_dec(VAR)

    def compile_statements(self):
        lextok, lexval = self.peek()
        while lextok == KEYWORD and lexval in ('let', 'if', 'while', 'do', 'return'):
            if lexval == 'let':
                self.compile_let()
            elif lexval == 'if':
                self.compile_if()
            elif lexval == 'while':
                self.compile_while()
            elif lexval == 'do':
                self.compile_do()
            elif lexval == 'return':
                self.compile_return()
            lextok, lexval = self.peek()

    def compile_let(self):
        self.advance()
        name = self.advance()[1]
        
        is_array = self.peek()[0] == SYM and self.peek()[1] == '['
        
        if is_array:
            self.vm_push_variable(name)
            self.advance()
            self.compile_expression()
            self.advance()
            self.write_vm_cmd('add')
            
        self.advance()
        self.compile_expression()
        self.advance()
        
        if is_array:
            self.write_vm_cmd('pop', 'temp', TEMP_ARRAY)
            self.write_vm_cmd('pop', 'pointer', 1)
            self.write_vm_cmd('push', 'temp', TEMP_ARRAY)
            self.write_vm_cmd('pop', 'that', 0)
        else:
            self.vm_pop_variable(name)

    def compile_if(self):
        self.advance()
        
        self.label_num += 1
        notif_label = f"label{self.label_num}"
        self.label_num += 1
        end_label = f"label{self.label_num}"

        self.advance()
        self.compile_expression()
        self.advance()
        
        self.write_vm_cmd('not')
        self.write_vm_cmd('if-goto', notif_label)
        
        self.advance()
        self.compile_statements()
        self.advance()
        self.write_vm_cmd('goto', end_label)

        self.write_vm_cmd('label', notif_label)
        
        lextok, lexval = self.peek()
        if lextok == KEYWORD and lexval == 'else':
            self.advance()
            self.advance()
            self.compile_statements()
            self.advance()
            
        self.write_vm_cmd('label', end_label)

    def compile_while(self):
        self.advance()
        
        self.label_num += 1
        top_label = f"label{self.label_num}"
        self.label_num += 1
        notif_label = f"label{self.label_num}"

        self.write_vm_cmd('label', top_label)
        
        self.advance()
        self.compile_expression()
        self.advance()
        
        self.write_vm_cmd('not')
        self.write_vm_cmd('if-goto', notif_label)
        
        self.advance()
        self.compile_statements()
        self.advance()
        self.write_vm_cmd('goto', top_label)

        self.write_vm_cmd('label', notif_label)

    def compile_do(self):
        self.advance()
        name = self.advance()[1]
        self.compile_subroutine_call(name)
        self.write_vm_cmd('pop', 'temp', TEMP_RETURN)
        self.advance()

    def compile_return(self):
        self.advance()
        
        if not (self.peek()[0] == SYM and self.peek()[1] == ';'):
            self.compile_expression()
        else:
            self.write_vm_cmd('push', 'constant', 0)
            
        self.advance()
        self.write_vm_cmd('return')

    def compile_expression(self):
        self.compile_term()
        
        lextok, lexval = self.peek()
        while lextok == SYM and lexval in '+-*/&|<>=.':
            op = self.advance()[1]
            self.compile_term()
            if op in VM_CMDS:
                self.write_vm_cmd(VM_CMDS[op])
            lextok, lexval = self.peek()

    def compile_term(self):
        lextok, lexval = self.peek()

        is_const = lextok == NUM or lextok == STR or \
                   (lextok == KEYWORD and lexval in ('true', 'false', 'null', 'this'))

        if is_const:
            self.compile_const()
        elif lextok == SYM and lexval == '(':
            self.advance()
            self.compile_expression()
            self.advance()
        elif lextok == SYM and lexval in '-~':
            op = self.advance()[1]
            self.compile_term()
            self.write_vm_cmd(VM_UNARY_CMDS[op])
        elif lextok == ID:
            name = self.advance()[1]
            
            lextok_peek, lexval_peek = self.peek()
            if lextok_peek == SYM and lexval_peek == '[':
                self.compile_array_subscript(name)
            elif lextok_peek == SYM and lexval_peek in '(.':
                self.compile_subroutine_call(name)
            else:
                self.vm_push_variable(name)

    def compile_const(self):
        tok, val = self.advance()
        if tok == NUM:
            self.write_vm_cmd('push', 'constant', val)
        elif tok == STR:
            self.write_vm_cmd('push', 'constant', len(val))
            self.write_vm_cmd('call', 'String.new', 1)
            for c in val:
                self.write_vm_cmd('push', 'constant', ord(c))
                self.write_vm_cmd('call', 'String.appendChar', 2)
        elif tok == KEYWORD:
            if val == 'this':
                self.write_vm_cmd('push', 'pointer', 0)
            elif val == 'true':
                self.write_vm_cmd('push', 'constant', 1)
                self.write_vm_cmd('neg')
            else:
                self.write_vm_cmd('push', 'constant', 0)

    def compile_array_subscript(self, name):
        self.vm_push_variable(name)
        self.advance()
        self.compile_expression()
        self.advance()
        self.write_vm_cmd('add')
        self.write_vm_cmd('pop', 'pointer', 1)
        self.write_vm_cmd('push', 'that', 0)

    def compile_subroutine_call(self, name):
        (var_type, var_kind, var_index) = self.lookup(name)
        num_args = 0
        
        if self.peek()[0] == SYM and self.peek()[1] == '.':
            self.advance()
            sub_name = self.advance()[1]
            
            if var_type in ('int', 'char', 'boolean', 'void'):
                pass
            elif var_type is None:
                name = name + '.' + sub_name
            else:
                num_args = 1
                self.vm_push_variable(name)
                name = self.type_of(name) + '.' + sub_name
        else:
            num_args = 1
            self.write_vm_cmd('push', 'pointer', 0)
            name = self._cur_class + '.' + name
            
        self.advance()
        num_args += self.compile_expr_list()
        self.advance()
        self.write_vm_cmd('call', name, num_args)

    def compile_expr_list(self):
        num_args = 0
        
        lextok, lexval = self.peek()
        is_const = lextok == NUM or lextok == STR or \
                   (lextok == KEYWORD and lexval in ('true', 'false', 'null', 'this'))
        is_term = is_const or lextok == ID or (lextok == SYM and lexval in '(-~')

        if is_term:
            self.compile_expression()
            num_args = 1
            while self.peek()[0] == SYM and self.peek()[1] == ',':
                self.advance()
                self.compile_expression()
                num_args += 1
        return num_args

if len(sys.argv) == 2:
    file_or_dir = sys.argv[1]
    infiles = []
    if file_or_dir.endswith('.jack'):
        infiles = [file_or_dir]
    else:
        infiles = glob.glob(os.path.join(file_or_dir, '*.jack'))
    
    if infiles:
        for infile in infiles:
            Compiler(infile)
