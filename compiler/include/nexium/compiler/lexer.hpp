#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace nexium::compiler {
enum class TokenKind {
    end, newline, indent, dedent, identifier, integer, floating, string_literal,
    kw_show, kw_if, kw_else, kw_repeat, kw_times, kw_make, kw_give,
    kw_true, kw_false, kw_and, kw_or, kw_not,
    left_paren, right_paren, comma, colon, plus, minus, star, slash, percent,
    equal, equal_equal, bang_equal, less, less_equal, greater, greater_equal,
    arrow
};
struct SourceLocation { std::size_t offset{0}; std::size_t line{1}; std::size_t column{1}; };
struct Token { TokenKind kind{TokenKind::end}; std::string lexeme; SourceLocation location{}; };
struct Diagnostic { SourceLocation location{}; std::string message; };
struct LexResult {
    std::vector<Token> tokens;
    std::vector<Diagnostic> diagnostics;
    bool ok() const noexcept { return diagnostics.empty(); }
};
class Lexer {
public:
    LexResult tokenize(const std::string& source) const;
};
const char* token_kind_name(TokenKind kind) noexcept;
} // namespace nexium::compiler
