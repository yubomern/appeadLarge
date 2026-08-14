#pragma once
// Petit client TCP de démonstration : se connecte au simulateur de vol,
// affiche la télémétrie reçue en continu, et permet d'envoyer des
// commandes depuis l'entrée standard (thread séparé).
//
// Usage : ./flight_client [port]   (localhost par défaut, port 5555)

#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include <thread>

using boost::asio::ip::tcp;

int mainClient(int argc, char* argv[]) {
    unsigned short port = 8082;
    if (argc > 1) {
        port = static_cast<unsigned short>(std::stoi(argv[1]));
    }

    try {
        boost::asio::io_context ioContext;
        tcp::socket socket(ioContext);
        tcp::resolver resolver(ioContext);
        boost::asio::connect(socket, resolver.resolve("127.0.0.1", std::to_string(port)));

        std::cout << "Connecté au simulateur sur le port " << port
            << ". Tape 'LIST', 'SPEED AF123 300', etc. Ctrl+C pour quitter.\n";

        // Thread de lecture : affiche tout ce que le serveur envoie
        std::thread reader([&socket]() {
            try {
                boost::asio::streambuf buf;
                while (true) {
                    boost::asio::read_until(socket, buf, '\n');
                    std::istream is(&buf);
                    std::string line;
                    std::getline(is, line);
                    std::cout << "< " << line << "\n";
                }
            }
            catch (const std::exception&) {
                std::cout << "[connexion fermée]\n";
            }
            });

        // Boucle principale : lit l'entrée utilisateur et envoie au serveur
        std::string input;
        while (std::getline(std::cin, input)) {
            input += "\n";
            boost::asio::write(socket, boost::asio::buffer(input));
        }

        reader.join();

    }
    catch (const std::exception& e) {
        std::cerr << "Erreur : " << e.what() << "\n";
        return 1;
    }

    return 0;
}
