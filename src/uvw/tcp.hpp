#pragma once


#include <string>
#include <cstdint>
#include <memory>
#include <chrono>
#include <ratio>
#include <uv.h>
#include "stream.hpp"
#include "error.hpp"


namespace uvw {


class Tcp final: public Stream {
    static void protoConnect(uv_connect_t* req, int status) {
        Tcp *tcp = static_cast<Tcp*>(req->handle->data);
        tcp->connCb(UVWError{status});
        tcp->connCb = nullptr;
    }

    Handle<Stream> accept() const noexcept override {
        auto handle = loop()->handle<Tcp>(get<uv_stream_t>());

        if(!handle || !static_cast<Tcp&>(handle)) {
            handle = Handle<Tcp>{};
        }

        return handle;
    }

public:
    using Time = std::chrono::duration<uint64_t>;
    using CallbackConnect = std::function<void(UVWError)>;

    enum { IPv4, IPv6 };

    explicit Tcp(std::shared_ptr<Loop> ref)
        : Stream{HandleType<uv_tcp_t>{}, std::move(ref)},
          conn{std::make_unique<uv_connect_t>()}
    {
        initialized = (uv_tcp_init(parent(), get<uv_tcp_t>()) == 0);
    }

    explicit Tcp(std::shared_ptr<Loop> ref, uv_stream_t *srv): Tcp{ref} {
        initialized = initialized || (uv_accept(srv, get<uv_stream_t>()) == 0);
    }

    UVWError noDelay(bool value = false) noexcept {
        return UVWError{uv_tcp_nodelay(get<uv_tcp_t>(), value ? 1 : 0)};
    }

    UVWError keepAlive(bool enable = false, Time time = Time{0}) noexcept {
        return UVWError{uv_tcp_keepalive(get<uv_tcp_t>(), enable ? 1 : 0, time.count())};
    }

    template<int>
    void connect(std::string, int, CallbackConnect) noexcept;

    explicit operator bool() { return initialized; }

private:
    std::unique_ptr<uv_connect_t> conn;
    CallbackConnect connCb;
    bool initialized;
};


template<>
void Tcp::connect<Tcp::IPv4>(std::string ip, int port, CallbackConnect cb) noexcept {
    sockaddr_in addr;
    uv_ip4_addr(ip.c_str(), port, &addr);
    connCb = std::move(cb);
    get<uv_tcp_t>()->data = this;
    auto err = uv_tcp_connect(conn.get(), get<uv_tcp_t>(), reinterpret_cast<const sockaddr*>(&addr), &protoConnect);

    if(err) {
        connCb(UVWError{err});
    }
}


template<>
void Tcp::connect<Tcp::IPv6>(std::string ip, int port, CallbackConnect cb) noexcept {
    sockaddr_in6 addr;
    uv_ip6_addr(ip.c_str(), port, &addr);
    connCb = std::move(cb);
    get<uv_tcp_t>()->data = this;
    auto err = uv_tcp_connect(conn.get(), get<uv_tcp_t>(), reinterpret_cast<const sockaddr*>(&addr), &protoConnect);

    if(err) {
        connCb(UVWError{err});
    }
}


}
