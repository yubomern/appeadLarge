#include "FlightSimulator.h"

#include <iostream>

namespace flightsim {

    FlightSimulator::FlightSimulator(boost::asio::io_context& ioContext,
        std::chrono::milliseconds tickInterval)
        : ioContext_(ioContext),
        timer_(ioContext),
        tickInterval_(tickInterval) {
    }

    std::shared_ptr<Aircraft> FlightSimulator::addAircraft(const std::string& callSign,
        double altitudeFt,
        double speedKts,
        double headingDeg,
        double fuelKg) {
        auto aircraft = std::make_shared<Aircraft>(callSign, altitudeFt, speedKts, headingDeg, fuelKg);
        aircrafts_.push_back(aircraft);
        return aircraft;
    }

    std::shared_ptr<Aircraft> FlightSimulator::findByCallSign(const std::string& callSign) const {
        for (const auto& ac : aircrafts_) {
            if (ac->callSign() == callSign) {
                return ac;
            }
        }
        return nullptr;
    }

    void FlightSimulator::start() {
        if (running_) return;
        running_ = true;
        scheduleTick();
    }

    void FlightSimulator::stop() {
        running_ = false;
        timer_.cancel();
    }

    void FlightSimulator::scheduleTick() {
        timer_.expires_after(tickInterval_);
        timer_.async_wait([this](const boost::system::error_code& ec) { onTick(ec); });
    }

    void FlightSimulator::onTick(const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted || !running_) {
            return; // timer annulé (arrêt demandé) : on ne réarme pas
        }
        if (ec) {
            std::cerr << "[FlightSimulator] erreur du timer : " << ec.message() << "\n";
            return;
        }

        double dtSeconds = tickInterval_.count() / 1000.0;
        for (auto& aircraft : aircrafts_) {
            aircraft->step(dtSeconds);
        }

        scheduleTick(); // se réarme pour le prochain tick (boucle async)
    }

} // namespace flightsim
