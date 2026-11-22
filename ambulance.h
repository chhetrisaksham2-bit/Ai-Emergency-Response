// Ambulance module: the ambulance fleet, choosing an ambulance, and trips.
//
// Current policy (decision 1a): the ambulance comes from the hospital that
// accepted the patient. selectAmbulance() is the single place to change when
// adding policy (b) later: nearest free ambulance for cardiac arrest / stroke.
//
// A trip is one doubly linked list of checkpoints:
//   hospital -> ... -> patient (scene) -> ... -> hospital
// `position` moves forward along the list as simulated time passes.
//
// Phase 3: when the trip reaches the hospital the ambulance is AtHospital, not
// yet Available. It becomes Available when the hospital presses the return
// button (confirmReturn), or automatically `returnTimeout` minutes after it was
// dispatched (never before it actually reaches the hospital).
#pragma once

#include <string>
#include <vector>

#include "../ds/doubly_linked_list.h"
#include "../ds/hash_map.h"
#include "../models/models.h"

namespace rahat {

enum class TripEventType { ArrivedAtScene, LeftScene, ArrivedAtHospital, ReturnConfirmed, ReturnTimedOut };
std::string toString(TripEventType type);

struct TripEvent {
    TripEventType type;
    std::string requestId;
    std::string ambulanceId;
    std::string hospitalId;
    double time;
};

struct Trip {
    using Node = DoublyLinkedList<Checkpoint>::Node;

    std::string requestId;
    std::string hospitalId;
    Ambulance* ambulance = nullptr;
    DoublyLinkedList<Checkpoint> route;  // absolute simulated times
    Node* scene = nullptr;               // checkpoint where the patient is picked up
    Node* position = nullptr;            // last checkpoint reached

    double dispatchedAt = 0;
    double arriveSceneAt = 0;
    double leaveSceneAt = 0;
    double arriveHospitalAt = 0;

    const Checkpoint* nextCheckpoint() const { return position && position->next ? &position->next->value : nullptr; }
};

class AmbulanceManager {
public:
    AmbulanceManager() = default;
    ~AmbulanceManager();
    AmbulanceManager(const AmbulanceManager&) = delete;
    AmbulanceManager& operator=(const AmbulanceManager&) = delete;

    Ambulance* addAmbulance(const std::string& id, const std::string& hospitalId, AmbulanceType type,
                            std::string* error);
    Ambulance* find(const std::string& id);
    const std::vector<Ambulance*>& all() const { return ambulances; }

    // ALS can do a BLS job, but BLS cannot do an ALS job.
    static bool suitable(const Ambulance& ambulance, AmbulanceType needed);

    int countFreeSuitable(const std::string& hospitalId, AmbulanceType needed) const;

    // Policy (a): a free, suitable ambulance from the accepting hospital.
    // For BLS jobs a BLS ambulance is preferred, keeping ALS free for critical cases.
    Ambulance* selectAmbulance(const Request& request, const std::string& hospitalId);

    // Starts a trip. toScene (hospital -> patient) and toHospital (patient -> hospital)
    // must have checkpoint times relative to 0; they are shifted to real times here.
    Trip* startTrip(Ambulance* ambulance, const std::string& requestId, const std::string& hospitalId,
                    const DoublyLinkedList<Checkpoint>& toScene, const DoublyLinkedList<Checkpoint>& toHospital,
                    double now, double sceneMinutes, std::string* error);

    // Moves every trip forward to `now`. Returns what happened, oldest first.
    // A trip that reaches the hospital is removed and its ambulance waits AtHospital.
    // Ambulances whose return timeout has passed become Available (ReturnTimedOut).
    std::vector<TripEvent> update(double now);

    // The hospital's return button. Allowed once the ambulance has the patient
    // (ToHospital or AtHospital). If it is still ToHospital the trip ends now,
    // so the events include ArrivedAtHospital before ReturnConfirmed.
    // Returns false with a reason if the ambulance cannot be confirmed.
    bool confirmReturn(const std::string& ambulanceId, const std::string& hospitalId, double now,
                       std::vector<TripEvent>* events, std::string* error);

    void setReturnTimeout(double minutes) { returnTimeout = minutes; }
    double returnTimeoutMinutes() const { return returnTimeout; }

    const std::vector<Trip*>& activeTrips() const { return trips; }
    const Trip* tripOf(const std::string& ambulanceId) const;

private:
    HashMap<std::string, Ambulance*> byId;
    std::vector<Ambulance*> ambulances;
    std::vector<Trip*> trips;
    double returnTimeout = 75.0;  // 1 hour 15 minutes

    void arriveAtHospital(Trip* trip, double at);
};

}  // namespace rahat
