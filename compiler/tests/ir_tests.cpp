#include "nexium/compiler/ir.hpp"
#include "nexium/compiler/lexer.hpp"
#include "nexium/compiler/parser.hpp"
#include "nexium/compiler/semantic.hpp"
#include <cassert>
#include <iostream>
using namespace nexium::compiler;
int main() {
    const std::string source=
        "make add(a, b):\n"
        "    give a + b\n"
        "show add(2, 3)\n"
        "if true:\n"
        "    show \"yes\"\n"
        "else:\n"
        "    show \"no\"\n"
        "repeat 3 times:\n"
        "    show 1\n";
    auto lex=Lexer{}.tokenize(source); assert(lex.ok());
    auto parsed=Parser{lex.tokens}.parse(); assert(parsed.ok());
    assert(SemanticAnalyzer{}.analyze(parsed.statements).ok());
    auto ir=IRLowerer{}.lower(parsed.statements);
    if(!ir.ok()) for(const auto& d:ir.diagnostics) std::cerr<<d.message<<"\n";
    assert(ir.ok());
    assert(ir.functions.size()==2);
    assert(ir.functions[0].name=="main");
    assert(ir.functions[1].name=="add");
    assert(!ir.functions[0].instructions.empty());
    bool saw_call=false,saw_branch=false,saw_print=false;
    for(const auto& i:ir.functions[0].instructions) {
        saw_call|=i.op==IROp::call; saw_branch|=i.op==IROp::branch; saw_print|=i.op==IROp::print;
    }
    assert(saw_call && saw_branch && saw_print);
    std::cout<<"Nexium IR tests passed.\n";
}
