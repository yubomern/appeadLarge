#pragma once
#include <boost/asio.hpp>
#include <iostream>

#include "FlightServer.h"
#include "FlightSimulator.h"

int maindata(int argc, char* argv[]) {
    unsigned short port = 8082;
    if (argc > 1) {
        port = static_cast<unsigned short>(std::stoi(argv[1]));
    }

    try {
        boost::asio::io_context ioContext;

        flightsim::FlightSimulator simulator(ioContext, std::chrono::milliseconds(500));

        // Quelques appareils de démonstration
        simulator.addAircraft("AF123", /*alt*/ 35000, /*speed*/ 480, /*heading*/ 270, /*fuel*/ 18000);
        simulator.addAircraft("BA456", /*alt*/ 28000, /*speed*/ 420, /*heading*/ 90, /*fuel*/ 15000);

        flightsim::FlightServer server(ioContext, port, simulator);

        simulator.start();
        server.start();

        std::cout << "Simulateur de vol démarré sur le port " << port << "\n";
        std::cout << "Connecte-toi avec : nc localhost " << port
            << "  (ou ./flight_client " << port << ")\n";
        std::cout << "Commandes : SPEED <callsign> <kts> | ALT <callsign> <ft> | "
            "HDG <callsign> <deg> | LIST | QUIT\n";

        // Gestion propre de Ctrl+C
        boost::asio::signal_set signals(ioContext, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code&, int) {
            std::cout << "\nArrêt du simulateur...\n";
            simulator.stop();
            ioContext.stop();
            });

        ioContext.run(); // boucle événementielle principale (bloquante)

    }
    catch (const std::exception& e) {
        std::cerr << "Erreur fatale : " << e.what() << "\n";
        return 1;
    }

    return 0;
}
