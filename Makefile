
# Compiler settings
CC = g++
CFLAGS = -std=c++17 -fdiagnostics-color=always -O3 -pthread
LDFLAGS = 
DEBUGFLAGS = -g
MACROFLAGS = -DNO_AUX_DATA

# Structural settings
COUNTER_TYPE ?= emptyCounter
DIRECT_COUNT ?= 0
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

hardwareCounter: MACROFLAGS += -DUSE_HARDWARE_COUNTER
hardwareCounter: counterBenchmark
hardwareCounterTest: MACROFLAGS += -DUSE_HARDWARE_COUNTER
hardwareCounterTest: counterTest

aggFunnelCounter: MACROFLAGS += -DUSE_CONFIGURED_AGG_COUNTER
aggFunnelCounter: counterBenchmark
aggFunnelCounterTest: MACROFLAGS += -DUSE_CONFIGURED_AGG_COUNTER
aggFunnelCounterTest: counterTest

configuredAggFunnelCounter: MACROFLAGS += -DUSE_CONFIGURED_AGG_COUNTER -DUSE_FIXED_STUMP -DAGG_COUNT=$(AGG_COUNT) -DDIRECT_COUNT=$(DIRECT_COUNT)
configuredAggFunnelCounter: counterBenchmark
configuredAggFunnelCounterTest: MACROFLAGS += -DUSE_CONFIGURED_AGG_COUNTER -DUSE_FIXED_STUMP -DAGG_COUNT=$(AGG_COUNT) -DDIRECT_COUNT=$(DIRECT_COUNT)
configuredAggFunnelCounterTest: counterTest

recursiveAggFunnelCounter: MACROFLAGS += -DUSE_RECURSIVE_AGG_COUNTER
recursiveAggFunnelCounter: counterBenchmark
recursiveAggFunnelCounterTest: MACROFLAGS += -DUSE_RECURSIVE_AGG_COUNTER
recursiveAggFunnelCounterTest: counterTest

combFunnelCounter: MACROFLAGS += -DUSE_COMBINING_FUNNEL_COUNTER
combFunnelCounter: counterBenchmark
combFunnelCounterTest: MACROFLAGS += -DUSE_COMBINING_FUNNEL_COUNTER
combFunnelCounterTest: counterTest
