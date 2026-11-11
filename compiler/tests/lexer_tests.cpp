#include "nexium/compiler/lexer.hpp"
#include <cassert>
#include <iostream>
using namespace nexium::compiler;
int main() {
    const std::string source=
        "make add(a, b):\n"
        "    give a + b\n"
        "show \"hello\\nworld\"\n"
        "if 12 >= 10:\n"
        "    show true # comment\n";
    const auto result=Lexer{}.tokenize(source);
    assert(result.ok());
    assert(!result.tokens.empty() && result.tokens.back().kind==TokenKind::end);
    int indents=0, dedents=0, strings=0, keywords=0;
    for(const auto& token:result.tokens) {
        if(token.kind==TokenKind::indent) ++indents;
        if(token.kind==TokenKind::dedent) ++dedents;
        if(token.kind==TokenKind::string_literal) {
            ++strings; assert(token.lexeme=="hello\nworld");
        }
        if(token.kind==TokenKind::kw_make || token.kind==TokenKind::kw_give ||
           token.kind==TokenKind::kw_show || token.kind==TokenKind::kw_if) ++keywords;
    }
    assert(indents==2 && dedents==2 && strings==1 && keywords==5);
    const auto bad=Lexer{}.tokenize("show \"unfinished\n\tshow @");
    assert(!bad.ok());
    std::cout<<"Nexium lexer tests passed.\n";
}
