#include "nexium/compiler/parser.hpp"
#include <utility>

namespace nexium::compiler {
const Token& Parser::peek() const { return tokens_[current_ < tokens_.size()?current_:tokens_.size()-1]; }
const Token& Parser::previous() const { return tokens_[current_?current_-1:0]; }
bool Parser::check(TokenKind k) const { return peek().kind==k; }
bool Parser::match(TokenKind k) { if(!check(k)) return false; if(current_<tokens_.size()) ++current_; return true; }
bool Parser::consume(TokenKind k,const char* message) { if(match(k)) return true; error(peek(),message); return false; }
void Parser::error(const Token& t,const char* message) { diagnostics_.push_back({t.location,message}); }
void Parser::skip_newlines() { while(match(TokenKind::newline)) {} }
void Parser::synchronize() {
    while(!check(TokenKind::end) && !check(TokenKind::newline) && !check(TokenKind::dedent)) ++current_;
    if(check(TokenKind::newline)) ++current_;
}
ParseResult Parser::parse() {
    ParseResult result;
    skip_newlines();
    while(!check(TokenKind::end)) {
        const auto before=current_;
        auto item=statement();
        if(item) result.statements.push_back(std::move(item));
        if(current_==before) { error(peek(),"parser made no progress"); ++current_; }
        skip_newlines();
    }
    result.diagnostics=std::move(diagnostics_);
    return result;
}
std::vector<std::unique_ptr<Stmt>> Parser::block() {
    std::vector<std::unique_ptr<Stmt>> items;
    if(!consume(TokenKind::newline,"expected a new line after ':'")) return items;
    skip_newlines();
    if(!consume(TokenKind::indent,"expected an indented block")) return items;
    skip_newlines();
    while(!check(TokenKind::dedent) && !check(TokenKind::end)) {
        const auto before=current_;
        auto item=statement();
        if(item) items.push_back(std::move(item));
        if(current_==before) { error(peek(),"parser made no progress in block"); ++current_; }
        skip_newlines();
    }
    consume(TokenKind::dedent,"expected the end of the indented block");
    return items;
}
std::unique_ptr<Stmt> Parser::statement() {
    auto node=std::make_unique<Stmt>(); node->location=peek().location;
    if(match(TokenKind::kw_show)) {
        node->kind=StmtKind::show; node->expression=expression();
        if(!node->expression) synchronize();
        return node;
    }
    if(match(TokenKind::kw_give)) {
        node->kind=StmtKind::give; node->expression=expression();
        if(!node->expression) synchronize();
        return node;
    }
    if(match(TokenKind::kw_make)) {
        node->kind=StmtKind::function;
        if(!consume(TokenKind::identifier,"expected a function name after 'make'")) { synchronize(); return nullptr; }
        node->name=previous().lexeme;
        if(!consume(TokenKind::left_paren,"expected '(' after function name")) { synchronize(); return nullptr; }
        if(!check(TokenKind::right_paren)) {
            do {
                if(!consume(TokenKind::identifier,"expected a parameter name")) { synchronize(); return nullptr; }
                node->parameters.push_back(previous().lexeme);
            } while(match(TokenKind::comma));
        }
        if(!consume(TokenKind::right_paren,"expected ')' after parameters") ||
           !consume(TokenKind::colon,"expected ':' before function body")) { synchronize(); return nullptr; }
        node->body=block(); return node;
    }
    if(match(TokenKind::kw_if)) {
        node->kind=StmtKind::if_statement; node->expression=expression();
        if(!node->expression || !consume(TokenKind::colon,"expected ':' after if condition")) { synchronize(); return nullptr; }
        node->body=block(); skip_newlines();
        if(match(TokenKind::kw_else)) {
            if(!consume(TokenKind::colon,"expected ':' after else")) { synchronize(); return node; }
            node->else_body=block();
        }
        return node;
    }
    if(match(TokenKind::kw_repeat)) {
        node->kind=StmtKind::repeat; node->expression=expression();
        if(!node->expression || !consume(TokenKind::kw_times,"expected 'times' after repeat count") ||
           !consume(TokenKind::colon,"expected ':' before repeat body")) { synchronize(); return nullptr; }
        node->body=block(); return node;
    }
    node->kind=StmtKind::expression; node->expression=expression();
    if(!node->expression) { synchronize(); return nullptr; }
    return node;
}
namespace {
int binary_precedence(TokenKind k) {
    switch(k) {
        case TokenKind::kw_or: return 1;
        case TokenKind::kw_and: return 2;
        case TokenKind::equal_equal: case TokenKind::bang_equal: return 3;
        case TokenKind::less: case TokenKind::less_equal: case TokenKind::greater: case TokenKind::greater_equal: return 4;
        case TokenKind::plus: case TokenKind::minus: return 5;
        case TokenKind::star: case TokenKind::slash: case TokenKind::percent: return 6;
        default: return 0;
    }
}
}
std::unique_ptr<Expr> Parser::expression() { return precedence(1); }
std::unique_ptr<Expr> Parser::precedence(int minimum) {
    auto left=prefix();
    if(!left) return nullptr;
    while(true) {
        const int p=binary_precedence(peek().kind);
        if(p<minimum) break;
        const Token op=peek(); ++current_;
        auto right=precedence(p+1);
        if(!right) { error(peek(),"expected expression after operator"); return left; }
        auto combined=std::make_unique<Expr>(); combined->kind=ExprKind::binary;
        combined->location=op.location; combined->value=op.lexeme.empty()?token_kind_name(op.kind):op.lexeme;
        combined->children.push_back(std::move(left)); combined->children.push_back(std::move(right));
        left=std::move(combined);
    }
    return left;
}
std::unique_ptr<Expr> Parser::prefix() {
    const Token token=peek();
    if(match(TokenKind::integer)) { auto e=std::make_unique<Expr>(); e->kind=ExprKind::integer; e->location=token.location; e->value=token.lexeme; return e; }
    if(match(TokenKind::floating)) { auto e=std::make_unique<Expr>(); e->kind=ExprKind::floating; e->location=token.location; e->value=token.lexeme; return e; }
    if(match(TokenKind::string_literal)) { auto e=std::make_unique<Expr>(); e->kind=ExprKind::string_literal; e->location=token.location; e->value=token.lexeme; return e; }
    if(match(TokenKind::kw_true) || match(TokenKind::kw_false)) { auto e=std::make_unique<Expr>(); e->kind=ExprKind::boolean; e->location=token.location; e->value=token.lexeme; return e; }
    if(match(TokenKind::identifier)) {
        auto e=std::make_unique<Expr>(); e->kind=ExprKind::name; e->location=token.location; e->value=token.lexeme;
        if(match(TokenKind::left_paren)) {
            auto call=std::make_unique<Expr>(); call->kind=ExprKind::call; call->location=token.location; call->value=token.lexeme;
            if(!check(TokenKind::right_paren)) {
                do { auto arg=expression(); if(!arg) return nullptr; call->children.push_back(std::move(arg)); } while(match(TokenKind::comma));
            }
            if(!consume(TokenKind::right_paren,"expected ')' after call arguments")) return nullptr;
            return call;
        }
        return e;
    }
    if(match(TokenKind::minus) || match(TokenKind::kw_not)) {
        auto e=std::make_unique<Expr>(); e->kind=ExprKind::unary; e->location=token.location; e->value=token.lexeme.empty()?token_kind_name(token.kind):token.lexeme;
        auto operand=precedence(7); if(!operand) { error(peek(),"expected an expression after unary operator"); return nullptr; }
        e->children.push_back(std::move(operand)); return e;
    }
    if(match(TokenKind::left_paren)) {
        auto e=expression(); if(!consume(TokenKind::right_paren,"expected ')' after expression")) return nullptr; return e;
    }
    error(peek(),"expected an expression"); return nullptr;
}
} // namespace nexium::compiler
