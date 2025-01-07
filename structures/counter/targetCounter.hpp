#include "./hardwareCounter.hpp"
#include "./aggregatingFunnelCounter.hpp"
#include "./fullAggregatingFunnelCounter.hpp"
#include "./configuredAggregatingFunnelCounter.hpp"
#include "./recursiveAggregatingFunnelCounter.hpp"
#include "./combiningFunnelCounter.hpp"

#ifdef USE_HARDWARE_COUNTER
#pragma message("Compiling with HardwareCounter")
typedef HARDWARE_ATOMIC::HardwareCounter<long long> TargetCounter;

#elif USE_COMBINING_FUNNEL_COUNTER
#pragma message("Compiling with CombiningFunnelCounter")
typedef COMB_FUNNEL::CombiningFunnelCounter<long long> TargetCounter;

#elif USE_SIMPLE_AGG_COUNTER
#pragma message("Compiling with AggFunnelCounter")
typedef SIMPLE_AGG_FUNNEL::AggFunnelCounter<long long> TargetCounter;

#elif USE_FULL_AGG_COUNTER
#pragma message("Compiling with FullAggFunnelCounter")
typedef FULL_AGG_FUNNEL::FullAggFunnelCounter<long long> TargetCounter;

#elif USE_CONFIGURED_AGG_COUNTER
#pragma message("Compiling with ConfiguredAggFunnelCounter")
typedef CONFIGURED_AGG_FUNNEL::ConfiguredAggFunnelCounter<long long> TargetCounter;

#elif USE_RECURSIVE_AGG_COUNTER
#pragma message("Compiling with RecursiveAggFunnelCounter")
typedef RECURSIVE_AGG_FUNNEL::RecursiveAggFunnelCounter<long long> TargetCounter;

#elif USE_EMPTY_COUNTER
#pragma message("Compiling with EmptyCounter")
typedef EmptyCounter<long long> TargetCounter;

#else
#error "No counter type specified"
typedef HARDWARE_ATOMIC::HardwareCounter<long long> TargetCounter;
#endif