#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <vector>

#include "Aircraft.h"

namespace flightsim {

    /// Fait tourner la boucle physique de un ou plusieurs Aircraft de façon
    /// asynchrone, via un boost::asio::steady_timer qui se réarme lui-même
    /// (pattern "async loop" classique avec Boost.Asio).
    class FlightSimulator {
    public:
        FlightSimulator(boost::asio::io_context& ioContext,
            std::chrono::milliseconds tickInterval = std::chrono::milliseconds(500));

        /// Ajoute un appareil à la simulation et retourne un pointeur partagé
        /// que le serveur réseau pourra aussi utiliser pour lire/écrire l'état.
        std::shared_ptr<Aircraft> addAircraft(const std::string& callSign,
            double altitudeFt,
            double speedKts,
            double headingDeg,
            double fuelKg);

        const std::vector<std::shared_ptr<Aircraft>>& aircrafts() const { return aircrafts_; }

        std::shared_ptr<Aircraft> findByCallSign(const std::string& callSign) const;

        /// Démarre la boucle asynchrone (ne bloque pas : programme les
        /// callbacks sur l'io_context fourni au constructeur).
        void start();

        void stop();

    private:
        void scheduleTick();
        void onTick(const boost::system::error_code& ec);

        boost::asio::io_context& ioContext_;
        boost::asio::steady_timer timer_;
        std::chrono::milliseconds tickInterval_;
        std::vector<std::shared_ptr<Aircraft>> aircrafts_;
        bool running_ = false;
    };

} // namespace flightsim
