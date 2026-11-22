# RAHAT - Phase 3

**Real-time Assignment of Healthcare Assets and Transport** - the final version.

Phase 3 joins the two earlier programs:

- from the first RAHAT program: the dispatch engine, triage, traffic-aware routing,
  doctors, ambulances, first aid, and the shared-folder messages between windows
  (`modules/`);
- from Phase 2: the full-screen FTXUI interface (`third_party/ftxui`) and the
  simple step-by-step patient flow.

and adds the Phase 3 features listed in `features.md`.

---

## The windows

Each window is its own program and runs in its own terminal.

| Program | Who uses it | What it shows |
|---|---|---|
| `./build/admin` | the system operator | every request, the queue (most urgent first), how each hospital was chosen, every hospital's free ambulances, every ambulance, a log |
| `./build/hospital --id H1` | one hospital (one window per hospital) | only the patients assigned to **this** hospital: the **pickup point**, the patient, the ambulance, first aid, route, progress and a **map**, plus the **return button** |
| `./build/patient` | the caller | the questions (or the one-question **fast track**), then the **destination hospital**, every hospital that was checked with the reason it was or was not selected, a **map** of the ambulance route, and live updates |
| `./build/generator` | demos and tests | sends many requests at once (scenario file or random), instead of typing them into patient windows |

There is **no accept / reject** step. The system checks every hospital and assigns
the nearest one that can take the patient right now. A hospital is skipped if it is
not reachable, is missing a facility, has no specialist on duty, its specialists are
all busy, or it has **no free ambulance** of the right type.

---

## Build and run (Mac / Linux)

```
cd RAHAT_3
make test      # build and run every test (no FTXUI needed)
make           # build all programs (the first time also builds FTXUI - needs cmake)
```

Install cmake once: Mac `brew install cmake`, Ubuntu `sudo apt install cmake`.

Then open one terminal per window, all in the `RAHAT_3` folder:

```
./build/admin --time 10:00                      # terminal 1 - start this FIRST
./build/hospital --id H1                        # terminal 2 (H1 ... H5, one per hospital)
./build/hospital --id H2                        # terminal 3 (optional, as many as you like)
./build/patient                                 # terminal 4 - report one emergency by hand
./build/generator --scenario scenarios/priority_demo.csv   # or many at once
```

The admin window starts a new session and clears the `shared` folder, so start it
first. Windows that are already open notice the new session and start again.

### Admin options

| Option | Meaning | Default |
|---|---|---|
| `--time HH:MM` | start the simulated clock at this time | now |
| `--speed N` | simulated seconds per real second | 60 (1 real second = 1 simulated minute) |
| `--batch MINUTES` | collect new requests this long, then serve them by priority. CRITICAL never waits | 1 |
| `--timeout MINUTES` | if a hospital never presses its return button, count the ambulance back this long after dispatch | 75 (1 h 15 min) |
| `--data DIR`, `--shared DIR` | data folder, shared folder | `data`, `shared` |

### Generator

```
./build/generator --scenario scenarios/priority_demo.csv
./build/generator --random 5 --seed 42 --interval 2
```

Scenario file columns: `send_at,landmark,injury,patients,answers`

- `send_at`: real seconds after the generator starts
- `landmark`: name or id from `data/landmarks.csv`
- `injury`: code or name from `data/injuries.csv`
- `answers`: one per follow-up question, separated by `|`, as the option text or its
  number (1 = first option). Put them in double quotes if an answer contains a comma.

Ready-made scenarios in `scenarios/`:

| File | Shows |
|---|---|
| `priority_demo.csv` | four requests sent at once, least urgent first; served most urgent first |
| `busy_hospital_demo.csv` | four burns patients next to one hospital; the fourth is sent elsewhere, with the reason |
| `mixed_demo.csv` | six common emergencies from different places, including an unknown condition and two patients at one place |

---

## How the windows talk

They exchange small text files in the `shared` folder. **Every folder has one writer.**

```
shared/
  session.txt                        admin       id of the running session
  clock.txt                          admin       simulated time, so every window shows the same clock
  requests/req_<id>.txt              patient / generator -> admin  (read and deleted)
  updates/<id>_<n>.txt               admin -> that patient window
  hospitals/H1/from_admin/<n>.txt    admin -> hospital H1  (assignments, progress, ambulance status)
  hospitals/H1/to_admin/msg_<id>.txt hospital H1 -> admin  (the return button; read and deleted)
```

Every file is written under a temporary name and then renamed, so a window never
reads half a file. Each window checks its folder twice a second on a background
thread and redraws only from its own memory, so there is no refresh key.

One emergency:

1. Patient window or generator writes a request into `requests/`.
2. Admin reads it, runs triage, puts it in the queue. After the batch window
   (at once if CRITICAL) it checks every hospital, picks one, picks an ambulance,
   and reserves a specialist.
3. Admin writes an **assignment** to that hospital's folder and an **update** with
   the destination and the hospital list to the patient.
4. As the simulated trip goes on, both windows get progress messages.
5. When the ambulance is back, the hospital presses **r**. Admin makes the ambulance
   available again and updates the hospital's ambulance count. If nobody presses it,
   the ambulance is counted back 1 h 15 min after dispatch (never before it reaches
   the hospital).

---

## Folders

| Folder | What is in it |
|---|---|
| `modules/` | the engine: `ds` (our heap, hash map, BST, linked list, graph), `models`, `routing` (Dijkstra with traffic), `triage`, `hospital`, `ambulance`, `firstaid`, `dispatch`, `data_io`, `comms`, `console`, `testkit` |
| `apps/` | the four programs, and the screen-free logic they use (`admin_server`, `hospital_client`, `patient_client`, `generator`), tested by `flow_test.cpp` |
| `data/` | CSV files: landmarks, roads, traffic, 16 injuries with follow-up questions, severity rules, first aid, hospitals, doctors, ambulances |
| `scenarios/` | demo files for the generator |
| `third_party/ftxui/` | the FTXUI terminal UI library (not edited by us) |

`make test` runs one test program per module plus `flow_test`, which runs the whole
generator -> admin -> hospital -> patient flow through real files.

---

## Data

**Fast track** is the first entry in the patient's list: one question with five
options, from "minor injury" to "EXTREME: unconscious, not breathing or no pulse".
The last two give CRITICAL, which is served at once without waiting for the batch.

The map (from Phase 2) uses `data/map_layout.csv` for drawing only; without that
file the windows work without the map.

16 emergencies: bleeding, dog bite, snake bite, stroke, cardiac arrest (Phase 1),
plus unknown / unclear condition, road accident / head injury, fracture, burns,
chest pain / heart attack, breathing difficulty / asthma, seizure, drug overdose /
poisoning, alcohol intoxication, gym muscle tear / sprain and diabetic emergency.

The places, roads, hospitals and doctors are made up for the prototype. The
questions, points, severity rules and first-aid steps are **sample values** and
must be checked by a doctor or a medical reference before any real use.
