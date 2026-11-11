#include "nexium/compiler/ir.hpp"
#include <cstdlib>
#include <limits>
#include <unordered_map>
#include <utility>

namespace nexium::compiler {
const char* ir_op_name(IROp op) noexcept {
    switch(op) {
#define NEXIUM_IR_NAME(x) case IROp::x:return #x
        NEXIUM_IR_NAME(constant_integer); NEXIUM_IR_NAME(constant_floating); NEXIUM_IR_NAME(constant_string);
        NEXIUM_IR_NAME(constant_boolean); NEXIUM_IR_NAME(move); NEXIUM_IR_NAME(negate); NEXIUM_IR_NAME(logical_not);
        NEXIUM_IR_NAME(add); NEXIUM_IR_NAME(subtract); NEXIUM_IR_NAME(multiply); NEXIUM_IR_NAME(divide);
        NEXIUM_IR_NAME(modulo); NEXIUM_IR_NAME(equal); NEXIUM_IR_NAME(not_equal); NEXIUM_IR_NAME(less);
        NEXIUM_IR_NAME(less_equal); NEXIUM_IR_NAME(greater); NEXIUM_IR_NAME(greater_equal);
        NEXIUM_IR_NAME(logical_and); NEXIUM_IR_NAME(logical_or); NEXIUM_IR_NAME(call); NEXIUM_IR_NAME(print);
        NEXIUM_IR_NAME(return_value); NEXIUM_IR_NAME(label); NEXIUM_IR_NAME(jump); NEXIUM_IR_NAME(branch);
        NEXIUM_IR_NAME(less_than);
#undef NEXIUM_IR_NAME
    }
    return "unknown";
}
namespace {
class FunctionLowerer {
public:
    IRFunction function;
    std::vector<Diagnostic>& diagnostics;
    std::unordered_map<std::string,std::uint32_t> names;
    std::uint32_t next_label{1};
    FunctionLowerer(std::string name,std::vector<Diagnostic>& d):diagnostics(d) { function.name=std::move(name); }
    std::uint32_t reg() { return function.register_count++; }
    std::uint32_t label() { return next_label++; }
    void emit(IRInstruction i) { function.instructions.push_back(std::move(i)); }
    void mark(std::uint32_t id,SourceLocation l={}) { IRInstruction i; i.op=IROp::label; i.target=id; i.location=l; emit(std::move(i)); }
    std::uint32_t expr(const Expr& e) {
        const auto dst=reg(); IRInstruction i; i.destination=dst; i.location=e.location;
        switch(e.kind) {
            case ExprKind::integer:
                i.op=IROp::constant_integer;
                try { i.integer=std::stoll(e.value); } catch(...) { diagnostics.push_back({e.location,"integer literal is outside the supported 64-bit range"}); }
                emit(std::move(i)); return dst;
            case ExprKind::floating:
                i.op=IROp::constant_floating;
                try { i.floating=std::stod(e.value); } catch(...) { diagnostics.push_back({e.location,"invalid floating-point literal"}); }
                emit(std::move(i)); return dst;
            case ExprKind::string_literal: i.op=IROp::constant_string; i.text=e.value; emit(std::move(i)); return dst;
            case ExprKind::boolean: i.op=IROp::constant_boolean; i.integer=e.value=="true"?1:0; emit(std::move(i)); return dst;
            case ExprKind::name: {
                const auto found=names.find(e.value);
                if(found==names.end()) diagnostics.push_back({e.location,"cannot lower unresolved name '"+e.value+"'"});
                else { i.op=IROp::move; i.left=found->second; emit(std::move(i)); }
                return dst;
            }
            case ExprKind::call:
                i.op=IROp::call; i.text=e.value;
                for(const auto& child:e.children) i.arguments.push_back(expr(*child));
                emit(std::move(i)); return dst;
            case ExprKind::unary:
                if(e.children.size()!=1) { diagnostics.push_back({e.location,"malformed unary expression"}); return dst; }
                i.left=expr(*e.children[0]);
                i.op=e.value=="not"?IROp::logical_not:IROp::negate; emit(std::move(i)); return dst;
            case ExprKind::binary: {
                if(e.children.size()!=2) { diagnostics.push_back({e.location,"malformed binary expression"}); return dst; }
                i.left=expr(*e.children[0]); i.right=expr(*e.children[1]);
                const std::string& op=e.value;
                if(op=="+") i.op=IROp::add; else if(op=="-") i.op=IROp::subtract;
                else if(op=="*") i.op=IROp::multiply; else if(op=="/") i.op=IROp::divide;
                else if(op=="%") i.op=IROp::modulo; else if(op=="==") i.op=IROp::equal;
                else if(op=="!=") i.op=IROp::not_equal; else if(op=="<") i.op=IROp::less;
                else if(op=="<=") i.op=IROp::less_equal; else if(op==">") i.op=IROp::greater;
                else if(op==">=") i.op=IROp::greater_equal; else if(op=="and") i.op=IROp::logical_and;
                else if(op=="or") i.op=IROp::logical_or;
                else { diagnostics.push_back({e.location,"unsupported binary operator '"+op+"'"}); return dst; }
                emit(std::move(i)); return dst;
            }
        }
        return dst;
    }
    void body(const std::vector<std::unique_ptr<Stmt>>& statements) {
        for(const auto& ptr:statements) {
            if(!ptr) continue;
            const auto& s=*ptr;
            switch(s.kind) {
                case StmtKind::show: {
                    if(!s.expression) break;
                    IRInstruction i; i.op=IROp::print; i.left=expr(*s.expression); i.location=s.location; emit(std::move(i)); break;
                }
                case StmtKind::expression:
                    if(s.expression) (void)expr(*s.expression);
                    break;
                case StmtKind::give: {
                    IRInstruction i; i.op=IROp::return_value; i.left=s.expression?expr(*s.expression):0; i.location=s.location; emit(std::move(i)); break;
                }
                case StmtKind::if_statement: {
                    if(!s.expression) break;
                    const auto condition=expr(*s.expression), otherwise=label(), then_body=label(), done=label();
                    IRInstruction branch; branch.op=IROp::branch; branch.left=condition; branch.target=otherwise; branch.alternate=then_body; branch.location=s.location; emit(std::move(branch));
                    mark(then_body,s.location); body(s.body);
                    IRInstruction jump; jump.op=IROp::jump; jump.target=done; emit(std::move(jump));
                    mark(otherwise,s.location); body(s.else_body); mark(done,s.location); break;
                }
                case StmtKind::repeat: {
                    if(!s.expression) break;
                    const auto count=expr(*s.expression), index=reg(), start=label(), done=label();
                    IRInstruction zero; zero.op=IROp::constant_integer; zero.destination=index; zero.integer=0; emit(std::move(zero));
                    mark(start,s.location);
                    const auto test=reg(), loop_body=label(); IRInstruction cmp; cmp.op=IROp::less_than; cmp.destination=test; cmp.left=index; cmp.right=count; emit(std::move(cmp));
                    IRInstruction branch; branch.op=IROp::branch; branch.left=test; branch.target=done; branch.alternate=loop_body; emit(std::move(branch));
                    mark(loop_body,s.location); body(s.body);
                    IRInstruction one; one.op=IROp::constant_integer; one.destination=reg(); one.integer=1; const auto one_reg=one.destination; emit(std::move(one));
                    IRInstruction inc; inc.op=IROp::add; inc.destination=index; inc.left=index; inc.right=one_reg; emit(std::move(inc));
                    IRInstruction jump; jump.op=IROp::jump; jump.target=start; emit(std::move(jump));
                    mark(done,s.location); break;
                }
                case StmtKind::function: break;
            }
        }
    }
};
}
IRResult IRLowerer::lower(const std::vector<std::unique_ptr<Stmt>>& program) {
    IRResult result;
    FunctionLowerer main("main",result.diagnostics);
    std::vector<const Stmt*> functions;
    for(const auto& ptr:program) if(ptr && ptr->kind==StmtKind::function) functions.push_back(ptr.get());
    main.body(program);
    IRInstruction ret; ret.op=IROp::return_value; ret.integer=0; main.emit(std::move(ret));
    result.functions.push_back(std::move(main.function));
    for(const auto* s:functions) {
        FunctionLowerer fn(s->name,result.diagnostics);
        fn.function.parameters=s->parameters;
        for(const auto& p:s->parameters) fn.names.emplace(p,fn.reg());
        fn.body(s->body);
        IRInstruction ret_default; ret_default.op=IROp::return_value; ret_default.integer=0; ret_default.location=s->location;
        fn.emit(std::move(ret_default));
        result.functions.push_back(std::move(fn.function));
    }
    return result;
}
} // namespace nexium::compiler
