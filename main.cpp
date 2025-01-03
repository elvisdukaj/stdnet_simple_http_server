// examples/simple-http-server.cpp                                    -*-C++-*-
// ----------------------------------------------------------------------------
//
//  Copyright (c) 2024 Dietmar Kuehl http://www.dietmar-kuehl.de
//
//  Licensed under the Apache License Version 2.0 with LLVM Exceptions
//  (the "License"); you may not use this file except in compliance with
//  the License. You may obtain a copy of the License at
//
//    https://llvm.org/LICENSE.txt
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
// ----------------------------------------------------------------------------

#include <exec/async_scope.hpp>
#include <exec/task.hpp>
#include <exec/when_any.hpp>
#include <fmt/base.h>
#include <stdnet/buffer.hpp>
#include <stdnet/internet.hpp>
#include <stdnet/socket.hpp>
#include <stdnet/timer.hpp>

#include <algorithm>
#include <exception>
#include <fstream>
#include <string>

#include <fmt/format.h>
#include <thread>

using namespace std::chrono_literals;
using namespace std::string_view_literals;

// ----------------------------------------------------------------------------

struct parser {
    char const* it{nullptr};
    char const* end{it};

    bool empty() const { return it == end; }
    std::string_view find(char c) {
        auto f{std::find(it, end, c)};
        auto begin{it};
        it = f;
        return {begin, it == end ? it : it++};
    }
    void skip_whitespace() {
        while (it != end && *it == ' ' || *it == '\t')
            ++it;
    }
    std::string_view search(std::string_view v) {
        auto s{std::search(it, end, v.begin(), v.end())};
        auto begin{it};
        it = s != end ? s + v.size() : s;
        return {begin, s};
    }
};

template <typename Stream> struct buffered_stream {
    static constexpr char sep[]{'\r', '\n', '\r', '\n', '\0'};
    Stream stream;
    std::vector<char> buffer = std::vector<char>(1024u);
    std::size_t pos{};
    std::size_t end{};

    void consume() {
        buffer.erase(buffer.begin(), buffer.begin() + pos);
        end -= pos;
        pos = 0;
    }
    auto read_head() -> exec::task<std::string_view> {
        while (true) {
            if (buffer.size() == end)
                buffer.resize(buffer.size() * 2);
            auto n = co_await stdnet::async_receive(
                stream,
                stdnet::buffer(buffer.data() + end, buffer.size() - end));
            if (n == 0u)
                co_return {};
            end += n;
            pos = std::string_view(buffer.data(), end).find(sep);
            if (pos != std::string_view::npos)
                co_return {buffer.data(), pos += std::size(sep) - 1};
        }
    }
    auto write_response(std::string_view message,
                        std::string_view response) -> exec::task<void> {
        std::string head = fmt::format("HTTP/1.1 {}\r\n"
                                       "Content-Length: {}\r\n"
                                       "\r\n",
                                       message, response.size());

        std::size_t p{}, n{};
        do
            n = co_await stdnet::async_send(
                stream, stdnet::buffer(head.data() + p, head.size() - p));
        while (0 < n && (p += n) != head.size());

        p = 0;
        do
            n = co_await stdnet::async_send(
                stream,
                stdnet::buffer(response.data() + p, response.size() - p));
        while (0 < n && (p += n) != response.size());
    }
};

struct request {
    std::string method, uri, version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

auto read_http_request(auto& stream) -> exec::task<request> {
    request r;
    auto head = co_await stream.read_head();
    if (not head.empty()) {
        parser p{head.begin(), head.end() - 2};
        r.method = p.find(' ');
        r.uri = p.find(' ');
        r.version = p.search("\r\n");

        while (not p.empty()) {
            std::string key{p.find(':')};
            p.skip_whitespace();
            auto value{p.search("\r\n")};
            r.headers[key] = value;
        }
    }
    stream.consume();
    co_return r;
}

std::unordered_map<std::string, std::string> res{{"/", "data/hello.html"},
                                                 {"/fav.png", "data/fav.png"}};

template <> struct fmt::formatter<stdnet::ip::basic_endpoint<stdnet::ip::tcp>> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) const {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const auto endpoint, FormatContext& ctx) const {
        std::ostringstream os;
        os << endpoint;
        return fmt::format_to(ctx.out(), "{}", os.str());
    }
};

template <typename Stream> auto make_client(Stream s) -> exec::task<void> {
    std::unique_ptr<char const,
                    decltype([](char const* str) { fmt::println("{}", str); })>
        dtor("stopping client");
    fmt::println("starting client");

    buffered_stream<Stream> stream{std::move(s)};
    bool keep_alive{true};

    while (keep_alive) {
        auto r = co_await read_http_request(stream);
        if (r.method.empty()) {
            co_await stream.write_response("550 ERROR", "");
            co_return;
        }

        if (r.method == "GET"sv) {
            auto it = res.find(r.uri);
            fmt::println("getting {} -> {}", r.uri,
                         (it == res.end() ? "404" : "OK"));
            if (it == res.end()) {
                co_await stream.write_response("404 NOT FOUND", "not found");
            } else {
                std::ifstream in(it->second);
                std::string res(std::istreambuf_iterator<char>(in), {});
                co_await stream.write_response("200 OK", res);
            }
        }

        keep_alive = r.headers["Connection"] == "Keep-Alive"sv;
        fmt::println("keep-alive={}", keep_alive);
    }
}

auto make_server(auto& context, auto& scope,
                 auto endpoint) -> exec::task<void> {
    stdnet::ip::tcp::acceptor acceptor(context, endpoint);
    while (true) {
        auto [stream, client] = co_await stdnet::async_accept(acceptor);
        fmt::println("received a connection from {}", client);
        scope.spawn(exec::when_any(stdnet::async_resume_after(
                                       context.get_scheduler(), 10s),
                                   make_client(::std::move(stream))) |
                    stdexec::upon_error([](auto) { fmt::println("error"); }));
    }
}

int main() {
    try {
        stdnet::io_context context;
        stdnet::ip::tcp::endpoint endpoint(stdnet::ip::address_v4::any(),
                                           12345);
        exec::async_scope scope;

        scope.spawn(make_server(context, scope, endpoint));

        context.run();
    } catch (std::exception const& exc) {
        fmt::println("ERROR: {}", exc.what());
    }
}
