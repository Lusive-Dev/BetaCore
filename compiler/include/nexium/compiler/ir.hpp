#pragma once
#include "nexium/compiler/parser.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace nexium::compiler {
enum class IROp {
    constant_integer, constant_floating, constant_string, constant_boolean,
    move, negate, logical_not, add, subtract, multiply, divide, modulo,
    equal, not_equal, less, less_equal, greater, greater_equal, logical_and, logical_or,
    call, print, return_value, label, jump, branch, less_than
};
struct IRInstruction {
    IROp op{IROp::constant_integer};
    std::uint32_t destination{0};
    std::uint32_t left{0};
    std::uint32_t right{0};
    std::string text;
    std::vector<std::uint32_t> arguments;
    std::int64_t integer{0};
    double floating{0.0};
    std::uint32_t target{0};
    std::uint32_t alternate{0};
    SourceLocation location{};
};
struct IRFunction {
    std::string name;
    std::vector<std::string> parameters;
    std::vector<IRInstruction> instructions;
    std::uint32_t register_count{0};
};
struct IRResult {
    std::vector<IRFunction> functions;
    std::vector<Diagnostic> diagnostics;
    bool ok() const noexcept { return diagnostics.empty(); }
};
class IRLowerer {
public:
    IRResult lower(const std::vector<std::unique_ptr<Stmt>>& program);
};
const char* ir_op_name(IROp op) noexcept;
} // namespace nexium::compiler
