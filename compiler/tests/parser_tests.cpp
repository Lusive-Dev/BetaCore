#include "nexium/compiler/lexer.hpp"
#include "nexium/compiler/parser.hpp"
#include <cassert>
#include <iostream>
using namespace nexium::compiler;
int main() {
    const std::string source=
        "make add(a, b):\n"
        "    give a + b * 2\n"
        "show add(3, 4)\n"
        "if 10 > 5:\n"
        "    show \"yes\"\n"
        "else:\n"
        "    show \"no\"\n"
        "repeat 3 times:\n"
        "    show true\n";
    auto lex=Lexer{}.tokenize(source); assert(lex.ok());
    auto parsed=Parser{lex.tokens}.parse();
    if(!parsed.ok()) {
        for(const auto& d:parsed.diagnostics) std::cerr<<d.location.line<<":"<<d.location.column<<" "<<d.message<<"\n";
    }
    assert(parsed.ok());
    assert(parsed.statements.size()==4);
    assert(parsed.statements[0]->kind==StmtKind::function);
    assert(parsed.statements[0]->parameters.size()==2);
    assert(parsed.statements[0]->body.size()==1);
    assert(parsed.statements[1]->kind==StmtKind::show);
    assert(parsed.statements[1]->expression->kind==ExprKind::call);
    assert(parsed.statements[2]->kind==StmtKind::if_statement);
    assert(parsed.statements[2]->else_body.size()==1);
    assert(parsed.statements[3]->kind==StmtKind::repeat);
    assert(parsed.statements[3]->body.size()==1);
    const auto badlex=Lexer{}.tokenize("if 4 > :\n    show 1\n");
    auto bad=Parser{badlex.tokens}.parse();
    assert(!bad.ok());
    std::cout<<"Nexium parser tests passed.\n";
}
