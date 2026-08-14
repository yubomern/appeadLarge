#pragma once

#include <string>
#include <mutex>

namespace flightsim {

    /// État physique instantané d'un appareil (thread-safe via mutex interne,
    /// car il est lu par le serveur réseau et écrit par la boucle de
    /// simulation qui tournent sur le même io_context mais pourraient être
    /// étendus à plusieurs threads).
    class Aircraft {
    public:
        Aircraft(std::string callSign,
            double altitudeFt,
            double speedKts,
            double headingDeg,
            double fuelKg);

        // --- Accesseurs thread-safe ---
        double altitudeFt() const;
        double speedKts() const;
        double headingDeg() const;
        double fuelKg() const;
        double distanceNm() const;
        const std::string& callSign() const;

        // --- Mutateurs (commandes pilote) ---
        void setTargetSpeed(double kts);
        void setTargetAltitude(double ft);
        void setTargetHeading(double deg);

        double targetSpeedKts() const;
        double targetAltitudeFt() const;
        double targetHeadingDeg() const;

        /// Fait avancer l'état physique de dtSeconds secondes.
        /// Applique une approche simple vers les valeurs cibles et
        /// consomme du carburant proportionnellement à la poussée/vitesse.
        void step(double dtSeconds);

        /// Sérialise l'état courant en une ligne texte (format simple
        /// clé=valeur) envoyée aux clients connectés.
        std::string toTelemetryLine() const;

    private:
        mutable std::mutex mutex_;

        std::string callSign_;
        double altitudeFt_;
        double speedKts_;
        double headingDeg_;
        double fuelKg_;
        double distanceNm_ = 0.0;

        double targetSpeedKts_;
        double targetAltitudeFt_;
        double targetHeadingDeg_;
    };

} // namespace flightsim
