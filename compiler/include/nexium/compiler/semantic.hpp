#pragma once
#include "nexium/compiler/parser.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace nexium::compiler {
enum class ValueType { unknown, integer, floating, string, boolean };
const char* value_type_name(ValueType type) noexcept;
struct SemanticResult {
    std::vector<Diagnostic> diagnostics;
    bool ok() const noexcept { return diagnostics.empty(); }
};
class SemanticAnalyzer {
public:
    SemanticResult analyze(std::vector<std::unique_ptr<Stmt>>& program);
private:
    struct FunctionInfo { std::vector<std::string> parameters; SourceLocation location{}; };
    std::unordered_map<std::string,FunctionInfo> functions_;
    std::vector<std::unordered_map<std::string,ValueType>> scopes_;
    std::vector<Diagnostic> diagnostics_;
    bool in_function_{false};
    void error(SourceLocation location,const std::string& message);
    ValueType expression(Expr& expr);
    void statements(std::vector<std::unique_ptr<Stmt>>& body);
    ValueType lookup(const std::string& name) const;
};
} // namespace nexium::compiler
