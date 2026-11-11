#include "nexium/compiler/semantic.hpp"
#include <cstdlib>
#include <utility>

namespace nexium::compiler {
const char* value_type_name(ValueType t) noexcept {
    switch(t) {
        case ValueType::integer:return "integer";
        case ValueType::floating:return "floating";
        case ValueType::string:return "string";
        case ValueType::boolean:return "boolean";
        default:return "unknown";
    }
}
void SemanticAnalyzer::error(SourceLocation l,const std::string& m) { diagnostics_.push_back({l,m}); }
ValueType SemanticAnalyzer::lookup(const std::string& name) const {
    for(auto scope=scopes_.rbegin();scope!=scopes_.rend();++scope) {
        const auto found=scope->find(name);
        if(found!=scope->end()) return found->second;
    }
    return ValueType::unknown;
}
ValueType SemanticAnalyzer::expression(Expr& e) {
    switch(e.kind) {
        case ExprKind::integer: return ValueType::integer;
        case ExprKind::floating: return ValueType::floating;
        case ExprKind::string_literal: return ValueType::string;
        case ExprKind::boolean: return ValueType::boolean;
        case ExprKind::name: {
            const auto type=lookup(e.value);
            if(type==ValueType::unknown) error(e.location,"unknown name '"+e.value+"'");
            return type;
        }
        case ExprKind::call: {
            const auto fn=functions_.find(e.value);
            if(fn==functions_.end()) {
                error(e.location,"unknown function '"+e.value+"'");
                for(auto& arg:e.children) expression(*arg);
                return ValueType::unknown;
            }
            if(e.children.size()!=fn->second.parameters.size())
                error(e.location,"function '"+e.value+"' expects "+std::to_string(fn->second.parameters.size())+
                      " argument(s), but received "+std::to_string(e.children.size()));
            for(auto& arg:e.children) expression(*arg);
            return ValueType::unknown;
        }
        case ExprKind::unary: {
            if(e.children.size()!=1) { error(e.location,"invalid unary expression"); return ValueType::unknown; }
            const auto operand=expression(*e.children[0]);
            if(e.value=="-" && operand!=ValueType::integer && operand!=ValueType::floating && operand!=ValueType::unknown)
                error(e.location,"unary '-' requires a number");
            if(e.value=="not" && operand!=ValueType::boolean && operand!=ValueType::unknown)
                error(e.location,"'not' requires a boolean");
            return e.value=="not"?ValueType::boolean:operand;
        }
        case ExprKind::binary: {
            if(e.children.size()!=2) { error(e.location,"invalid binary expression"); return ValueType::unknown; }
            const auto left=expression(*e.children[0]);
            const auto right=expression(*e.children[1]);
            const auto numeric=[](ValueType t){return t==ValueType::integer || t==ValueType::floating;};
            if(e.value=="+" && left==ValueType::string && right==ValueType::string) return ValueType::string;
            if(e.value=="+" || e.value=="-" || e.value=="*" || e.value=="/" || e.value=="%") {
                if(!numeric(left) && left!=ValueType::unknown) error(e.location,"left operand of '"+e.value+"' must be numeric");
                if(!numeric(right) && right!=ValueType::unknown) error(e.location,"right operand of '"+e.value+"' must be numeric");
                return left==ValueType::floating || right==ValueType::floating?ValueType::floating:ValueType::integer;
            }
            if(e.value=="and" || e.value=="or") {
                if(left!=ValueType::boolean && left!=ValueType::unknown) error(e.location,"logical operators require boolean operands");
                if(right!=ValueType::boolean && right!=ValueType::unknown) error(e.location,"logical operators require boolean operands");
                return ValueType::boolean;
            }
            if(e.value=="==" || e.value=="!=" || e.value=="equal_equal" || e.value=="bang_equal") {
                if(left!=ValueType::unknown && right!=ValueType::unknown && left!=right)
                    error(e.location,"equality operands must have matching types");
                return ValueType::boolean;
            }
            if(e.value=="<" || e.value=="<=" || e.value==">" || e.value==">=" ||
               e.value=="less" || e.value=="less_equal" || e.value=="greater" || e.value=="greater_equal") {
                if((!numeric(left) && left!=ValueType::string && left!=ValueType::unknown) ||
                   (!numeric(right) && right!=ValueType::string && right!=ValueType::unknown))
                    error(e.location,"ordered comparisons require numbers or strings");
                if(left!=ValueType::unknown && right!=ValueType::unknown && left!=right)
                    error(e.location,"comparison operands must have matching types");
                return ValueType::boolean;
            }
            return ValueType::unknown;
        }
    }
    return ValueType::unknown;
}
void SemanticAnalyzer::statements(std::vector<std::unique_ptr<Stmt>>& body) {
    for(auto& ptr:body) {
        if(!ptr) continue;
        auto& s=*ptr;
        switch(s.kind) {
            case StmtKind::show:
            case StmtKind::expression:
                if(s.expression) expression(*s.expression);
                break;
            case StmtKind::give:
                if(!in_function_) error(s.location,"'give' can only be used inside a function");
                if(s.expression) expression(*s.expression);
                break;
            case StmtKind::function: {
                const auto found=functions_.find(s.name);
                if(found==functions_.end() || found->second.location.offset!=s.location.offset) break;
                const bool previous=in_function_; in_function_=true;
                scopes_.push_back({});
                for(const auto& p:s.parameters) {
                    if(scopes_.back().find(p)!=scopes_.back().end()) error(s.location,"duplicate parameter '"+p+"'");
                    else scopes_.back().emplace(p,ValueType::unknown);
                }
                statements(s.body);
                scopes_.pop_back(); in_function_=previous;
                break;
            }
            case StmtKind::if_statement: {
                if(s.expression) {
                    const auto t=expression(*s.expression);
                    if(t!=ValueType::boolean && t!=ValueType::unknown) error(s.expression->location,"if condition must be boolean");
                }
                scopes_.push_back({}); statements(s.body); scopes_.pop_back();
                scopes_.push_back({}); statements(s.else_body); scopes_.pop_back();
                break;
            }
            case StmtKind::repeat:
                if(s.expression) {
                    const auto t=expression(*s.expression);
                    if(t!=ValueType::integer && t!=ValueType::unknown) error(s.expression->location,"repeat count must be an integer");
                }
                scopes_.push_back({}); statements(s.body); scopes_.pop_back();
                break;
        }
    }
}
SemanticResult SemanticAnalyzer::analyze(std::vector<std::unique_ptr<Stmt>>& program) {
    functions_.clear(); scopes_.clear(); diagnostics_.clear(); in_function_=false;
    for(const auto& ptr:program) {
        if(!ptr || ptr->kind!=StmtKind::function) continue;
        const auto inserted=functions_.emplace(ptr->name,FunctionInfo{ptr->parameters,ptr->location});
        if(!inserted.second) error(ptr->location,"duplicate function '"+ptr->name+"'");
    }
    // Only the first declaration is analyzed; duplicate declarations are already diagnosed.
    statements(program);
    return SemanticResult{std::move(diagnostics_)};
}
} // namespace nexium::compiler
