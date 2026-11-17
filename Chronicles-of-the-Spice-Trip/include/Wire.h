// ============================================================================
//  Wire.h — writing game state into bytes for the network, and reading it back.
//
//  Windows and Android must read each other's bytes, so nothing is sent as a
//  whole struct that holds a pointer or a string (their sizes differ between
//  64-bit and 32-bit builds). Plain data of floats, ints and bools is the same
//  everywhere, and goes as it is; the rest is written field by field.
//
//  io(s, thing) functions take a Writer or a Reader, so one list of fields
//  serves both ways: s(a, b, c) writes them, or reads them back into place.
// ============================================================================
#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace wire {

    // A name the other side sent, kept for good so a const char* can point at it.
    inline const char* intern(const std::string& s) {
        static std::unordered_set<std::string> pool;
        return pool.insert(s).first->c_str();
    }

    template <class T>
    constexpr bool plain = std::is_trivially_copyable_v<T> && !std::is_pointer_v<T>;

    struct Writer {
        static constexpr bool reading = false;
        std::vector<uint8_t>& b;
        explicit Writer(std::vector<uint8_t>& into) : b(into) {}

        void bytes(const void* p, size_t n) { const uint8_t* s = (const uint8_t*)p; b.insert(b.end(), s, s + n); }
        template <class T> void raw(const T& v) { static_assert(plain<T>, "pointers and strings go field by field"); bytes(&v, sizeof v); }
        template <class... T> void operator()(T&... v) { (raw(v), ...); }
        void u8(int v) { b.push_back((uint8_t)v); }
        void u16(int v) { uint16_t x = (uint16_t)v; raw(x); }
        void i32(int v) { int32_t x = v; raw(x); }
        void text(std::string& s) { size_t n = s.size() < 65535 ? s.size() : 65535; u16((int)n); bytes(s.data(), n); }
        void name(const char*& s) {                 // a const char* (nullptr allowed)
            u8(s ? 1 : 0);
            if (s) { std::string t = s; text(t); }
        }
        template <class T, class F> void list(std::vector<T>& v, F each) {
            size_t n = v.size() < 65535 ? v.size() : 65535;
            u16((int)n);
            for (size_t i = 0; i < n; ++i) each(v[i]);
        }
        template <class T> void list(std::vector<T>& v) { list(v, [&](T& x) { raw(x); }); }
    };

    struct Reader {
        static constexpr bool reading = true;
        const uint8_t* p;
        const uint8_t* end;
        bool ok = true;
        Reader(const void* data, size_t size) : p((const uint8_t*)data), end((const uint8_t*)data + size) {}
        explicit Reader(const std::vector<uint8_t>& v) : Reader(v.data(), v.size()) {}

        bool bytes(void* out, size_t n) {
            if (!ok || (size_t)(end - p) < n) { ok = false; memset(out, 0, n); return false; }
            memcpy(out, p, n); p += n;
            return true;
        }
        template <class T> void raw(T& v) { static_assert(plain<T>, "pointers and strings go field by field"); bytes(&v, sizeof v); }
        template <class... T> void operator()(T&... v) { (raw(v), ...); }
        int u8() { uint8_t x = 0; raw(x); return x; }
        int u16() { uint16_t x = 0; raw(x); return x; }
        int i32() { int32_t x = 0; raw(x); return x; }
        void u8(int& v) { v = u8(); }
        void text(std::string& s) {
            size_t n = (size_t)u16();
            if (!ok || (size_t)(end - p) < n) { ok = false; s.clear(); return; }
            s.assign((const char*)p, n); p += n;
        }
        void name(const char*& s) {
            if (!u8()) { s = nullptr; return; }
            std::string t; text(t);
            s = ok ? intern(t) : nullptr;
        }
        template <class T, class F> void list(std::vector<T>& v, F each) {
            size_t n = (size_t)u16();
            v.clear();
            if (!ok) return;
            v.resize(n);
            for (size_t i = 0; i < n && ok; ++i) each(v[i]);
            if (!ok) v.clear();
        }
        template <class T> void list(std::vector<T>& v) { list(v, [&](T& x) { raw(x); }); }
        bool done() const { return ok && p == end; }
    };

} // namespace wire
