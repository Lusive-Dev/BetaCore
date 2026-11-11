#pragma once

#include "nexium/runtime.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <functional>
#include <limits>
#include <regex>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace nexium::stdlib {

using runtime::Array;

// Hosted bootstrap containers. The public Nexium API will remain independent of
// the host STL ABI; these templates are compiled into the caller.
template<class K, class V, class Hash=std::hash<K>, class Equal=std::equal_to<K>>
class Map {
public:
    bool insert_or_assign(K key, V value) noexcept {
        try { values_.insert_or_assign(std::move(key),std::move(value)); return true; }
        catch(...) { return false; }
    }
    V* find(const K& key) noexcept {
        auto it=values_.find(key); return it==values_.end()?nullptr:&it->second;
    }
    const V* find(const K& key) const noexcept {
        auto it=values_.find(key); return it==values_.end()?nullptr:&it->second;
    }
    bool erase(const K& key) noexcept { return values_.erase(key)!=0; }
    bool contains(const K& key) const noexcept { return values_.find(key)!=values_.end(); }
    void clear() noexcept { values_.clear(); }
    std::size_t size() const noexcept { return values_.size(); }
    bool empty() const noexcept { return values_.empty(); }
private:
    std::unordered_map<K,V,Hash,Equal> values_;
};

template<class T, class Hash=std::hash<T>, class Equal=std::equal_to<T>>
class Set {
public:
    bool insert(T value) noexcept {
        try { return values_.insert(std::move(value)).second; }
        catch(...) { return false; }
    }
    bool erase(const T& value) noexcept { return values_.erase(value)!=0; }
    bool contains(const T& value) const noexcept { return values_.find(value)!=values_.end(); }
    void clear() noexcept { values_.clear(); }
    std::size_t size() const noexcept { return values_.size(); }
    bool empty() const noexcept { return values_.empty(); }
private:
    std::unordered_set<T,Hash,Equal> values_;
};

template<class T>
class Queue {
public:
    bool push(T value) noexcept {
        try { values_.push_back(std::move(value)); return true; }
        catch(...) { return false; }
    }
    bool pop(T* output) noexcept {
        if(!output || values_.empty()) return false;
        try { *output=std::move(values_.front()); values_.pop_front(); return true; }
        catch(...) { return false; }
    }
    T* front() noexcept { return values_.empty()?nullptr:&values_.front(); }
    const T* front() const noexcept { return values_.empty()?nullptr:&values_.front(); }
    std::size_t size() const noexcept { return values_.size(); }
    bool empty() const noexcept { return values_.empty(); }
    void clear() noexcept { values_.clear(); }
private:
    std::deque<T> values_;
};

template<class T, class Compare=std::less<T>>
bool sort(Array<T>& values, Compare compare={}) noexcept {
    if(values.size()<2) return true;
    try { std::sort(values.data(),values.data()+values.size(),compare); return true; }
    catch(...) { return false; }
}
template<class T, class Compare=std::less<T>>
T* binary_search(Array<T>& values,const T& key,Compare compare={}) noexcept {
    if(values.empty()) return nullptr;
    T* first=values.data(); T* last=first+values.size();
    T* found=std::lower_bound(first,last,key,compare);
    return found!=last && !compare(key,*found)?found:nullptr;
}
template<class T, class Compare=std::less<T>>
const T* binary_search(const Array<T>& values,const T& key,Compare compare={}) noexcept {
    if(values.empty()) return nullptr;
    const T* first=values.data(); const T* last=first+values.size();
    const T* found=std::lower_bound(first,last,key,compare);
    return found!=last && !compare(key,*found)?found:nullptr;
}

namespace math {
inline constexpr double pi=3.141592653589793238462643383279502884;
inline constexpr double e=2.718281828459045235360287471352662498;
inline double abs(double x) noexcept { return std::fabs(x); }
inline double sqrt(double x) noexcept { return std::sqrt(x); }
inline double pow(double x,double y) noexcept { return std::pow(x,y); }
inline double exp(double x) noexcept { return std::exp(x); }
inline double log(double x) noexcept { return std::log(x); }
inline double log10(double x) noexcept { return std::log10(x); }
inline double sin(double x) noexcept { return std::sin(x); }
inline double cos(double x) noexcept { return std::cos(x); }
inline double tan(double x) noexcept { return std::tan(x); }
inline double asin(double x) noexcept { return std::asin(x); }
inline double acos(double x) noexcept { return std::acos(x); }
inline double atan2(double y,double x) noexcept { return std::atan2(y,x); }
inline double floor(double x) noexcept { return std::floor(x); }
inline double ceil(double x) noexcept { return std::ceil(x); }
inline double round(double x) noexcept { return std::round(x); }
inline double fmod(double x,double y) noexcept { return std::fmod(x,y); }
inline double min(double a,double b) noexcept { return a<b?a:b; }
inline double max(double a,double b) noexcept { return a>b?a:b; }
inline double clamp(double x,double low,double high) noexcept { return x<low?low:(x>high?high:x); }
inline bool is_finite(double x) noexcept { return std::isfinite(x); }
inline bool is_nan(double x) noexcept { return std::isnan(x); }
}

inline bool parse_integer(const char* text,std::int64_t* value,int base=10) noexcept {
    if(!text || !value || base<2 || base>36) return false;
    const char* end=text+std::strlen(text);
    auto result=std::from_chars(text,end,*value,base);
    return result.ec==std::errc{} && result.ptr==end;
}
inline bool parse_unsigned(const char* text,std::uint64_t* value,int base=10) noexcept {
    if(!text || !value || base<2 || base>36 || *text=='-') return false;
    const char* end=text+std::strlen(text);
    auto result=std::from_chars(text,end,*value,base);
    return result.ec==std::errc{} && result.ptr==end;
}
inline bool parse_float(const char* text,double* value) noexcept {
    if(!text || !value || !*text) return false;
    char* end=nullptr;
    const double parsed=std::strtod(text,&end);
    if(end==text || *end!='\0' || !std::isfinite(parsed)) return false;
    *value=parsed; return true;
}
inline bool format_integer(std::int64_t value,char* output,std::size_t capacity,std::size_t* required) noexcept {
    if(!output || capacity==0) return false;
    const int n=std::snprintf(output,capacity,"%lld",static_cast<long long>(value));
    if(n<0) return false;
    if(required) *required=static_cast<std::size_t>(n)+1;
    return static_cast<std::size_t>(n)<capacity;
}
inline bool format_float(double value,char* output,std::size_t capacity,int precision=6,std::size_t* required=nullptr) noexcept {
    if(!output || capacity==0 || precision<0 || precision>17 || !std::isfinite(value)) return false;
    const int n=std::snprintf(output,capacity,"%.*g",precision,value);
    if(n<0) return false;
    if(required) *required=static_cast<std::size_t>(n)+1;
    return static_cast<std::size_t>(n)<capacity;
}

struct DateTime {
    int year{0}, month{0}, day{0}, hour{0}, minute{0}, second{0}, weekday{0}, yearday{0};
};
inline bool datetime_utc(std::int64_t unix_seconds,DateTime* out) noexcept {
    if(!out) return false;
    const std::time_t stamp=static_cast<std::time_t>(unix_seconds);
    if(static_cast<std::int64_t>(stamp)!=unix_seconds) return false;
    std::tm parts{};
#ifdef _WIN32
    if(gmtime_s(&parts,&stamp)!=0) return false;
#else
    if(!gmtime_r(&stamp,&parts)) return false;
#endif
    out->year=parts.tm_year+1900; out->month=parts.tm_mon+1; out->day=parts.tm_mday;
    out->hour=parts.tm_hour; out->minute=parts.tm_min; out->second=parts.tm_sec;
    out->weekday=parts.tm_wday; out->yearday=parts.tm_yday; return true;
}

inline bool regex_match(const char* text,const char* pattern,bool* matched) noexcept {
    if(!text || !pattern || !matched) return false;
    try { *matched=std::regex_match(text,std::regex(pattern,std::regex::ECMAScript)); return true; }
    catch(...) { return false; }
}
inline bool regex_search(const char* text,const char* pattern,bool* matched) noexcept {
    if(!text || !pattern || !matched) return false;
    try { *matched=std::regex_search(text,std::regex(pattern,std::regex::ECMAScript)); return true; }
    catch(...) { return false; }
}

} // namespace nexium::stdlib
