#include "FlightServer.h"

#include <iostream>
#include <sstream>

namespace flightsim {

    using boost::asio::ip::tcp;

    // ======================================================================
    // FlightSession
    // ======================================================================
    FlightSession::FlightSession(tcp::socket socket, FlightServer& server)
        : socket_(std::move(socket)), server_(server) {
    }

    void FlightSession::start() {
        deliver("BIENVENUE - commandes: SPEED <callsign> <kts> | ALT <callsign> <ft> | "
            "HDG <callsign> <deg> | LIST | QUIT\n");
        doRead();
    }

    void FlightSession::doRead() {
        auto self = shared_from_this();
        boost::asio::async_read_until(
            socket_, inputBuffer_, '\n',
            [this, self](const boost::system::error_code& ec, std::size_t /*length*/) {
                if (ec) {
                    close();
                    return;
                }
                std::istream is(&inputBuffer_);
                std::string line;
                std::getline(is, line);
                handleCommand(line);
                if (!closed_) {
                    doRead();
                }
            });
    }

    void FlightSession::handleCommand(const std::string& line) {
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "SPEED" || cmd == "ALT" || cmd == "HDG") {
            std::string callsign;
            double value = 0.0;
            if (!(iss >> callsign >> value)) {
                deliver("ERREUR: syntaxe attendue: " + cmd + " <callsign> <valeur>\n");
                return;
            }
            auto aircraft = server_.simulator().findByCallSign(callsign);
            if (!aircraft) {
                deliver("ERREUR: appareil inconnu '" + callsign + "'\n");
                return;
            }
            if (cmd == "SPEED") aircraft->setTargetSpeed(value);
            else if (cmd == "ALT") aircraft->setTargetAltitude(value);
            else if (cmd == "HDG") aircraft->setTargetHeading(value);

            deliver("OK: " + cmd + " " + callsign + " -> " + std::to_string(value) + "\n");

        }
        else if (cmd == "LIST") {
            std::ostringstream oss;
            for (const auto& ac : server_.simulator().aircrafts()) {
                oss << ac->toTelemetryLine() << "\n";
            }
            deliver(oss.str());

        }
        else if (cmd == "QUIT") {
            deliver("AU REVOIR\n");
            close();

        }
        else if (!cmd.empty()) {
            deliver("ERREUR: commande inconnue '" + cmd + "'\n");
        }
    }

    void FlightSession::deliver(const std::string& line) {
        bool writeInProgress = !writeQueue_.empty();
        writeQueue_.push_back(line);
        if (!writeInProgress && !closed_) {
            doWrite();
        }
    }

    void FlightSession::doWrite() {
        auto self = shared_from_this();
        boost::asio::async_write(
            socket_, boost::asio::buffer(writeQueue_.front()),
            [this, self](const boost::system::error_code& ec, std::size_t /*length*/) {
                if (ec) {
                    close();
                    return;
                }
                writeQueue_.pop_front();
                if (!writeQueue_.empty()) {
                    doWrite();
                }
            });
    }

    void FlightSession::close() {
        if (closed_) return;
        closed_ = true;
        boost::system::error_code ec;
        socket_.shutdown(tcp::socket::shutdown_both, ec);
        socket_.close(ec);
        server_.unregisterSession(shared_from_this());
    }

    // ======================================================================
    // FlightServer
    // ======================================================================
    FlightServer::FlightServer(boost::asio::io_context& ioContext,
        unsigned short port,
        FlightSimulator& simulator)
        : ioContext_(ioContext),
        acceptor_(ioContext, tcp::endpoint(tcp::v4(), port)),
        broadcastTimer_(ioContext),
        simulator_(simulator) {
    }

    void FlightServer::start() {
        doAccept();
        scheduleBroadcastTick();
    }

    void FlightServer::doAccept() {
        acceptor_.async_accept(
            [this](const boost::system::error_code& ec, tcp::socket socket) {
                if (!ec) {
                    auto session = std::make_shared<FlightSession>(std::move(socket), *this);
                    registerSession(session);
                    session->start();
                    std::cout << "[FlightServer] client connecté ("
                        << sessions_.size() << " au total)\n";
                }
                else {
                    std::cerr << "[FlightServer] erreur accept: " << ec.message() << "\n";
                }
                doAccept(); // continue d'accepter d'autres connexions
            });
    }

    void FlightServer::registerSession(std::shared_ptr<FlightSession> session) {
        sessions_.insert(std::move(session));
    }

    void FlightServer::unregisterSession(std::shared_ptr<FlightSession> session) {
        sessions_.erase(session);
    }

    void FlightServer::broadcast(const std::string& line) {
        for (const auto& session : sessions_) {
            session->deliver(line);
        }
    }

    void FlightServer::scheduleBroadcastTick() {
        broadcastTimer_.expires_after(std::chrono::seconds(2));
        broadcastTimer_.async_wait([this](const boost::system::error_code& ec) {
            if (ec) return; // annulé ou erreur : on arrête la diffusion
            for (const auto& aircraft : simulator_.aircrafts()) {
                broadcast(aircraft->toTelemetryLine() + "\n");
            }
            scheduleBroadcastTick();
            });
    }

} // namespace flightsim
