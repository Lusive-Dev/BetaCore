#include "nexium/compiler/lexer.hpp"
#include "nexium/compiler/parser.hpp"
#include "nexium/compiler/semantic.hpp"
#include <cassert>
#include <iostream>
using namespace nexium::compiler;
static SemanticResult check(const std::string& source) {
    auto lex=Lexer{}.tokenize(source); assert(lex.ok());
    auto parsed=Parser{lex.tokens}.parse(); assert(parsed.ok());
    return SemanticAnalyzer{}.analyze(parsed.statements);
}
int main() {
    const auto valid=check(
        "make add(a, b):\n"
        "    give a + b\n"
        "show add(2, 3)\n"
        "if true:\n"
        "    show \"ok\"\n"
        "repeat 4 times:\n"
        "    show 1 + 2\n");
    assert(valid.ok());
    const auto invalid=check(
        "make add(a, a):\n"
        "    give missing + 1\n"
        "show add(1)\n"
        "if 42:\n"
        "    show false\n"
        "repeat true times:\n"
        "    show 0\n"
        "give 5\n");
    assert(!invalid.ok());
    assert(invalid.diagnostics.size()>=6);
    std::cout<<"Nexium semantic tests passed.\n";
}
