
# Compiler settings
CC = g++
CFLAGS = -std=c++17 -fdiagnostics-color=always -O3 -pthread
LDFLAGS = 
DEBUGFLAGS = -g
MACROFLAGS = -DNO_AUX_DATA

# Structural settings
COUNTER_TYPE ?= emptyCounter
DIRECT_THREAD_COUNT ?= 0
AGG_COUNT ?= -1

counterBenchmark:
	mkdir -p build
	$(CC) $(CFLAGS) $(MACROFLAGS) $(LDFLAGS) $(INCLUDES) $(LIBS) bench/benchmark.cpp -o ./build/counter_benchmark

counterTest:
	mkdir -p build
	$(CC) $(DEBUGFLAGS) $(CFLAGS) $(MACROFLAGS) $(LDFLAGS) $(INCLUDES) $(LIBS) tests/correctnessTest.cpp -o ./build/counter_test

clean:
	rm -r ./build

emptyCounter: MACROFLAGS += -DUSE_EMPTY_COUNTER
emptyCounter: counterBenchmark

simpleAtomicCounter: MACROFLAGS += -DUSE_SIMPLE_ATOMIC_COUNTER
simpleAtomicCounter: counterBenchmark
simpleAtomicCounterTest: MACROFLAGS += -DUSE_SIMPLE_ATOMIC_COUNTER
simpleAtomicCounterTest: counterTest

stumpCounter: MACROFLAGS += -DUSE_STUMP_COUNTER
stumpCounter: counterBenchmark
stumpCounterTest: MACROFLAGS += -DUSE_STUMP_COUNTER
stumpCounterTest: counterTest


rootStumpCounter: MACROFLAGS += -DUSE_STUMP_COUNTER -DUSE_ROOT_STUMP -DDIRECT_THREAD_COUNT=$(DIRECT_THREAD_COUNT)
rootStumpCounter: counterBenchmark
rootStumpCounterTest: MACROFLAGS += -DUSE_STUMP_COUNTER -DUSE_ROOT_STUMP -DDIRECT_THREAD_COUNT=$(DIRECT_THREAD_COUNT)
rootStumpCounterTest: counterTest

fixedStumpCounter: MACROFLAGS += -DUSE_STUMP_COUNTER -DUSE_FIXED_STUMP -DAGG_COUNT=$(AGG_COUNT) -DDIRECT_THREAD_COUNT=$(DIRECT_THREAD_COUNT)
fixedStumpCounter: counterBenchmark
fixedStumpCounterTest: MACROFLAGS += -DUSE_STUMP_COUNTER -DUSE_FIXED_STUMP -DAGG_COUNT=$(AGG_COUNT) -DDIRECT_THREAD_COUNT=$(DIRECT_THREAD_COUNT)
fixedStumpCounterTest: counterTest


nestedStumpCounter: MACROFLAGS += -DUSE_NESTED_STUMP_COUNTER
nestedStumpCounter: counterBenchmark
nestedStumpCounterTest: MACROFLAGS += -DUSE_NESTED_STUMP_COUNTER
nestedStumpCounterTest: counterTest


combiningFunnelCounter: MACROFLAGS += -DUSE_COMBINING_FUNNEL_COUNTER
combiningFunnelCounter: counterBenchmark
combiningFunnelCounterTest: MACROFLAGS += -DUSE_COMBINING_FUNNEL_COUNTER
combiningFunnelCounterTest: counterTest


subEricTree: MACROFLAGS += -DUSE_SUB_ERIC_TREE
subEricTree: mapBenchmark

