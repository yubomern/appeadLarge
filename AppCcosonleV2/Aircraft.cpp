#include "Aircraft.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace flightsim {

    namespace {
        // Rapproche `current` de `target` d'au plus `maxDelta` par appel.
        double approach(double current, double target, double maxDelta) {
            if (std::abs(target - current) <= maxDelta) {
                return target;
            }
            return current + (target > current ? maxDelta : -maxDelta);
        }
    }

    Aircraft::Aircraft(std::string callSign,
        double altitudeFt,
        double speedKts,
        double headingDeg,
        double fuelKg)
        : callSign_(std::move(callSign)),
        altitudeFt_(altitudeFt),
        speedKts_(speedKts),
        headingDeg_(std::fmod(headingDeg, 360.0)),
        fuelKg_(fuelKg),
        targetSpeedKts_(speedKts),
        targetAltitudeFt_(altitudeFt),
        targetHeadingDeg_(headingDeg) {
    }

    double Aircraft::altitudeFt() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return altitudeFt_;
    }

    double Aircraft::speedKts() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return speedKts_;
    }

    double Aircraft::headingDeg() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return headingDeg_;
    }

    double Aircraft::fuelKg() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return fuelKg_;
    }

    double Aircraft::distanceNm() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return distanceNm_;
    }

    const std::string& Aircraft::callSign() const {
        return callSign_;
    }

    void Aircraft::setTargetSpeed(double kts) {
        std::lock_guard<std::mutex> lock(mutex_);
        targetSpeedKts_ = std::clamp(kts, 0.0, 600.0);
    }

    void Aircraft::setTargetAltitude(double ft) {
        std::lock_guard<std::mutex> lock(mutex_);
        targetAltitudeFt_ = std::clamp(ft, 0.0, 45000.0);
    }

    void Aircraft::setTargetHeading(double deg) {
        std::lock_guard<std::mutex> lock(mutex_);
        double normalized = std::fmod(deg, 360.0);
        if (normalized < 0) normalized += 360.0;
        targetHeadingDeg_ = normalized;
    }

    double Aircraft::targetSpeedKts() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return targetSpeedKts_;
    }

    double Aircraft::targetAltitudeFt() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return targetAltitudeFt_;
    }

    double Aircraft::targetHeadingDeg() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return targetHeadingDeg_;
    }

    void Aircraft::step(double dtSeconds) {
        std::lock_guard<std::mutex> lock(mutex_);

        // Taux de variation approximatifs (unités par seconde)
        constexpr double climbRateFtPerSec = 33.0;      // ~2000 ft/min
        constexpr double accelKtsPerSec = 2.5;           // accélération
        constexpr double turnRateDegPerSec = 3.0;        // taux de virage standard

        altitudeFt_ = approach(altitudeFt_, targetAltitudeFt_, climbRateFtPerSec * dtSeconds);
        speedKts_ = approach(speedKts_, targetSpeedKts_, accelKtsPerSec * dtSeconds);

        // Virage par le chemin le plus court
        double diff = std::fmod(targetHeadingDeg_ - headingDeg_ + 540.0, 360.0) - 180.0;
        double maxTurn = turnRateDegPerSec * dtSeconds;
        if (std::abs(diff) <= maxTurn) {
            headingDeg_ = targetHeadingDeg_;
        }
        else {
            headingDeg_ += (diff > 0 ? maxTurn : -maxTurn);
            headingDeg_ = std::fmod(headingDeg_ + 360.0, 360.0);
        }

        // Distance parcourue : vitesse (kts = nm/h) * temps écoulé
        distanceNm_ += speedKts_ * (dtSeconds / 3600.0);

        // Consommation de carburant : dépend de la vitesse et de l'altitude
        // (modèle simplifié, pas une donnée aéronautique réelle)
        double burnRateKgPerSec = 0.02 * speedKts_ + 0.0005 * altitudeFt_;
        fuelKg_ = std::max(0.0, fuelKg_ - burnRateKgPerSec * dtSeconds);
    }

    std::string Aircraft::toTelemetryLine() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1);
        oss << "callsign=" << callSign_
            << " alt_ft=" << altitudeFt_
            << " speed_kts=" << speedKts_
            << " heading_deg=" << headingDeg_
            << " fuel_kg=" << fuelKg_
            << " dist_nm=" << distanceNm_;
        return oss.str();
    }

} // namespace flightsim
