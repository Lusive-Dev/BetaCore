#pragma once
#include "nexium/compiler/lexer.hpp"
#include <memory>
#include <string>
#include <vector>

namespace nexium::compiler {
enum class ExprKind { integer, floating, string_literal, boolean, name, unary, binary, call };
struct Expr {
    ExprKind kind{ExprKind::integer};
    SourceLocation location{};
    std::string value;
    std::vector<std::unique_ptr<Expr>> children;
};
enum class StmtKind { show, give, expression, function, if_statement, repeat };
struct Stmt {
    StmtKind kind{StmtKind::expression};
    SourceLocation location{};
    std::string name;
    std::vector<std::string> parameters;
    std::unique_ptr<Expr> expression;
    std::vector<std::unique_ptr<Stmt>> body;
    std::vector<std::unique_ptr<Stmt>> else_body;
};
struct ParseResult {
    std::vector<std::unique_ptr<Stmt>> statements;
    std::vector<Diagnostic> diagnostics;
    bool ok() const noexcept { return diagnostics.empty(); }
};
class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens) : tokens_(tokens) {}
    ParseResult parse();
private:
    const std::vector<Token>& tokens_;
    std::size_t current_{0};
    std::vector<Diagnostic> diagnostics_;
    const Token& peek() const;
    const Token& previous() const;
    bool check(TokenKind kind) const;
    bool match(TokenKind kind);
    bool consume(TokenKind kind,const char* message);
    void skip_newlines();
    void synchronize();
    std::unique_ptr<Stmt> statement();
    std::vector<std::unique_ptr<Stmt>> block();
    std::unique_ptr<Expr> expression();
    std::unique_ptr<Expr> precedence(int minimum);
    std::unique_ptr<Expr> prefix();
    void error(const Token& token,const char* message);
};
} // namespace nexium::compiler
