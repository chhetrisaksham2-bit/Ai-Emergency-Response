#include "ambulance.h"

#include <algorithm>

namespace rahat {

std::string toString(TripEventType type) {
    switch (type) {
        case TripEventType::ArrivedAtScene: return "ARRIVED_AT_SCENE";
        case TripEventType::LeftScene: return "LEFT_SCENE";
        case TripEventType::ArrivedAtHospital: return "ARRIVED_AT_HOSPITAL";
        case TripEventType::ReturnConfirmed: return "RETURN_CONFIRMED";
        case TripEventType::ReturnTimedOut: return "RETURN_TIMED_OUT";
    }
    return "UNKNOWN";
}

AmbulanceManager::~AmbulanceManager() {
    for (Trip* trip : trips) delete trip;
    for (Ambulance* ambulance : ambulances) delete ambulance;
}

Ambulance* AmbulanceManager::addAmbulance(const std::string& id, const std::string& hospitalId, AmbulanceType type,
                                          std::string* error) {
    if (byId.contains(id)) {
        *error = "Duplicate ambulance id " + id;
        return nullptr;
    }
    Ambulance* ambulance = new Ambulance();
    ambulance->id = id;
    ambulance->hospitalId = hospitalId;
    ambulance->type = type;
    byId.put(id, ambulance);
    ambulances.push_back(ambulance);
    return ambulance;
}

Ambulance* AmbulanceManager::find(const std::string& id) {
    Ambulance** found = byId.find(id);
    return found == nullptr ? nullptr : *found;
}

bool AmbulanceManager::suitable(const Ambulance& ambulance, AmbulanceType needed) {
    return needed == AmbulanceType::BLS || ambulance.type == AmbulanceType::ALS;
}

int AmbulanceManager::countFreeSuitable(const std::string& hospitalId, AmbulanceType needed) const {
    int count = 0;
    for (const Ambulance* ambulance : ambulances) {
        if (ambulance->hospitalId == hospitalId && ambulance->status == AmbulanceStatus::Available &&
            suitable(*ambulance, needed)) {
            ++count;
        }
    }
    return count;
}

Ambulance* AmbulanceManager::selectAmbulance(const Request& request, const std::string& hospitalId) {
    // TODO(policy b): for CARDIAC_ARREST and STROKE, search every hospital's free
    // ALS ambulances and pick the one nearest to the patient.
    Ambulance* fallback = nullptr;
    for (Ambulance* ambulance : ambulances) {
        if (ambulance->hospitalId != hospitalId || ambulance->status != AmbulanceStatus::Available) continue;
        if (!suitable(*ambulance, request.requiredAmbulance)) continue;
        if (ambulance->type == request.requiredAmbulance) return ambulance;  // exact match
        if (fallback == nullptr) fallback = ambulance;                       // ALS doing a BLS job
    }
    return fallback;
}

Trip* AmbulanceManager::startTrip(Ambulance* ambulance, const std::string& requestId, const std::string& hospitalId,
                                  const DoublyLinkedList<Checkpoint>& toScene,
                                  const DoublyLinkedList<Checkpoint>& toHospital, double now, double sceneMinutes,
                                  std::string* error) {
    if (ambulance == nullptr || ambulance->status != AmbulanceStatus::Available) {
        *error = "Ambulance is not available";
        return nullptr;
    }
    if (toScene.empty() || toHospital.empty()) {
        *error = "Trip needs a route to the patient and to the hospital";
        return nullptr;
    }

    Trip* trip = new Trip();
    trip->requestId = requestId;
    trip->hospitalId = hospitalId;
    trip->ambulance = ambulance;
    trip->dispatchedAt = now;

    // Leg 1: hospital -> patient
    toScene.forEach([&](const Checkpoint& c) {
        trip->route.pushBack({c.landmarkId, c.name, now + c.atMinute});
    });
    trip->scene = trip->route.tail();
    trip->arriveSceneAt = trip->scene->value.atMinute;
    trip->leaveSceneAt = trip->arriveSceneAt + sceneMinutes;

    // Leg 2: patient -> hospital (skip its first checkpoint, the scene itself)
    for (auto* node = toHospital.head()->next; node != nullptr; node = node->next) {
        trip->route.pushBack({node->value.landmarkId, node->value.name, trip->leaveSceneAt + node->value.atMinute});
    }
    trip->arriveHospitalAt = trip->route.back().atMinute;
    if (trip->arriveHospitalAt < trip->leaveSceneAt) trip->arriveHospitalAt = trip->leaveSceneAt;  // same landmark
    trip->position = trip->route.head();

    ambulance->status = AmbulanceStatus::ToScene;
    ambulance->requestId = requestId;
    ambulance->dispatchedAt = now;
    ambulance->releaseAt = 0;
    trips.push_back(trip);
    return trip;
}

std::vector<TripEvent> AmbulanceManager::update(double now) {
    std::vector<TripEvent> events;

    for (size_t i = 0; i < trips.size();) {
        Trip* trip = trips[i];
        Ambulance* ambulance = trip->ambulance;

        while (trip->position->next != nullptr && trip->position->next->value.atMinute <= now) {
            trip->position = trip->position->next;
        }

        // Several stages can pass in one update if time jumped far ahead.
        bool finished = false;
        bool changed = true;
        while (changed && !finished) {
            changed = false;
            if (ambulance->status == AmbulanceStatus::ToScene && now >= trip->arriveSceneAt) {
                ambulance->status = AmbulanceStatus::AtScene;
                events.push_back({TripEventType::ArrivedAtScene, trip->requestId, ambulance->id, trip->hospitalId,
                                  trip->arriveSceneAt});
                changed = true;
            } else if (ambulance->status == AmbulanceStatus::AtScene && now >= trip->leaveSceneAt) {
                ambulance->status = AmbulanceStatus::ToHospital;
                events.push_back({TripEventType::LeftScene, trip->requestId, ambulance->id, trip->hospitalId,
                                  trip->leaveSceneAt});
                changed = true;
            } else if (ambulance->status == AmbulanceStatus::ToHospital && now >= trip->arriveHospitalAt) {
                arriveAtHospital(trip, trip->arriveHospitalAt);
                events.push_back({TripEventType::ArrivedAtHospital, trip->requestId, ambulance->id, trip->hospitalId,
                                  trip->arriveHospitalAt});
                finished = true;
            }
        }

        if (finished) {
            delete trip;
            trips.erase(trips.begin() + static_cast<long>(i));
        } else {
            ++i;
        }
    }

    // Ambulances the hospital never confirmed: counted back after the timeout.
    for (Ambulance* ambulance : ambulances) {
        if (ambulance->status == AmbulanceStatus::AtHospital && now >= ambulance->releaseAt) {
            events.push_back({TripEventType::ReturnTimedOut, ambulance->requestId, ambulance->id,
                              ambulance->hospitalId, ambulance->releaseAt});
            ambulance->status = AmbulanceStatus::Available;
            ambulance->requestId.clear();
        }
    }

    std::stable_sort(events.begin(), events.end(),
                     [](const TripEvent& a, const TripEvent& b) { return a.time < b.time; });
    return events;
}

void AmbulanceManager::arriveAtHospital(Trip* trip, double at) {
    Ambulance* ambulance = trip->ambulance;
    ambulance->status = AmbulanceStatus::AtHospital;
    double timeout = ambulance->dispatchedAt + returnTimeout;
    ambulance->releaseAt = timeout > at ? timeout : at;
}

bool AmbulanceManager::confirmReturn(const std::string& ambulanceId, const std::string& hospitalId, double now,
                                     std::vector<TripEvent>* events, std::string* error) {
    Ambulance* ambulance = find(ambulanceId);
    if (ambulance == nullptr) {
        *error = "Unknown ambulance " + ambulanceId;
        return false;
    }
    if (ambulance->hospitalId != hospitalId) {
        *error = "Ambulance " + ambulanceId + " belongs to another hospital";
        return false;
    }
    if (ambulance->status == AmbulanceStatus::Available) {
        *error = "Ambulance " + ambulanceId + " is already available";
        return false;
    }
    if (ambulance->status == AmbulanceStatus::ToScene || ambulance->status == AmbulanceStatus::AtScene) {
        *error = "Ambulance " + ambulanceId + " has not picked up the patient yet";
        return false;
    }

    std::string requestId = ambulance->requestId;
    if (ambulance->status == AmbulanceStatus::ToHospital) {  // back earlier than planned
        for (size_t i = 0; i < trips.size(); ++i) {
            if (trips[i]->ambulance == ambulance) {
                arriveAtHospital(trips[i], now);
                events->push_back({TripEventType::ArrivedAtHospital, requestId, ambulance->id, hospitalId, now});
                delete trips[i];
                trips.erase(trips.begin() + static_cast<long>(i));
                break;
            }
        }
    }
    ambulance->status = AmbulanceStatus::Available;
    ambulance->requestId.clear();
    events->push_back({TripEventType::ReturnConfirmed, requestId, ambulance->id, hospitalId, now});
    return true;
}

const Trip* AmbulanceManager::tripOf(const std::string& ambulanceId) const {
    for (const Trip* trip : trips) {
        if (trip->ambulance->id == ambulanceId) return trip;
    }
    return nullptr;
}

}  // namespace rahat
