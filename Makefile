# RAHAT Phase 3 - build file. Run from the RAHAT_3 folder.
#
#   make            build every program into build/
#   make test       build and run the test of every module
#   make clean      delete build/ (FTXUI's own build stays)
#
# Programs (each runs in its own terminal):
#   ./build/admin       admin / server window: receives requests and assigns hospitals
#   ./build/hospital    hospital window:  ./build/hospital --id H1
#   ./build/patient     patient window:   asks the questions and shows the updates
#   ./build/generator   request generator for demos and tests
#   ./build/distance_demo  shortest vs fastest route between two landmarks (from Phase 2)
#
# The windows use FTXUI (third_party/ftxui). It is built once with cmake.

CXX      ?= c++
CXXFLAGS := -std=c++17 -Wall -Wextra -O1
LDFLAGS  := -pthread

MODULES  := models routing triage firstaid hospital ambulance data_io dispatch comms console
CORE_SRC := $(foreach m,$(MODULES),modules/$(m)/$(m).cpp)
CORE_OBJ := $(patsubst modules/%.cpp,build/obj/%.o,$(CORE_SRC))
HEADERS  := $(wildcard modules/*/*.h) $(wildcard apps/*.h)

TESTS    := ds models routing triage firstaid hospital ambulance data_io dispatch comms console
TEST_BIN := $(foreach t,$(TESTS),build/$(t)_test)

FTXUI      := third_party/ftxui
FTXUI_LIBS := $(FTXUI)/build/libftxui-component.a $(FTXUI)/build/libftxui-dom.a $(FTXUI)/build/libftxui-screen.a
UI_FLAGS   := -I$(FTXUI)/include

APPS := build/admin build/hospital build/patient build/generator
APP_CORE := apps/admin_server.cpp apps/hospital_client.cpp apps/patient_client.cpp apps/generator.cpp

all: $(APPS) build/distance_demo

build/obj/%.o: modules/%.cpp $(HEADERS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

define TEST_RULE
build/$(1)_test: modules/$(1)/$(1)_test.cpp $(CORE_OBJ) $(HEADERS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $$@ $$< $(CORE_OBJ) $(LDFLAGS)
endef
$(foreach t,$(TESTS),$(eval $(call TEST_RULE,$(t))))

# One stamp file, so a parallel build (make -j) runs cmake only once.
FTXUI_STAMP := $(FTXUI)/build/.rahat_built

$(FTXUI_STAMP):
	cmake -S $(FTXUI) -B $(FTXUI)/build -DCMAKE_BUILD_TYPE=Release \
	      -DFTXUI_BUILD_EXAMPLES=OFF -DFTXUI_BUILD_TESTS=OFF -DFTXUI_BUILD_DOCS=OFF
	cmake --build $(FTXUI)/build -j4
	@touch $@

$(FTXUI_LIBS): $(FTXUI_STAMP)

ADMIN_SRC    := apps/admin_app.cpp apps/admin_server.cpp apps/ui_common.cpp
HOSPITAL_SRC := apps/hospital_app.cpp apps/hospital_client.cpp apps/ui_common.cpp apps/map_view.cpp
PATIENT_SRC  := apps/patient_app.cpp apps/patient_client.cpp apps/ui_common.cpp apps/map_view.cpp
GEN_SRC      := apps/generator_app.cpp apps/generator.cpp apps/patient_client.cpp

build/admin: $(ADMIN_SRC) $(CORE_OBJ) $(HEADERS) $(FTXUI_LIBS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(UI_FLAGS) -o $@ $(ADMIN_SRC) $(CORE_OBJ) $(FTXUI_LIBS) $(LDFLAGS)

build/hospital: $(HOSPITAL_SRC) $(CORE_OBJ) $(HEADERS) $(FTXUI_LIBS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(UI_FLAGS) -o $@ $(HOSPITAL_SRC) $(CORE_OBJ) $(FTXUI_LIBS) $(LDFLAGS)

build/patient: $(PATIENT_SRC) $(CORE_OBJ) $(HEADERS) $(FTXUI_LIBS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(UI_FLAGS) -o $@ $(PATIENT_SRC) $(CORE_OBJ) $(FTXUI_LIBS) $(LDFLAGS)

build/generator: $(GEN_SRC) $(CORE_OBJ) $(HEADERS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $(GEN_SRC) $(CORE_OBJ) $(LDFLAGS)

build/distance_demo: apps/distance_demo.cpp $(CORE_OBJ) $(HEADERS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ apps/distance_demo.cpp $(CORE_OBJ) $(LDFLAGS)

build/flow_test: apps/flow_test.cpp $(APP_CORE) $(CORE_OBJ) $(HEADERS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ apps/flow_test.cpp $(APP_CORE) $(CORE_OBJ) $(LDFLAGS)

tests: $(TEST_BIN) build/flow_test

test: $(TEST_BIN) build/flow_test
	@for t in $(TEST_BIN) build/flow_test; do ./$$t > build/last_test.log || { cat build/last_test.log; echo "FAILED: $$t"; exit 1; }; tail -1 build/last_test.log; done

clean:
	rm -rf build

.PHONY: all tests test clean
