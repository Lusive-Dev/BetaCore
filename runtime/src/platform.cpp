#include "nexium/runtime.hpp"
#include <chrono>
#include <cstring>
#include <string>
#include <vector>
#include <filesystem>
#include <thread>
#include <limits>
#include <cstdio>
#include <climits>
#include <new>
#ifdef _WIN32
# define NOMINMAX
# include <winsock2.h>
# include <ws2tcpip.h>
# include <windows.h>
# include <bcrypt.h>
#else
# include <cerrno>
# include <fcntl.h>
# include <netdb.h>
# include <sys/socket.h>
# include <sys/types.h>
# include <sys/wait.h>
# include <signal.h>
# include <unistd.h>
# if defined(__linux__)
#  include <sys/random.h>
# endif
#endif

namespace nexium::runtime {
namespace {
thread_local Error tls_error{};
void set_error(ErrorCode code,const char* message) noexcept { tls_error={code,message}; }
#ifdef _WIN32
struct ProcessImpl { HANDLE process; HANDLE thread; };
using NativeSocket=SOCKET;
constexpr NativeSocket invalid_socket=INVALID_SOCKET;
#else
struct ProcessImpl { pid_t pid; };
using NativeSocket=int;
constexpr NativeSocket invalid_socket=-1;
#endif
struct SocketImpl { NativeSocket value{invalid_socket}; };
}
Error last_error() noexcept { return tls_error; }
void set_last_error(ErrorCode code,const char* message) noexcept { tls_error={code,message}; }
void clear_error() noexcept { tls_error={}; }

bool directory_create(const char* path) noexcept {
    if(!path || !*path) { set_error(ErrorCode::invalid_argument,"directory path is empty"); return false; }
    try {
        std::error_code ec;
        const auto native=std::filesystem::u8path(path);
        const bool created=std::filesystem::create_directories(native,ec);
        if(ec) { set_error(ErrorCode::io_failure,"could not create directory"); return false; }
        if(created || std::filesystem::is_directory(native,ec)) {
            if(ec) { set_error(ErrorCode::io_failure,"could not inspect directory"); return false; }
            clear_error(); return true;
        }
        set_error(ErrorCode::io_failure,"path is not a directory"); return false;
    } catch(...) { set_error(ErrorCode::out_of_memory,"directory path conversion failed"); return false; }
}
bool directory_exists(const char* path) noexcept {
    if(!path || !*path) { set_error(ErrorCode::invalid_argument,"directory path is empty"); return false; }
    try {
        std::error_code ec;
        const bool ok=std::filesystem::is_directory(std::filesystem::u8path(path),ec);
        if(ec) set_error(ErrorCode::io_failure,"could not inspect directory"); else clear_error();
        return ok;
    } catch(...) { set_error(ErrorCode::out_of_memory,"directory path conversion failed"); return false; }
}
bool file_copy(const char* source,const char* destination,bool overwrite) noexcept {
    if(!source || !*source || !destination || !*destination) { set_error(ErrorCode::invalid_argument,"invalid copy path"); return false; }
    try {
        std::error_code ec;
        auto options=overwrite?std::filesystem::copy_options::overwrite_existing:std::filesystem::copy_options::none;
        const bool ok=std::filesystem::copy_file(std::filesystem::u8path(source),std::filesystem::u8path(destination),options,ec);
        if(!ok || ec) { set_error(ErrorCode::io_failure,"file copy failed"); return false; }
        clear_error(); return true;
    } catch(...) { set_error(ErrorCode::out_of_memory,"file copy path conversion failed"); return false; }
}
bool file_move(const char* source,const char* destination,bool overwrite) noexcept {
    if(!source || !*source || !destination || !*destination) { set_error(ErrorCode::invalid_argument,"invalid move path"); return false; }
    try {
        auto src=std::filesystem::u8path(source), dst=std::filesystem::u8path(destination);
#ifdef _WIN32
        const DWORD flags=overwrite?MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH:MOVEFILE_WRITE_THROUGH;
        if(!MoveFileExW(src.c_str(),dst.c_str(),flags)) {
            set_error(GetLastError()==ERROR_ACCESS_DENIED?ErrorCode::permission_denied:ErrorCode::io_failure,"file move failed");
            return false;
        }
#else
        std::error_code ec;
        if(!overwrite && std::filesystem::exists(dst,ec)) {
            set_error(ErrorCode::io_failure,"destination already exists"); return false;
        }
        if(ec) { set_error(ErrorCode::io_failure,"could not inspect move destination"); return false; }
        std::filesystem::rename(src,dst,ec);
        if(ec) { set_error(ErrorCode::io_failure,"file move failed"); return false; }
#endif
        clear_error(); return true;
    } catch(...) { set_error(ErrorCode::out_of_memory,"file move path conversion failed"); return false; }
}
bool file_size(const char* path,std::uint64_t* size) noexcept {
    if(!path || !*path || !size) { set_error(ErrorCode::invalid_argument,"invalid file size arguments"); return false; }
    try {
        std::error_code ec; auto n=std::filesystem::file_size(std::filesystem::u8path(path),ec);
        if(ec) { set_error(ErrorCode::io_failure,"could not read file size"); return false; }
        *size=static_cast<std::uint64_t>(n); clear_error(); return true;
    } catch(...) { set_error(ErrorCode::out_of_memory,"file size path conversion failed"); return false; }
}

bool process_start(Process& process,const char* executable,const char* const* arguments,std::size_t count) noexcept {
    if(process.handle || !executable || !*executable || (count && !arguments)) { set_error(ErrorCode::invalid_argument,"invalid process start arguments"); return false; }
    try {
#ifdef _WIN32
    auto quote=[](const char* arg)->std::wstring {
        int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,arg,-1,nullptr,0);
        if(n<=0) return {};
        std::wstring w(static_cast<std::size_t>(n),L'\0');
        MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,arg,-1,w.data(),n); w.resize(static_cast<std::size_t>(n-1));
        std::wstring out=L"\""; std::size_t slashes=0;
        for(wchar_t ch:w) {
            if(ch==L'\\') { ++slashes; continue; }
            if(ch==L'"') { out.append(slashes*2+1,L'\\'); out.push_back(ch); slashes=0; continue; }
            out.append(slashes,L'\\'); slashes=0; out.push_back(ch);
        }
        out.append(slashes*2,L'\\'); out.push_back(L'"'); return out;
    };
    std::wstring exe=quote(executable);
    if(exe.empty()) { set_error(ErrorCode::invalid_argument,"executable is not valid UTF-8"); return false; }
    std::wstring cmd=exe;
    for(std::size_t i=0;i<count;++i) { if(!arguments[i]) { set_error(ErrorCode::invalid_argument,"null process argument"); return false; } auto a=quote(arguments[i]); if(a.empty()) { set_error(ErrorCode::invalid_argument,"argument is not valid UTF-8"); return false; } cmd+=L" "+a; }
    STARTUPINFOW si{}; si.cb=sizeof(si); PROCESS_INFORMATION pi{};
    if(!CreateProcessW(nullptr,cmd.data(),nullptr,nullptr,FALSE,CREATE_UNICODE_ENVIRONMENT,nullptr,nullptr,&si,&pi)) { set_error(ErrorCode::process_failure,"CreateProcessW failed"); return false; }
    auto* impl=new(std::nothrow) ProcessImpl{pi.hProcess,pi.hThread};
    if(!impl) { TerminateProcess(pi.hProcess,1); CloseHandle(pi.hThread); CloseHandle(pi.hProcess); set_error(ErrorCode::out_of_memory,"process handle allocation failed"); return false; }
    process.handle=impl; process.id=pi.dwProcessId;
#else
    std::vector<char*> argv; argv.reserve(count+2);
    argv.push_back(const_cast<char*>(executable));
    for(std::size_t i=0;i<count;++i) {
        if(!arguments[i]) { set_error(ErrorCode::invalid_argument,"null process argument"); return false; }
        argv.push_back(const_cast<char*>(arguments[i]));
    }
    argv.push_back(nullptr);
    auto* impl=new(std::nothrow) ProcessImpl{};
    if(!impl) { set_error(ErrorCode::out_of_memory,"process handle allocation failed"); return false; }
    pid_t pid=fork();
    if(pid<0) { delete impl; set_error(ErrorCode::process_failure,"fork failed"); return false; }
    if(pid==0) { execvp(executable,argv.data()); _exit(127); }
    impl->pid=pid; process.handle=impl; process.id=static_cast<std::uint32_t>(pid);
#endif
    clear_error(); return true;
    } catch(...) { set_error(ErrorCode::out_of_memory,"process argument preparation failed"); return false; }
}
bool process_wait(Process& process,std::uint64_t timeout_ms,std::int32_t* exit_code) noexcept {
    if(!process.handle || !exit_code) { set_error(ErrorCode::invalid_argument,"invalid process wait arguments"); return false; }
    auto* impl=static_cast<ProcessImpl*>(process.handle);
#ifdef _WIN32
    DWORD timeout=timeout_ms>MAXDWORD?MAXDWORD:static_cast<DWORD>(timeout_ms);
    DWORD result=WaitForSingleObject(impl->process,timeout);
    if(result==WAIT_TIMEOUT) { set_error(ErrorCode::timeout,"process wait timed out"); return false; }
    if(result!=WAIT_OBJECT_0) { set_error(ErrorCode::process_failure,"process wait failed"); return false; }
    DWORD code=0; if(!GetExitCodeProcess(impl->process,&code)) { set_error(ErrorCode::process_failure,"could not read process exit code"); return false; }
    *exit_code=static_cast<std::int32_t>(code);
#else
    const auto start=std::chrono::steady_clock::now(); int status=0;
    for(;;) {
        pid_t r=waitpid(impl->pid,&status,WNOHANG);
        if(r==impl->pid) break;
        if(r<0 && errno!=EINTR) { set_error(ErrorCode::process_failure,"waitpid failed"); return false; }
        if(timeout_ms==0 || std::chrono::steady_clock::now()-start>=std::chrono::milliseconds(timeout_ms)) { set_error(ErrorCode::timeout,"process wait timed out"); return false; }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    *exit_code=WIFEXITED(status)?WEXITSTATUS(status):(WIFSIGNALED(status)?128+WTERMSIG(status):-1);
#endif
    clear_error(); return true;
}
bool process_terminate(Process& process,std::uint32_t code) noexcept {
    if(!process.handle) { set_error(ErrorCode::invalid_argument,"process is not open"); return false; }
    auto* impl=static_cast<ProcessImpl*>(process.handle);
#ifdef _WIN32
    const bool ok=TerminateProcess(impl->process,code)!=0;
#else
    const bool ok=kill(impl->pid,SIGTERM)==0;
#endif
    if(!ok) { set_error(ErrorCode::process_failure,"process termination failed"); return false; }
    clear_error(); return true;
}
void process_close(Process& process) noexcept {
    if(!process.handle) return;
    auto* impl=static_cast<ProcessImpl*>(process.handle);
#ifdef _WIN32
    CloseHandle(impl->thread); CloseHandle(impl->process);
#endif
    delete impl; process.handle=nullptr; process.id=0;
}

bool network_initialize() noexcept {
#ifdef _WIN32
    WSADATA data{}; if(WSAStartup(MAKEWORD(2,2),&data)!=0) { set_error(ErrorCode::network_failure,"Winsock initialization failed"); return false; }
#endif
    clear_error(); return true;
}
void network_shutdown() noexcept {
#ifdef _WIN32
    WSACleanup();
#endif
}
bool tcp_connect(Socket& socket,const char* host,std::uint16_t port,std::uint64_t timeout_ms) noexcept {
    if(socket.handle!=-1 || !host || !*host || !port) { set_error(ErrorCode::invalid_argument,"invalid TCP connection arguments"); return false; }
    char service[6]; std::snprintf(service,sizeof(service),"%u",static_cast<unsigned>(port));
    addrinfo hints{}; hints.ai_family=AF_UNSPEC; hints.ai_socktype=SOCK_STREAM; hints.ai_protocol=IPPROTO_TCP;
    addrinfo* list=nullptr; if(getaddrinfo(host,service,&hints,&list)!=0) { set_error(ErrorCode::network_failure,"host name resolution failed"); return false; }
    bool connected=false; NativeSocket chosen=invalid_socket;
    for(addrinfo* p=list;p && !connected;p=p->ai_next) {
        NativeSocket s=::socket(p->ai_family,p->ai_socktype,p->ai_protocol); if(s==invalid_socket) continue;
#ifdef _WIN32
        u_long nonblock=1; ioctlsocket(s,FIONBIO,&nonblock);
        int rc=::connect(s,p->ai_addr,static_cast<int>(p->ai_addrlen));
        if(rc==SOCKET_ERROR && WSAGetLastError()==WSAEWOULDBLOCK) {
            fd_set wfds; FD_ZERO(&wfds); FD_SET(s,&wfds); timeval tv{static_cast<long>(timeout_ms/1000),static_cast<long>((timeout_ms%1000)*1000)};
            rc=select(0,nullptr,&wfds,nullptr,&tv);
            if(rc>0) { int err=0; int len=sizeof(err); getsockopt(s,SOL_SOCKET,SO_ERROR,reinterpret_cast<char*>(&err),&len); rc=err==0?0:SOCKET_ERROR; }
            else rc=SOCKET_ERROR;
        }
        nonblock=0; ioctlsocket(s,FIONBIO,&nonblock);
        if(rc==0) { chosen=s; connected=true; } else closesocket(s);
#else
        int flags=fcntl(s,F_GETFL,0); if(flags<0 || fcntl(s,F_SETFL,flags|O_NONBLOCK)<0) { close(s); continue; }
        int rc=::connect(s,p->ai_addr,p->ai_addrlen);
        if(rc<0 && errno==EINPROGRESS) {
            fd_set wfds; FD_ZERO(&wfds); FD_SET(s,&wfds); timeval tv{static_cast<time_t>(timeout_ms/1000),static_cast<suseconds_t>((timeout_ms%1000)*1000)};
            rc=select(s+1,nullptr,&wfds,nullptr,&tv);
            if(rc>0) { int err=0; socklen_t len=sizeof(err); getsockopt(s,SOL_SOCKET,SO_ERROR,&err,&len); rc=err==0?0:-1; } else rc=-1;
        }
        fcntl(s,F_SETFL,flags);
        if(rc==0) { chosen=s; connected=true; } else close(s);
#endif
    }
    freeaddrinfo(list);
    if(!connected) { set_error(ErrorCode::network_failure,"TCP connection failed or timed out"); return false; }
    socket.handle=static_cast<std::intptr_t>(chosen); clear_error(); return true;
}
bool tcp_listen(Socket& listener,const char* host,std::uint16_t port,std::uint16_t* bound_port,int backlog) noexcept {
    if(listener.handle!=-1 || !bound_port || backlog<1) { set_error(ErrorCode::invalid_argument,"invalid TCP listen arguments"); return false; }
    char service[6]; std::snprintf(service,sizeof(service),"%u",static_cast<unsigned>(port));
    addrinfo hints{}; hints.ai_family=AF_UNSPEC; hints.ai_socktype=SOCK_STREAM; hints.ai_protocol=IPPROTO_TCP; hints.ai_flags=AI_PASSIVE;
    addrinfo* list=nullptr;
    if(getaddrinfo((host && *host)?host:nullptr,service,&hints,&list)!=0) { set_error(ErrorCode::network_failure,"listen address resolution failed"); return false; }
    NativeSocket chosen=invalid_socket; std::uint16_t actual=0;
    for(addrinfo* p=list;p && chosen==invalid_socket;p=p->ai_next) {
        NativeSocket s=::socket(p->ai_family,p->ai_socktype,p->ai_protocol); if(s==invalid_socket) continue;
        int reuse=1;
#ifdef _WIN32
        setsockopt(s,SOL_SOCKET,SO_REUSEADDR,reinterpret_cast<const char*>(&reuse),sizeof(reuse));
#else
        setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
#endif
        if(::bind(s,p->ai_addr,static_cast<int>(p->ai_addrlen))!=0 || ::listen(s,backlog)!=0) {
#ifdef _WIN32
            closesocket(s);
#else
            close(s);
#endif
            continue;
        }
        sockaddr_storage address{};
#ifdef _WIN32
        int address_length=sizeof(address);
#else
        socklen_t address_length=sizeof(address);
#endif
        if(getsockname(s,reinterpret_cast<sockaddr*>(&address),&address_length)!=0) {
#ifdef _WIN32
            closesocket(s);
#else
            close(s);
#endif
            continue;
        }
        if(address.ss_family==AF_INET) actual=ntohs(reinterpret_cast<sockaddr_in*>(&address)->sin_port);
        else if(address.ss_family==AF_INET6) actual=ntohs(reinterpret_cast<sockaddr_in6*>(&address)->sin6_port);
        chosen=s;
    }
    freeaddrinfo(list);
    if(chosen==invalid_socket) { set_error(ErrorCode::network_failure,"could not bind TCP listener"); return false; }
    listener.handle=static_cast<std::intptr_t>(chosen); *bound_port=actual; clear_error(); return true;
}
bool tcp_accept(Socket& listener,Socket& client,std::uint64_t timeout_ms) noexcept {
    if(listener.handle==-1 || client.handle!=-1) { set_error(ErrorCode::invalid_argument,"invalid TCP accept arguments"); return false; }
    NativeSocket server=static_cast<NativeSocket>(listener.handle);
    fd_set readfds; FD_ZERO(&readfds); FD_SET(server,&readfds);
    const std::uint64_t capped=timeout_ms>static_cast<std::uint64_t>(LONG_MAX)*1000?static_cast<std::uint64_t>(LONG_MAX)*1000:timeout_ms;
    timeval tv{static_cast<long>(capped/1000),static_cast<long>((capped%1000)*1000)};
#ifdef _WIN32
    int ready=select(0,&readfds,nullptr,nullptr,&tv);
#else
    int ready=select(server+1,&readfds,nullptr,nullptr,&tv);
#endif
    if(ready<=0) { set_error(ready==0?ErrorCode::timeout:ErrorCode::network_failure,ready==0?"TCP accept timed out":"TCP listener wait failed"); return false; }
    NativeSocket accepted=::accept(server,nullptr,nullptr);
    if(accepted==invalid_socket) { set_error(ErrorCode::network_failure,"TCP accept failed"); return false; }
    client.handle=static_cast<std::intptr_t>(accepted); clear_error(); return true;
}

std::int64_t tcp_send(Socket& socket,const void* data,std::size_t size) noexcept {
    if(socket.handle==-1 || (!data && size)) { set_error(ErrorCode::invalid_argument,"invalid TCP send arguments"); return -1; }
#ifdef _WIN32
    int n=::send(static_cast<SOCKET>(socket.handle),static_cast<const char*>(data),static_cast<int>(size>INT_MAX?INT_MAX:size),0);
    if(n==SOCKET_ERROR) { set_error(ErrorCode::network_failure,"TCP send failed"); return -1; }
#else
# if defined(MSG_NOSIGNAL)
    constexpr int send_flags=MSG_NOSIGNAL;
# else
    constexpr int send_flags=0;
# endif
    const std::size_t bounded=size>static_cast<std::size_t>(SSIZE_MAX)?static_cast<std::size_t>(SSIZE_MAX):size;
    ssize_t n=::send(static_cast<int>(socket.handle),data,bounded,send_flags);
    if(n<0) { set_error(ErrorCode::network_failure,"TCP send failed"); return -1; }
#endif
    clear_error(); return static_cast<std::int64_t>(n);
}
std::int64_t tcp_receive(Socket& socket,void* data,std::size_t capacity) noexcept {
    if(socket.handle==-1 || (!data && capacity)) { set_error(ErrorCode::invalid_argument,"invalid TCP receive arguments"); return -1; }
#ifdef _WIN32
    int n=::recv(static_cast<SOCKET>(socket.handle),static_cast<char*>(data),static_cast<int>(capacity>INT_MAX?INT_MAX:capacity),0);
    if(n==SOCKET_ERROR) { set_error(ErrorCode::network_failure,"TCP receive failed"); return -1; }
#else
    ssize_t n=::recv(static_cast<int>(socket.handle),data,capacity,0);
    if(n<0) { set_error(ErrorCode::network_failure,"TCP receive failed"); return -1; }
#endif
    clear_error(); return static_cast<std::int64_t>(n);
}
void tcp_close(Socket& socket) noexcept {
    if(socket.handle==-1) return;
#ifdef _WIN32
    closesocket(static_cast<SOCKET>(socket.handle));
#else
    close(static_cast<int>(socket.handle));
#endif
    socket.handle=-1;
}
bool secure_random(void* destination,std::size_t size) noexcept {
    if(!destination && size) { set_error(ErrorCode::invalid_argument,"random destination is null"); return false; }
#ifdef _WIN32
    if(size>ULONG_MAX || BCryptGenRandom(nullptr,static_cast<PUCHAR>(destination),static_cast<ULONG>(size),BCRYPT_USE_SYSTEM_PREFERRED_RNG)!=0) { set_error(ErrorCode::unsupported,"system random generator failed"); return false; }
#else
    auto* out=static_cast<unsigned char*>(destination); std::size_t done=0;
# if defined(__linux__)
    while(done<size) {
        ssize_t n=getrandom(out+done,size-done,0);
        if(n<0 && errno==EINTR) continue;
        if(n<0) { set_error(ErrorCode::unsupported,"getrandom failed"); return false; }
        done+=static_cast<std::size_t>(n);
    }
# else
    int fd=open("/dev/urandom",O_RDONLY);
    if(fd<0) { set_error(ErrorCode::unsupported,"system random source unavailable"); return false; }
    while(done<size) {
        ssize_t n=read(fd,out+done,size-done);
        if(n<0 && errno==EINTR) continue;
        if(n<=0) { close(fd); set_error(ErrorCode::unsupported,"system random read failed"); return false; }
        done+=static_cast<std::size_t>(n);
    }
    close(fd);
# endif
#endif
    clear_error(); return true;
}
} // namespace nexium::runtime
