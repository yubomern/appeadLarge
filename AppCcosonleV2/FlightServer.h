#pragma once

#include <boost/asio.hpp>
#include <deque>
#include <memory>
#include <set>
#include <string>

#include "FlightSimulator.h"

namespace flightsim {

    class FlightServer;

    /// Une connexion client individuelle. Gère sa propre lecture asynchrone
    /// de commandes et sa file d'écriture (télémétrie diffusée + réponses).
    /// Utilise enable_shared_from_this pour prolonger sa propre durée de vie
    /// tant que des opérations async sont en cours (pattern standard Asio).
    class FlightSession : public std::enable_shared_from_this<FlightSession> {
    public:
        FlightSession(boost::asio::ip::tcp::socket socket, FlightServer& server);

        void start();
        void deliver(const std::string& line);
        void close();

    private:
        void doRead();
        void handleCommand(const std::string& line);
        void doWrite();

        boost::asio::ip::tcp::socket socket_;
        FlightServer& server_;
        boost::asio::streambuf inputBuffer_;
        std::deque<std::string> writeQueue_;
        bool closed_ = false;
    };

    /// Serveur TCP asynchrone :
    ///  - accepte plusieurs clients simultanément (accept en boucle async)
    ///  - diffuse périodiquement la télémétrie de tous les appareils
    ///  - interprète des commandes texte envoyées par les clients :
    ///      SPEED <callsign> <kts>
    ///      ALT   <callsign> <ft>
    ///      HDG   <callsign> <deg>
    ///      LIST
    ///      QUIT
    class FlightServer {
    public:
        FlightServer(boost::asio::io_context& ioContext,
            unsigned short port,
            FlightSimulator& simulator);

        void start();

        // Utilisé par les sessions pour retrouver le simulateur (commandes)
        FlightSimulator& simulator() { return simulator_; }

        void registerSession(std::shared_ptr<FlightSession> session);
        void unregisterSession(std::shared_ptr<FlightSession> session);

        /// Diffuse une ligne de texte à tous les clients connectés.
        void broadcast(const std::string& line);

    private:
        void doAccept();
        void scheduleBroadcastTick();

        boost::asio::io_context& ioContext_;
        boost::asio::ip::tcp::acceptor acceptor_;
        boost::asio::steady_timer broadcastTimer_;
        FlightSimulator& simulator_;
        std::set<std::shared_ptr<FlightSession>> sessions_;
    };

} // namespace flightsim
