#include "./hardwareCounter.hpp"
#include "./simpleAggregatingFunnelsCounter.hpp"
#include "./combiningFunnelsCounter.hpp"
#include "./recursiveAggregatingFunnelsCounter.hpp"

#ifdef USE_SIMPLE_ATOMIC_COUNTER
#pragma message("Compiling with HardwareCounter")
typedef HARDWARE_ATOMIC::HardwareCounter<long long> TargetCounter;

#elif USE_COMBINING_FUNNEL_COUNTER
#pragma message("Compiling with CombiningFunnelsCounter")
typedef COMB_FUNNEL::CombiningFunnelsCounter<long long> TargetCounter;

#elif USE_STUMP_COUNTER
#pragma message("Compiling with SimpleAggFunnelsCounter")
typedef SIMPLE_AGG_FUNNEL::SimpleAggFunnelsCounter<long long> TargetCounter;

#elif USE_NESTED_STUMP_COUNTER
#pragma message("Compiling with RecursiveAggFunnelsCounter")
typedef RECURSIVE_AGG_FUNNEL::RecursiveAggFunnelsCounter<long long> TargetCounter;

#elif USE_EMPTY_COUNTER
#pragma message("Compiling with EmptyCounter")
typedef EmptyCounter<long long> TargetCounter;

#else
#error "No counter type specified"
typedef HARDWARE_ATOMIC::HardwareCounter<long long> TargetCounter;
#endif