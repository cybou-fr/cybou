// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/bootstrap_protocol.h>

#include <boost/asio.hpp>

#include <atomic>
#include <csignal>
#include <iostream>
#include <thread>
#include <limits>
#include <string_view>

namespace {
std::atomic_bool running{true};
void Stop(int) { running.store(false); }

int Usage()
{
    std::cerr << "Usage:\n"
        << "  cybou-bootstrap provision <data-dir>\n"
        << "  cybou-bootstrap serve <data-dir> <numeric-bind-address> <port> <certificate-chain.pem> <private-key.pem>\n";
    return 2;
}
}

int main(int argc, char** argv)
{
    if (argc == 3 && std::string_view{argv[1]} == "provision") {
        auto provision = cybou::BootstrapStore::Provision(argv[2]);
        if (!provision) {
            std::cerr << "Could not provision bootstrap store (it may already exist).\n";
            return 1;
        }
        std::cout << "Activation code (record securely; it is shown only once): "
                  << provision->activation_code << '\n';
        return 0;
    }
    if (argc != 7 || std::string_view{argv[1]} != "serve") return Usage();

    unsigned port_value{0};
    try {
        const std::string port_text{argv[4]};
        size_t parsed{0};
        port_value = static_cast<unsigned>(std::stoul(port_text, &parsed));
        if (parsed != port_text.size() || port_value == 0 || port_value > 65535) return Usage();
    } catch (...) { return Usage(); }
    boost::system::error_code error;
    const auto address = boost::asio::ip::make_address(argv[3], error);
    if (error) return Usage();

    auto store = cybou::BootstrapStore::Open(argv[2]);
    if (!store) {
        std::cerr << "Could not open bootstrap store. Provision it once before serving.\n";
        return 1;
    }
    boost::asio::io_context io;
    boost::asio::ip::tcp::acceptor acceptor{io};
    const boost::asio::ip::tcp::endpoint endpoint{address, static_cast<uint16_t>(port_value)};
    acceptor.open(endpoint.protocol(), error);
    if (!error) acceptor.set_option(boost::asio::socket_base::reuse_address(true), error);
    if (!error) acceptor.bind(endpoint, error);
    if (!error) acceptor.listen(boost::asio::socket_base::max_listen_connections, error);
    if (error) {
        std::cerr << "Could not bind bootstrap listener: " << error.message() << '\n';
        return 1;
    }
    acceptor.non_blocking(true, error);
    if (error) return 1;

    std::signal(SIGINT, Stop);
    std::signal(SIGTERM, Stop);
    std::cout << "Bootstrap endpoint listening on " << endpoint << '\n';
    while (running.load()) {
        boost::asio::ip::tcp::socket socket{io};
        acceptor.accept(socket, error);
        if (error == boost::asio::error::would_block || error == boost::asio::error::try_again) {
            std::this_thread::sleep_for(std::chrono::milliseconds{50});
            continue;
        }
        if (error) {
            if (running.load()) std::cerr << "Accept failed: " << error.message() << '\n';
            error.clear();
            continue;
        }
        cybou::BootstrapProtocolHandler handler{*store};
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER,
            cybou::p2p::TlsSessionConfig{argv[5], argv[6], std::nullopt}};
        session.ServeBootstrapRequest([&handler](const cybou::p2p::Frame& frame) { return handler.Handle(frame); });
    }
    return 0;
}
