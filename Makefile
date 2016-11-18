SHELL=/bin/bash

CPPFLAGS += -std=c++11 -W -Wall  -g
CPPFLAGS += -O3
CPPFLAGS += -I include

LDLIBS += -ltbb

ifeq ($(OS),Windows_NT)
LDLIBS += -lws2_32
else
LDLIBS += -lrt
endif

all : bin/execute_puzzle bin/create_puzzle_input bin/run_puzzle bin/compare_puzzle_output

lib/libpuzzler.a : $(wildcard provider/*.cpp provider/*.hpp include/puzzler/*.hpp include/puzzler/*/*.hpp)
	$(MAKE) -C provider all

bin/% : src/%.cpp lib/libpuzzler.a
	-mkdir -p bin
	$(CXX) $(CPPFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS) -Llib -lpuzzler


serenity_now_% : all
	mkdir -p w
	bin/run_puzzle $* 100 1
	bin/create_puzzle_input $* 100 1 > w/$*.in
	cat w/$*.in | bin/execute_puzzle 1 1 > w/$*.ref.out
	cat w/$*.in | bin/execute_puzzle 0 1 > w/$*.got.out
	diff w/$*.ref.out w/$*.got.out

serenity_now : $(foreach x,julia ising_spin logic_sim random_walk,serenity_now_$(x))

.DELETE_ON_ERROR:
.SECONDARY:

PUZZLES = julia ising_spin logic_sim random_walk

.PHONY: test
test:	results/julia-5000.pass \
	results/ising_spin-300.pass \
	results/logic_sim-5000.pass \
	results/random_walk-10000.pass

bin w results records:
	mkdir -p $@

results/%.pass: w/%.ref w/%.out | results
	-diff -q $^ && touch $@

VERBOSE ?= 2

w/%.in: | bin/create_puzzle_input w
	bin/create_puzzle_input $(shell echo $* | sed 's/-/ /g') $(VERBOSE) > $@

w/%.ref: w/%.in | bin/execute_puzzle
	cat $< | (time bin/execute_puzzle 1 $(VERBOSE)) | sha1sum -b > $@

w/%.out: w/%.in bin/execute_puzzle
	cat $< | (time bin/execute_puzzle 0 $(VERBOSE)) | sha1sum -b > $@

# PDF generation

PLATFORM ?= i5_3337U-HD4000

.PHONY: pdf
pdf:	results/$(PLATFORM)-julia.pdf \
	results/$(PLATFORM)-ising_spin.pdf \
	results/$(PLATFORM)-logic_sim.pdf \
	results/$(PLATFORM)-random_walk.pdf

# Puzzle and platform specific evaluation ranges

VERBOSE-julia		:= 2
VERBOSE-ising_spin	:= 2
VERBOSE-logic_sim	:= 2
VERBOSE-random_walk	:= 3

SEQ-julia	:= $(shell for i in `seq 5 11`; do echo $$((2 ** i)); done)
SEQ-ising_spin	:= $(shell for i in `seq 3 9`; do echo $$((2 ** i)); done)
SEQ-logic_sim	:= $(shell for i in `seq 5 14`; do echo $$((2 ** i)); done)
SEQ-random_walk	:= $(shell for i in `seq 5 14`; do echo $$((2 ** i)); done)

# Records and data generation

TIME := $(shell date +%y%m%d-%H%M%S)

.PHONY: record
record: $(foreach i,$(PUZZLES),records/$(TIME)-$(PLATFORM)-$i.dat)

results/$(PLATFORM)-%.pdf: w/$(PLATFORM)-%.dat plot.gnu provider/user_%.hpp | results
	gnuplot -e "set xlabel 'scale'; set ylabel 'time'" \
		-e "set logscale x; set logscale y" \
		-e "filename='$<'" -e "cols=$(shell head -n 1 $< | sed 's/"[^"]*"/w/g' | wc -w)" ./plot.gnu > $@

w/$(PLATFORM)-%.dat: w/$(PLATFORM)-%-x.dat records/$(TIME)-$(PLATFORM)-%.dat provider/user_%.hpp | results
	echo "scale `ls records/*-$(PLATFORM)-$*.dat | sed 's/.*\/\([0-9]*-[0-9]*\).*/\1/' | xargs`" > $@
	paste w/$(PLATFORM)-$*-x.dat `ls records/*-$(PLATFORM)-$*.dat` | sed 's/^\t/NaN\t/;s/\s\s/\tNaN\t/g;s/\s\s/\tNan\t/g' >> $@

w/$(PLATFORM)-%-x.dat: | w
	echo $(SEQ-$*) | tr ' ' '\n' > $@

w/julia.time: $(foreach i,$(SEQ-julia),w/julia-$i.in)
w/ising_spin.time: $(foreach i,$(SEQ-ising_spin),w/ising_spin-$i.in)
w/logic_sim.time: $(foreach i,$(SEQ-logic_sim),w/logic_sim-$i.in)
w/random_walk.time: $(foreach i,$(SEQ-random_walk),w/random_walk-$i.in)

records/$(TIME)-$(PLATFORM)-%.dat: w/%.real | records
	cat $^ > $@

w/%.time: provider/user_%.hpp | bin/execute_puzzle
	for f in $(filter-out $<,$^); do echo $$f >&2; cat $$f | (time bin/execute_puzzle 0 $(VERBOSE-$*)); done 2>&1 > /dev/null | tee $@

w/%.real: w/%.time
	cat $< | grep -E 'Begin execution|Finished execution' | awk '{print $$2}' | sed 's/,//' | paste - - | sed 's/^/-/;s/\s/+/' | bc > $@

#	(echo "scale = 5;"; cat $< | grep -E '^real' | awk '{print $$2}' | sed 's/real//;s/m/*60+/;s/s//') | bc > $@
