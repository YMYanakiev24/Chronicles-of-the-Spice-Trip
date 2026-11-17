// ============================================================================
//  Net.h — the local network, for co-op (Net.cpp). Windows and Android talk
//  to each other over the Wi-Fi; a browser build has no network play.
//
//  A host runs a Server: it takes joiners on TCP port GAME_PORT and answers
//  "who's hosting?" on UDP port FIND_PORT. A joiner's Finder asks that on the
//  whole network every second and lists who answers; a Link is one connection,
//  carrying whole messages: [u32 size][u8 type][size bytes].
//
//  Nothing here blocks: call update() once a frame and read what has arrived.
//  (No raylib in here: windows.h and raylib don't mix.)
// ============================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace net {

    constexpr int GAME_PORT = 47740;        // TCP: the game
    constexpr int FIND_PORT = 47741;        // UDP: "who's hosting?" and the answers
    constexpr size_t MAX_MESSAGE = 4 << 20; // anything bigger is a broken stream

    bool available();                       // false in a browser

    struct Message {
        uint8_t type = 0;
        std::vector<uint8_t> data;
    };

    class Link {
    public:
        Link() = default;
        ~Link() { close(); }
        Link(const Link&) = delete;
        Link& operator=(const Link&) = delete;

        bool connect(const std::string& address, int port = GAME_PORT);   // starts it; see connected()/failed()
        void update();                      // sends what's waiting, reads what has arrived
        bool connected() const { return state == State::Open; }
        bool connecting() const { return state == State::Connecting; }
        bool failed() const { return state == State::Failed || state == State::Closed; }
        void send(uint8_t type, const void* data, size_t size);
        void send(uint8_t type, const std::vector<uint8_t>& data) { send(type, data.data(), data.size()); }
        bool receive(Message& m);           // the next whole message, if one is in
        size_t pending() const { return out.size() - outAt; }   // bytes still to go out
        void close();
        std::string peer;                   // their address

        // (the Server hands over sockets it accepted)
        void adopt(intptr_t socket, const std::string& address);

    private:
        enum class State { Idle, Connecting, Open, Failed, Closed };
        State state = State::Idle;
        intptr_t sock = -1;
        double started = 0;
        std::vector<uint8_t> out, in;
        size_t outAt = 0;
        void fail();
    };

    class Server {
    public:
        ~Server() { stop(); }
        bool start(int port = GAME_PORT);   // false if the port can't be opened
        void stop();
        bool running() const { return listener != -1; }
        // Answers anyone looking for games: `name` is shown in their list, with `players`/`max`.
        void advertise(const std::string& name, int players, int max, bool open);
        bool accept(Link& into);            // a new joiner, if one is knocking
        void update();                      // answers the "who's hosting?" queries
    private:
        intptr_t listener = -1, finder = -1;
        int port = GAME_PORT;
        std::string name;
        int players = 1, max = 4;
        bool open = true;
    };

    struct Found {
        uint32_t id = 0;                    // the host's own number (one host can answer twice)
        std::string name, address;
        int players = 0, max = 0;
        bool open = true;
        double seen = 0;                    // when it last answered (seconds)
    };

    class Finder {
    public:
        ~Finder() { stop(); }
        bool start();
        void stop();
        void update();                      // asks every second, lists who answers (and forgets the quiet)
        bool listening() const { return sock != -1; }
        const std::vector<Found>& games() const { return found; }
    private:
        intptr_t sock = -1;
        double lastAsk = -10;
        std::vector<Found> found;
    };

    std::string localAddress();             // this device's address on the network ("" if none)
    double now();                           // seconds, from any fixed point

    // Android has to be told to let the game hear broadcasts on the Wi-Fi (PlatformAndroid.cpp).
    void holdMulticastLock(bool on);

} // namespace net
