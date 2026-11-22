# RAHAT — Design

**Real-time Assignment of Healthcare Assets and Transport**
Team SAVIOUR X · Graphic Era (Deemed to be University), Dehradun · 2026–27

This document records what RAHAT does and the decisions behind it. Class and
function names match the code in `modules/`.

> **Phase 3 changes** (see `README.md` and `features.md` for the full picture):
> - Three windows instead of two: **admin / server**, one **hospital** window per
>   hospital, and the **patient** window, plus a **request generator** for demos.
> - **Doctor capacity is in load units, pooled per hospital and specialty.** A MINOR or
>   MODERATE patient uses 1 unit, SERIOUS 2, and CRITICAL needs a whole doctor. A
>   hospital takes a patient if any on-duty specialist of that kind has enough units
>   free. This replaces the old "one slot per patient" rule in section 6.
> - **No accept / reject** (decision 2 below is replaced): the system checks every
>   hospital (`HospitalManager::checkAll`) and assigns the nearest one that can take
>   the patient right now. The "no response" and "ward full" simulation is removed.
>   The patient sees every hospital that was checked and why it was not selected.
> - An ambulance that has delivered its patient waits **at the hospital** until the
>   hospital presses its return button, or until 1 h 15 min after dispatch.
> - New requests are collected for a **batch window** (default 1 simulated minute)
>   and then served by priority; CRITICAL requests do not wait.
> - The shared folder has one folder per writer (section 9 is replaced by the table
>   in `README.md`).
> - 11 more emergencies in `data/` (16 in total).

---

## 1. Goal

Get the patient the **right** medical help as fast as possible:

- **Right hospital:** one that can actually treat this injury, meaning it has the
  needed facilities *and* a matching specialist on duty now. Not just the nearest one.
- **Fastest route:** based on **travel time with traffic** at the current time
  of day. The shortest road is not always the fastest.

The prototype is **terminal-based** and runs on a simulated area: our own
landmarks, roads, traffic patterns, hospitals, doctors and ambulances.

## 2. The three cases

| Case | Situation | What RAHAT does |
|---|---|---|
| 1 | One request | Rank hospitals, offer the patient to them in order, dispatch an ambulance |
| 2 | Several requests at the same time | The priority queue orders them by severity. The most urgent patient gets the first choice of hospital and ambulance. |
| 3 | Several patients from one place | One request with `patientCount > 1`. A hospital may accept only some of them; the rest are offered to the next hospital. |

## 3. Decisions

| # | Topic | Decision |
|---|---|---|
| 1 | Where the ambulance comes from | From the hospital that accepts the patient. |
| 2 | Hospital accept / reject | **Automatic.** A hospital accepts if it can treat the patient and has a free suitable ambulance. "No response" and "ward full" are simulated, and either moves the request to the next hospital. |
| 3 | Priority | Based on **injury intensity**: each injury has its own follow-up questions, and the answers add points. |
| 4 | First aid | Stored first-aid steps for each injury and severity are shown to the **ambulance crew** on dispatch. |
| 5 | Choosing a hospital | **Specialist first:** needed facilities and a matching specialist on duty now come first, then the shortest travel time. |
| 6 | Interface | Terminal only, with **two windows**: patient and hospital. |
| 7 | Data structures | **Hybrid:** heap, hash map, BST, doubly linked list and graph are written by us; the STL is used for strings, streams, files and `std::vector`. |

## 4. How a request flows

```
 PATIENT WINDOW                          HOSPITAL WINDOW (RAHAT engine)
 ──────────────                          ──────────────────────────────
 1. Choose injury (menu)
 2. Answer intensity questions
 3. Choose nearest landmark
 4. Number of patients
 5. Time taken automatically
        │
        └── shared/inbox ──────────►  6. Triage: points, severity, specialist,
                                         facilities, ambulance type
                                      7. Priority queue (Case 2 ordering)
                                      8. Rank hospitals: facilities + specialist
                                         on duty, then travel time with traffic
                                      9. Offer to hospital #1 ──► accept?
                                            no / no response ──► try #2, #3 …
                                     10. Reserve a specialist slot, choose ambulance
                                     11. Plan route (Dijkstra), start the trip
                                     12. Show first-aid steps to the crew
        ┌── shared/outbox ◄──────────   Status updates at each step
        │
 Shows: accepted by …, ambulance …,
 ETA, arrived, reached hospital
```

If no hospital can take the patient, the request goes back to waiting and is
tried again later.

## 5. Priority (Triage)

Each injury (e.g. `BLEEDING`, `SNAKE_BITE`) has a **base priority**, a list of
**intensity questions**, and **severity rules**.

```
points        = sum of the points of the chosen answers
priorityScore = basePriority + points + crowd bonus (patientCount - 1, max 5)
severity      = the highest rule whose minPoints <= points
```

The matching severity rule also decides what the patient needs: the
**specialization** (e.g. cardiologist), the **facilities** (e.g. ICU, CT scan,
antivenom) and the **ambulance type** (BLS or ALS).

Severity levels: Minor, Moderate, Serious, Critical.

**Queue order** (`RequestOrder`), in this order:

1. Higher severity first
2. Then higher priority score
3. Then the one received earlier
4. Then arrival sequence

## 6. Choosing a hospital

**Ranking** (`HospitalManager::rankFor`) keeps only hospitals that:

1. have **all** the required facilities,
2. have at least one required **specialist on duty** at this minute of the day,
   using doctor shifts, including overnight ones, and
3. can be reached by road.

These are sorted by **travel time with current traffic**, nearest first. Roads
are two-way with the same traffic both ways, so this also ranks the round trip.

**Hospital response** (`HospitalManager::respond`), checked in order:

| Check | Result |
|---|---|
| Simulated no-response chance | **No response**: try the next hospital |
| Missing a required facility | **Reject** |
| No free slot with an on-duty specialist | **Reject** |
| No free suitable ambulance | **Reject** |
| Simulated "ward full" chance | **Reject** |
| Otherwise | **Accept** `min(patients, free specialist slots, free ambulances)` patients |

A doctor can handle `maxPatients` emergency patients at once. A slot is reserved
on accept and released when treatment time is over. Both chances can be set to 0
for predictable tests.

## 7. Ambulance and trip

- **Types:** BLS (Basic Life Support) and ALS (Advanced Life Support). An ALS
  ambulance can do a BLS job, but not the other way round. For BLS jobs a BLS
  ambulance is preferred, so ALS ones stay free for critical cases.
- **Selection:** `selectAmbulance()` uses policy (a), a free suitable ambulance
  from the accepting hospital.
- **Trip:** one **doubly linked list** of checkpoints,
  `hospital → … → patient (scene) → … → hospital`, each with its arrival time.
  The trip's position moves forward as simulated time passes.
- **Trip events:** arrived at scene, left scene, arrived at hospital. At the end
  the ambulance is back at its home hospital and becomes available again.

## 8. Routing and traffic

- **Map:** landmarks are graph **vertices**; roads are two-way weighted **edges**.
- **Road travel time:**

  ```
  minutes = distance_km / speed_kmph × 60 × traffic multiplier
  ```

  | Traffic level | Multiplier |
  |---|---|
  | Clear | 1.0 |
  | Moderate | 1.4 |
  | Heavy | 2.0 |
  | Jam | 3.0 |

- **Traffic by time of day:** every road has a usual level, and it may follow a
  named pattern (e.g. `MARKET`) with time slots such as 17:00–20:00 = Heavy.
  Slots are stored in a **BST** keyed by start minute. Outside every slot, the
  usual level applies.
- **Dijkstra** runs once from the patient's landmark. Because roads are two-way,
  that single run gives both legs of the trip (hospital → patient and
  patient → hospital) and the travel time to every hospital for ranking.
- **Simplification:** a trip uses the traffic at the moment the route is planned.

## 9. Two windows (Comms)

The patient window and the hospital window are **two separate programs** in two
terminals. They talk by exchanging small text files in a shared folder:

```
shared/inbox/req_<id>.txt       patient  → hospital   one new request
shared/outbox/<id>_<seq>.txt    hospital → patient    status update number <seq>
```

- **Safe writes:** every file is written as `<name>.tmp` and then **renamed**, so
  the other program never reads a half-written file.
- **Format:** one `key=value` per line; newlines inside values are escaped.
- **Request IDs** are unique and time-ordered, e.g. `R1757925123456-042`, so
  several patient windows can run at once (a Case 2 demo).
- **Hospital side:** collects every waiting request, oldest ID first, deletes
  the files, and writes numbered status updates.
- **Patient side:** keeps checking for update `#0`, `#1`, `#2` … for its own
  request and shows each one.

## 10. Modules

Each module sits in `modules/<name>/` and gets its own `<name>_test.cpp`, so it
can be built and tested on its own.

| Module | Responsibility | Data structures / concepts |
|---|---|---|
| `ds` | Our own `PriorityQueue`, `HashMap`, `BST`, `DoublyLinkedList`, `Graph` | Heap, hashing, BST, DLL, graph, templates, `new`/`delete` |
| `models` | `Request`, `Hospital`, `Doctor`, `Ambulance`, enums, simulated clock, time helpers | OOP, encapsulation |
| `triage` | Injury catalog, intensity scoring, severity rules, request queue | Priority queue, hash map |
| `firstaid` | First-aid steps for each injury and severity | Hash map |
| `routing` | City map, traffic patterns, Dijkstra, route checkpoints | Graph, BST, DLL, hash map |
| `hospital` | Hospitals, doctors, ranking, automatic accept/reject, specialist slots | Hash map, priority queue |
| `ambulance` | Fleet, BLS/ALS suitability, `selectAmbulance()`, trips and trip events | DLL, hash map |
| `data_io` | Reads the CSV files and loads everything into a `World` | File handling |
| `comms` | Messages and the shared-folder channel between the two windows | File handling, hash map |
| `testkit` | Small test helpers used by the module tests | — |

## 11. Data files (`data/`)

Every file has a header row, and columns are found by name. Lines starting with
`#` are comments. Values containing commas go in double quotes. Lists inside one
value, such as facilities, are separated with `|`.

| File | Columns |
|---|---|
| `landmarks.csv` | `id,name` |
| `roads.csv` | `id,from,to,distance_km,speed_kmph,usual_traffic,pattern` |
| `traffic_patterns.csv` | `pattern,start,end,level` |
| `injuries.csv` | `code,name,base_priority` |
| `injury_questions.csv` | `injury,question,option,points` |
| `severity_rules.csv` | `injury,severity,min_points,specialization,facilities,ambulance` |
| `first_aid.csv` | `injury,severity,step` |
| `hospitals.csv` | `id,name,landmark,facilities` |
| `doctors.csv` | `id,hospital,name,specialization,shift_start,shift_end,max_patients` |
| `ambulances.csv` | `id,hospital,type` |

Starting injuries: bleeding, dog bite, snake bite, stroke, cardiac arrest. The
injury catalog, questions, severity rules and first-aid steps all come from these
files, so injuries can be added or tuned by editing data instead of code.
