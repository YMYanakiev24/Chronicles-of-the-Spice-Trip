// ============================================================================
//  Net.cpp — sockets for co-op on the local network (see Net.h).
//  Winsock on Windows, BSD sockets on Android and Linux; none in a browser.
// ============================================================================
#include "Net.h"
#include <chrono>
#include <cstring>

double net::now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

#if defined(PLATFORM_WEB)
// ---------------------------------------------------------------- a browser: no network play
namespace net {
    bool available() { return false; }
    bool Link::connect(const std::string&, int) { state = State::Failed; return false; }
    void Link::update() {}
    void Link::send(uint8_t, const void*, size_t) {}
    bool Link::receive(Message&) { return false; }
    void Link::close() { state = State::Closed; }
    void Link::adopt(intptr_t, const std::string&) {}
    void Link::fail() { state = State::Failed; }
    bool Server::start(int) { return false; }
    void Server::stop() {}
    void Server::advertise(const std::string&, int, int, bool) {}
    bool Server::accept(Link&) { return false; }
    void Server::update() {}
    bool Finder::start() { return false; }
    void Finder::stop() {}
    void Finder::update() {}
    std::string localAddress() { return ""; }
    void holdMulticastLock(bool) {}
}
#else

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using sock_t = SOCKET;
static int lastError() { return WSAGetLastError(); }
static bool wouldBlock(int e) { return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS || e == WSAEALREADY; }
static void closeSock(intptr_t s) { closesocket((SOCKET)s); }
static void setNonBlocking(intptr_t s) { u_long on = 1; ioctlsocket((SOCKET)s, FIONBIO, &on); }
static constexpr int SEND_FLAGS = 0;
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using sock_t = int;
static int lastError() { return errno; }
static bool wouldBlock(int e) { return e == EWOULDBLOCK || e == EAGAIN || e == EINPROGRESS || e == EALREADY; }
static void closeSock(intptr_t s) { ::close((int)s); }
static void setNonBlocking(intptr_t s) { fcntl((int)s, F_SETFL, fcntl((int)s, F_GETFL, 0) | O_NONBLOCK); }
static constexpr int SEND_FLAGS = MSG_NOSIGNAL;
#endif

namespace net {

    namespace {
        constexpr uint16_t FIND_VERSION = 1;
        const char ASK[] = "SPICE?", ANSWER[] = "SPICE!";

        bool ready() {
#ifdef _WIN32
            static bool started = false;
            if (!started) {
                WSADATA wsa;
                started = WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
            }
            return started;
#else
            return true;
#endif
        }

        void noDelay(intptr_t s) {
            int on = 1;
            setsockopt((sock_t)s, IPPROTO_TCP, TCP_NODELAY, (const char*)&on, sizeof on);
        }

        std::string addressOf(const sockaddr_in& a) {
            char buf[64] = {};
            inet_ntop(AF_INET, (void*)&a.sin_addr, buf, sizeof buf);
            return buf;
        }

        bool resolve(const std::string& host, int port, sockaddr_in& out) {
            memset(&out, 0, sizeof out);
            out.sin_family = AF_INET;
            out.sin_port = htons((uint16_t)port);
            if (inet_pton(AF_INET, host.c_str(), &out.sin_addr) == 1) return true;
            addrinfo hints{}, * res = nullptr;
            hints.ai_family = AF_INET;
            if (getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0 || !res) return false;
            out.sin_addr = ((sockaddr_in*)res->ai_addr)->sin_addr;
            freeaddrinfo(res);
            return true;
        }

        void put16(std::vector<uint8_t>& b, uint16_t v) { b.push_back(v & 0xff); b.push_back(v >> 8); }
        void put32(std::vector<uint8_t>& b, uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back((v >> (8 * i)) & 0xff); }
        uint16_t get16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
        uint32_t get32(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

        uint32_t hostId() {                 // tells one host's answers apart from another's
            static uint32_t id = (uint32_t)(now() * 1000003.0) ^ 0x5bd1e995u;
            return id;
        }
    }

    bool available() { return true; }

#ifndef PLATFORM_ANDROID
    void holdMulticastLock(bool) {}         // only Android filters broadcasts
#endif

    // ---------------------------------------------------------------- Link
    bool Link::connect(const std::string& address, int port) {
        close();
        if (!ready()) { state = State::Failed; return false; }
        std::string host = address;
        size_t colon = host.find(':');
        if (colon != std::string::npos) { port = atoi(host.c_str() + colon + 1); host.resize(colon); }
        sockaddr_in to;
        if (!resolve(host, port, to)) { state = State::Failed; return false; }
        sock_t s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if ((intptr_t)s < 0) { state = State::Failed; return false; }
        sock = (intptr_t)s;
        setNonBlocking(sock);
        noDelay(sock);
        peer = host;
        started = now();
        out.clear(); in.clear(); outAt = 0;
        if (::connect(s, (sockaddr*)&to, sizeof to) == 0) { state = State::Open; return true; }
        if (!wouldBlock(lastError())) { fail(); return false; }
        state = State::Connecting;
        return true;
    }

    void Link::adopt(intptr_t socket, const std::string& address) {
        close();
        sock = socket;
        peer = address;
        setNonBlocking(sock);
        noDelay(sock);
        out.clear(); in.clear(); outAt = 0;
        state = State::Open;
    }

    void Link::fail() {
        if (sock != -1) closeSock(sock);
        sock = -1;
        state = State::Failed;
    }

    void Link::close() {
        if (sock != -1) closeSock(sock);
        sock = -1;
        if (state != State::Idle) state = State::Closed;
        out.clear(); in.clear(); outAt = 0;
    }

    void Link::update() {
        if (state == State::Connecting) {
            fd_set w, e;
            FD_ZERO(&w); FD_ZERO(&e);
            FD_SET((sock_t)sock, &w); FD_SET((sock_t)sock, &e);
            timeval tv{ 0, 0 };
            int n = select((int)sock + 1, nullptr, &w, &e, &tv);
            if (n > 0) {
                int err = 0;
                socklen_t len = sizeof err;
                getsockopt((sock_t)sock, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
                if (err || FD_ISSET((sock_t)sock, &e)) { fail(); return; }
                state = State::Open;
            }
            else if (now() - started > 6) { fail(); return; }   // nobody there
            else return;
        }
        if (state != State::Open) return;
        while (outAt < out.size()) {
            int n = ::send((sock_t)sock, (const char*)out.data() + outAt, (int)(out.size() - outAt), SEND_FLAGS);
            if (n > 0) { outAt += n; continue; }
            if (n < 0 && wouldBlock(lastError())) break;
            fail(); return;
        }
        if (outAt == out.size()) { out.clear(); outAt = 0; }
        else if (outAt > (1 << 20)) { out.erase(out.begin(), out.begin() + outAt); outAt = 0; }
        uint8_t buf[16384];
        for (;;) {
            int n = recv((sock_t)sock, (char*)buf, sizeof buf, 0);
            if (n > 0) { in.insert(in.end(), buf, buf + n); continue; }
            if (n < 0 && wouldBlock(lastError())) break;
            fail(); return;                 // 0: they hung up
        }
    }

    void Link::send(uint8_t type, const void* data, size_t size) {
        if (state != State::Open && state != State::Connecting) return;
        put32(out, (uint32_t)size);
        out.push_back(type);
        const uint8_t* p = (const uint8_t*)data;
        out.insert(out.end(), p, p + size);
    }

    bool Link::receive(Message& m) {
        if (in.size() < 5) return false;
        uint32_t size = get32(in.data());
        if (size > MAX_MESSAGE) { fail(); return false; }
        if (in.size() < 5 + (size_t)size) return false;
        m.type = in[4];
        m.data.assign(in.begin() + 5, in.begin() + 5 + size);
        in.erase(in.begin(), in.begin() + 5 + size);
        return true;
    }

    // ---------------------------------------------------------------- Server
    bool Server::start(int p) {
        stop();
        if (!ready()) return false;
        port = p;
        sock_t s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if ((intptr_t)s < 0) return false;
        int on = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof on);
        sockaddr_in at{};
        at.sin_family = AF_INET; at.sin_port = htons((uint16_t)port); at.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(s, (sockaddr*)&at, sizeof at) != 0 || listen(s, 4) != 0) { closeSock((intptr_t)s); return false; }
        listener = (intptr_t)s;
        setNonBlocking(listener);

        sock_t u = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);   // answers "who's hosting?"
        if ((intptr_t)u >= 0) {
            setsockopt(u, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof on);
            setsockopt(u, SOL_SOCKET, SO_BROADCAST, (const char*)&on, sizeof on);
            sockaddr_in fa{};
            fa.sin_family = AF_INET; fa.sin_port = htons(FIND_PORT); fa.sin_addr.s_addr = htonl(INADDR_ANY);
            if (bind(u, (sockaddr*)&fa, sizeof fa) == 0) { finder = (intptr_t)u; setNonBlocking(finder); }
            else closeSock((intptr_t)u);     // someone else answers on this machine: joining by address still works
        }
        holdMulticastLock(true);
        return true;
    }

    void Server::stop() {
        if (listener != -1) closeSock(listener);
        if (finder != -1) closeSock(finder);
        if (listener != -1) holdMulticastLock(false);
        listener = finder = -1;
    }

    void Server::advertise(const std::string& n, int p, int m, bool o) { name = n; players = p; max = m; open = o; }

    bool Server::accept(Link& into) {
        if (listener == -1) return false;
        sockaddr_in from{};
        socklen_t len = sizeof from;
        sock_t s = ::accept((sock_t)listener, (sockaddr*)&from, &len);
        if ((intptr_t)s < 0) return false;
        into.adopt((intptr_t)s, addressOf(from));
        return true;
    }

    void Server::update() {
        if (finder == -1) return;
        uint8_t buf[256];
        for (;;) {
            sockaddr_in from{};
            socklen_t len = sizeof from;
            int n = recvfrom((sock_t)finder, (char*)buf, sizeof buf, 0, (sockaddr*)&from, &len);
            if (n <= 0) break;
            if (n < (int)sizeof ASK - 1 + 2 || memcmp(buf, ASK, sizeof ASK - 1) != 0) continue;
            if (get16(buf + sizeof ASK - 1) != FIND_VERSION) continue;
            std::vector<uint8_t> a(ANSWER, ANSWER + sizeof ANSWER - 1);
            put16(a, FIND_VERSION);
            put32(a, hostId());
            put16(a, (uint16_t)port);
            a.push_back((uint8_t)players); a.push_back((uint8_t)max); a.push_back(open ? 1 : 0);
            std::string nm = name.substr(0, 40);
            a.insert(a.end(), nm.begin(), nm.end());
            sendto((sock_t)finder, (const char*)a.data(), (int)a.size(), 0, (sockaddr*)&from, len);
        }
    }

    // ---------------------------------------------------------------- Finder
    bool Finder::start() {
        stop();
        if (!ready()) return false;
        sock_t u = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if ((intptr_t)u < 0) return false;
        int on = 1;
        setsockopt(u, SOL_SOCKET, SO_BROADCAST, (const char*)&on, sizeof on);
        sockaddr_in at{};
        at.sin_family = AF_INET; at.sin_port = 0; at.sin_addr.s_addr = htonl(INADDR_ANY);
        bind(u, (sockaddr*)&at, sizeof at);
        sock = (intptr_t)u;
        setNonBlocking(sock);
        lastAsk = -10;
        found.clear();
        holdMulticastLock(true);
        return true;
    }

    void Finder::stop() {
        if (sock != -1) { closeSock(sock); holdMulticastLock(false); }
        sock = -1;
    }

    void Finder::update() {
        if (sock == -1) return;
        double t = now();
        if (t - lastAsk > 1.0) {             // ask the whole network, the likely subnet, and this machine
            lastAsk = t;
            std::vector<uint8_t> q(ASK, ASK + sizeof ASK - 1);
            put16(q, FIND_VERSION);
            std::vector<std::string> to = { "255.255.255.255", "127.0.0.1" };
            std::string me = localAddress();
            size_t dot = me.rfind('.');
            if (dot != std::string::npos) to.push_back(me.substr(0, dot) + ".255");
            for (auto& a : to) {
                sockaddr_in addr;
                if (resolve(a, FIND_PORT, addr))
                    sendto((sock_t)sock, (const char*)q.data(), (int)q.size(), 0, (sockaddr*)&addr, sizeof addr);
            }
        }
        uint8_t buf[256];
        for (;;) {
            sockaddr_in from{};
            socklen_t len = sizeof from;
            int n = recvfrom((sock_t)sock, (char*)buf, sizeof buf, 0, (sockaddr*)&from, &len);
            if (n <= 0) break;
            const int head = sizeof ANSWER - 1;
            if (n < head + 2 + 4 + 2 + 3 || memcmp(buf, ANSWER, head) != 0 || get16(buf + head) != FIND_VERSION) continue;
            Found f;
            f.id = get32(buf + head + 2);
            int port = get16(buf + head + 6);
            f.players = buf[head + 8]; f.max = buf[head + 9]; f.open = buf[head + 10] != 0;
            f.name.assign((const char*)buf + head + 11, (const char*)buf + n);
            std::string ip = addressOf(from);
            f.address = port == GAME_PORT ? ip : ip + ":" + std::to_string(port);
            f.seen = t;
            auto loopback = [](const std::string& a) { return a.rfind("127.", 0) == 0; };
            bool known = false;
            for (auto& g : found) {
                if (g.id != f.id) continue;
                // the same host over the network and on this machine: keep the network address
                std::string keep = loopback(f.address) && !loopback(g.address) ? g.address : f.address;
                g = f; g.address = keep; known = true;
                break;
            }
            if (!known) found.push_back(f);
        }
        for (size_t i = 0; i < found.size();)
            if (t - found[i].seen > 3.5) found.erase(found.begin() + i); else ++i;
    }

    // ---------------------------------------------------------------- this device
    std::string localAddress() {
        if (!ready()) return "";
        // A UDP socket "connected" anywhere off the machine says which address it would send from.
        for (const char* probe : { "8.8.8.8", "192.168.1.1", "10.0.0.1" }) {
            sock_t s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if ((intptr_t)s < 0) return "";
            sockaddr_in to;
            resolve(probe, 9, to);
            std::string ip;
            if (::connect(s, (sockaddr*)&to, sizeof to) == 0) {
                sockaddr_in me{};
                socklen_t len = sizeof me;
                if (getsockname(s, (sockaddr*)&me, &len) == 0) ip = addressOf(me);
            }
            closeSock((intptr_t)s);
            if (!ip.empty() && ip != "0.0.0.0") return ip;
        }
        return "";
    }

} // namespace net
#endif
