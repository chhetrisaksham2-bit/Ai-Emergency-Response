# RAHAT - Phase 3 Feature List

Status key:
- **Built**: in the Phase 3 code and tested
- **Decide**: needs an answer before it is built

| # | Feature | Status | Where |
|---|---|---|---|
| 1 | Request generator (separate program, scenario and random modes) | Built | `apps/generator_app.cpp`, `apps/generator.cpp`, `scenarios/` |
| 2 | Admin / server window: all requests, queue, every selection | Built | `apps/admin_app.cpp`, `apps/admin_server.cpp` |
| 3 | One window per hospital, own folder, only its own patients | Built | `apps/hospital_app.cpp`, `apps/hospital_client.cpp` |
| 4 | Automatic assignment, no accept / reject; skip a hospital with no free ambulance; patient sees every hospital checked and why | Built | `modules/hospital` (`checkAll`), `modules/dispatch` |
| 5 | Hospital assignment view: pickup point, patient, ambulance type, first aid | Built | hospital window |
| 6 | Hospital ambulance count (down on dispatch, up on return) | Built | hospital window, admin `STATUS` messages |
| 7 | Return button, and timeout 1 h 15 min after dispatch | Built | `modules/ambulance` (`confirmReturn`), admin `--timeout` |
| 8 | Fast-track intake (five quick options, last = extreme condition) | Built | `FAST_TRACK` in `data/` (first in the patient's list); sample options, see below |
| 9 | Unknown / unclear condition with simple follow-up questions | Built | `data/injuries.csv`, `data/injury_questions.csv` |
| 10 | Ambulance count per hospital | Built | `data/ambulances.csv` |
| 11 | Ten more common emergencies | Built | `data/` (16 injuries in total) |
| 12 | Priority queue with a batch window | Built | admin `--batch` (default 1 simulated minute; CRITICAL never waits) |
| 13 | Map from Phase 2 in the patient and hospital windows | Built | `apps/map_view.cpp`, `data/map_layout.csv` |

## Decisions recorded

- **Fast track options (sample, please review):** 1. Minor injury, can walk and talk
  (MINOR); 2. In pain or unwell, but stable (MODERATE); 3. Serious: heavy bleeding, a
  broken bone or severe pain (SERIOUS); 4. Struggling to breathe, chest pain or very
  confused (CRITICAL); 5. EXTREME: unconscious, not breathing or no pulse (CRITICAL,
  highest priority). CRITICAL requests skip the batch window.

- **Windows:** patient window shows the destination hospital; the admin window shows
  every request and every selection; each hospital window shows the patient's
  landmark as the pickup point.
- **No accept / reject:** the system decides. The "no response" and "ward full"
  simulation from the first RAHAT program is removed.
- **Return timeout:** 1 hour 15 minutes after dispatch, simulated time, and never
  before the ambulance reaches the hospital.
- **Batch window:** default 1 simulated minute (about 1 real second at the default
  speed of 60). The earlier suggestion of 10 simulated seconds was shorter than one
  check of the shared folder (half a real second = 30 simulated seconds), so it would
  have had no effect.
- **New emergencies:** road accident / head injury, fracture, burns, chest pain /
  heart attack, breathing difficulty / asthma, seizure, drug overdose / poisoning,
  alcohol intoxication, gym muscle tear / sprain, diabetic emergency, plus the
  unknown / unclear condition. Fractures and sprains need an orthopaedic surgeon and
  an X-ray, so three orthopaedic surgeons and the X-ray facility were added to the
  sample data.
- All questions, points, rules and first-aid steps are sample values and need a
  medical review.
