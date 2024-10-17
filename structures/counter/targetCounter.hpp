#include "./combiningFunnelCounter.hpp"
#include "./nestedStumpCounter.hpp"

#ifndef STUMP_COUNTER_HPP
#define STUMP_COUNTER_HPP
#include "stumpCounter.hpp"
#endif

#ifndef SIMPLE_ATOMIC_COUNTER_HPP
#define SIMPLE_ATOMIC_COUNTER_HPP
#include "simpleAtomicCounter.hpp"
#endif

#ifdef USE_SIMPLE_ATOMIC_COUNTER
#pragma message("Compiling with SimpleAtomicCounter")
typedef SIMPLE_ATOMIC_COUNTER::SimpleAtomicCounter<long long> TargetCounter;

#elif USE_COMBINING_FUNNEL_COUNTER
#pragma message("Compiling with CombiningFunnelCounter")
typedef COMBINING_FUNNEL_COUNTER::CombiningFunnelCounter<long long> TargetCounter;

#elif USE_STUMP_COUNTER
#pragma message("Compiling with StumpCounter")
typedef STUMP_COUNTER::StumpCounter<long long> TargetCounter;

#elif USE_NESTED_STUMP_COUNTER
#pragma message("Compiling with NestedStumpCounter")
typedef NESTED_STUMP_COUNTER::NestedStumpCounter<long long> TargetCounter;

#elif USE_EMPTY_COUNTER
#pragma message("Compiling with EmptyCounter")
typedef EmptyCounter<long long> TargetCounter;

#else
#error "No counter type specified"
typedef SIMPLE_ATOMIC_COUNTER::SimpleAtomicCounter<long long> TargetCounter;
#endif