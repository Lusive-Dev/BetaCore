#include "nexium/compiler/lexer.hpp"
#include <cctype>
#include <cstdint>
#include <limits>

namespace nexium::compiler {
namespace {
bool identifier_start(unsigned char c) { return std::isalpha(c) || c=='_'; }
bool identifier_continue(unsigned char c) { return std::isalnum(c) || c=='_'; }
TokenKind keyword(const std::string& s) {
    if(s=="show") return TokenKind::kw_show;
    if(s=="if") return TokenKind::kw_if;
    if(s=="else") return TokenKind::kw_else;
    if(s=="repeat") return TokenKind::kw_repeat;
    if(s=="times") return TokenKind::kw_times;
    if(s=="make") return TokenKind::kw_make;
    if(s=="give") return TokenKind::kw_give;
    if(s=="true") return TokenKind::kw_true;
    if(s=="false") return TokenKind::kw_false;
    if(s=="and") return TokenKind::kw_and;
    if(s=="or") return TokenKind::kw_or;
    if(s=="not") return TokenKind::kw_not;
    return TokenKind::identifier;
}
}
const char* token_kind_name(TokenKind k) noexcept {
    switch(k) {
#define NEXIUM_TOKEN(x) case TokenKind::x: return #x
        NEXIUM_TOKEN(end); NEXIUM_TOKEN(newline); NEXIUM_TOKEN(indent); NEXIUM_TOKEN(dedent);
        NEXIUM_TOKEN(identifier); NEXIUM_TOKEN(integer); NEXIUM_TOKEN(floating); NEXIUM_TOKEN(string_literal);
        NEXIUM_TOKEN(kw_show); NEXIUM_TOKEN(kw_if); NEXIUM_TOKEN(kw_else); NEXIUM_TOKEN(kw_repeat);
        NEXIUM_TOKEN(kw_times); NEXIUM_TOKEN(kw_make); NEXIUM_TOKEN(kw_give); NEXIUM_TOKEN(kw_true);
        NEXIUM_TOKEN(kw_false); NEXIUM_TOKEN(kw_and); NEXIUM_TOKEN(kw_or); NEXIUM_TOKEN(kw_not);
        NEXIUM_TOKEN(left_paren); NEXIUM_TOKEN(right_paren); NEXIUM_TOKEN(comma); NEXIUM_TOKEN(colon);
        NEXIUM_TOKEN(plus); NEXIUM_TOKEN(minus); NEXIUM_TOKEN(star); NEXIUM_TOKEN(slash);
        NEXIUM_TOKEN(percent); NEXIUM_TOKEN(equal); NEXIUM_TOKEN(equal_equal); NEXIUM_TOKEN(bang_equal);
        NEXIUM_TOKEN(less); NEXIUM_TOKEN(less_equal); NEXIUM_TOKEN(greater); NEXIUM_TOKEN(greater_equal);
        NEXIUM_TOKEN(arrow);
#undef NEXIUM_TOKEN
    }
    return "unknown";
}
LexResult Lexer::tokenize(const std::string& src) const {
    LexResult out;
    std::vector<std::size_t> indents{0};
    std::size_t i=0, line=1, col=1;
    bool at_line_start=true;
    auto loc=[&] { return SourceLocation{i,line,col}; };
    auto emit=[&](TokenKind k,std::size_t start,const SourceLocation& where) {
        out.tokens.push_back(Token{k,src.substr(start,i-start),where});
    };
    auto error=[&](const SourceLocation& where,const char* message) { out.diagnostics.push_back(Diagnostic{where,message}); };
    while(i<src.size()) {
        if(at_line_start) {
            const SourceLocation start=loc();
            std::size_t width=0;
            while(i<src.size() && (src[i]==' ' || src[i]=='\t')) {
                if(src[i]=='\t') { error(loc(),"tabs are not allowed for indentation; use spaces"); ++width; }
                else ++width;
                ++i; ++col;
            }
            if(i<src.size() && src[i]!='\n' && src[i]!='\r' && src[i]!='#') {
                if(width>indents.back()) { indents.push_back(width); emit(TokenKind::indent,i,start); }
                else if(width<indents.back()) {
                    while(indents.size()>1 && width<indents.back()) { indents.pop_back(); out.tokens.push_back(Token{TokenKind::dedent,"",loc()}); }
                    if(width!=indents.back()) error(start,"indentation does not match an outer block");
                }
            }
            at_line_start=false;
            if(i>=src.size()) break;
        }
        const char c=src[i];
        if(c==' ' || c=='\t') { ++i; ++col; continue; }
        if(c=='#') { while(i<src.size() && src[i]!='\n' && src[i]!='\r') { ++i; ++col; } continue; }
        if(c=='\r' || c=='\n') {
            const auto where=loc();
            if(c=='\r' && i+1<src.size() && src[i+1]=='\n') ++i;
            ++i; ++line; col=1; at_line_start=true;
            out.tokens.push_back(Token{TokenKind::newline,"",where});
            continue;
        }
        const SourceLocation where=loc();
        const std::size_t start=i;
        if(identifier_start(static_cast<unsigned char>(c))) {
            ++i; ++col;
            while(i<src.size() && identifier_continue(static_cast<unsigned char>(src[i]))) { ++i; ++col; }
            const auto text=src.substr(start,i-start);
            out.tokens.push_back(Token{keyword(text),text,where}); continue;
        }
        if(std::isdigit(static_cast<unsigned char>(c))) {
            ++i; ++col;
            while(i<src.size() && std::isdigit(static_cast<unsigned char>(src[i]))) { ++i; ++col; }
            bool is_float=false;
            if(i+1<src.size() && src[i]=='.' && std::isdigit(static_cast<unsigned char>(src[i+1]))) {
                is_float=true; ++i; ++col;
                while(i<src.size() && std::isdigit(static_cast<unsigned char>(src[i]))) { ++i; ++col; }
            }
            emit(is_float?TokenKind::floating:TokenKind::integer,start,where); continue;
        }
        if(c=='"') {
            ++i; ++col; std::string value; bool closed=false;
            while(i<src.size()) {
                char ch=src[i++];
                if(ch=='"') { ++col; closed=true; break; }
                if(ch=='\n' || ch=='\r') { error(where,"string literal cannot cross a line"); break; }
                ++col;
                if(ch=='\\') {
                    if(i>=src.size()) { error(where,"unfinished escape sequence"); break; }
                    char e=src[i++]; ++col;
                    switch(e) { case 'n': value.push_back('\n'); break; case 'r': value.push_back('\r'); break;
                        case 't': value.push_back('\t'); break; case '0': value.push_back('\0'); break;
                        case '\\': value.push_back('\\'); break; case '"': value.push_back('"'); break;
                        default: error(where,"unsupported string escape"); value.push_back(e); break; }
                } else value.push_back(ch);
            }
            if(!closed) error(where,"unterminated string literal");
            out.tokens.push_back(Token{TokenKind::string_literal,std::move(value),where}); continue;
        }
        auto one=[&](TokenKind k) { ++i; ++col; emit(k,start,where); };
        auto two=[&](char expected,TokenKind yes,TokenKind no) {
            ++i; ++col;
            if(i<src.size() && src[i]==expected) { ++i; ++col; emit(yes,start,where); }
            else emit(no,start,where);
        };
        switch(c) {
            case '(': one(TokenKind::left_paren); break; case ')': one(TokenKind::right_paren); break;
            case ',': one(TokenKind::comma); break; case ':': one(TokenKind::colon); break;
            case '+': one(TokenKind::plus); break; case '*': one(TokenKind::star); break;
            case '/': one(TokenKind::slash); break; case '%': one(TokenKind::percent); break;
            case '-': ++i; ++col; if(i<src.size() && src[i]=='>') { ++i; ++col; emit(TokenKind::arrow,start,where); } else emit(TokenKind::minus,start,where); break;
            case '=': two('=',TokenKind::equal_equal,TokenKind::equal); break;
            case '!': two('=',TokenKind::bang_equal,TokenKind::end); if(out.tokens.back().kind==TokenKind::end) error(where,"expected '=' after '!'"); break;
            case '<': two('=',TokenKind::less_equal,TokenKind::less); break;
            case '>': two('=',TokenKind::greater_equal,TokenKind::greater); break;
            default: error(where,"unexpected character"); ++i; ++col; break;
        }
    }
    if(!src.empty() && src.back()!='\n' && src.back()!='\r')
        out.tokens.push_back(Token{TokenKind::newline,"",SourceLocation{src.size(),line,col}});
    while(indents.size()>1) { indents.pop_back(); out.tokens.push_back(Token{TokenKind::dedent,"",SourceLocation{src.size(),line,col}}); }
    out.tokens.push_back(Token{TokenKind::end,"",SourceLocation{src.size(),line,col}});
    return out;
}
} // namespace nexium::compiler
