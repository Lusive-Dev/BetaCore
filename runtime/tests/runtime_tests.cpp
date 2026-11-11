#include "nexium/runtime.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <utility>

using namespace nexium::runtime;
int main() {
    assert(initialize());
    assert(is_initialized());
    void* p=allocate(128,16);
    assert(p && reinterpret_cast<std::uintptr_t>(p)%16==0);
    deallocate(p,16);
    String a("Hello");
    assert(a.size()==5);
    assert(a.append(", Nexium",8));
    assert(std::strcmp(a.c_str(),"Hello, Nexium")==0);
    String b=a;
    assert(b.equals(a));
    String c=std::move(b);
    assert(c.equals(a));
    Array<int> values;
    for (int i=0;i<100;++i) assert(values.push_back(i));
    assert(values.size()==100 && values[0]==0 && values[99]==99);
    assert(values.at(100)==nullptr);
    values.pop_back();
    assert(values.size()==99);
    assert(write_line("Nexium runtime tests passed."));
    shutdown();
    assert(!is_initialized());
    return 0;
}
