#! /bin/zsh

mkdir -p lprq-fork/build
cd lprq-fork/build
CC=clang-13 CXX=clang++-13 cmake -DCMAKE_BUILD_TYPE=Release -DUSE_LIBCPP=ON -DDISABLE_HP=ON ..
make -j8
cd ../..

mkdir -p results/queue/task__figure6


BENCH="lprq-fork/build/bench-enq-deq"
RESULTa="results/queue/task__figure6a/main.csv"
RESULTb="results/queue/task__figure6b/main.csv"
RESULTc="results/queue/task__figure6c/main.csv"
ARGS="-w 32 -r 1024 -i 5"

if [[ -f $RESULT ]]; then
  mv $RESULT $RESULT.bak
fi

if [[ -z "$MAX_THREADS" ]]; then
  MAX_THREADS=`nproc --all`
fi

# if max thread is less than 64, use seq 4 4 max_thread
if [[ $MAX_THREADS -lt 64 ]]; then
  THREADS="1 2 "
  THREADS+=`seq 4 4 $MAX_THREADS | tr '\n' ' '`
else
  THREADS="1 2 4 8 12 16 24 32 40 48 56 64"
  for (( i=80; i<=$MAX_THREADS; i+=16 )); do
    THREADS="$THREADS $i"
  done
fi

NUMA_ARGS="numactl -i all"
if ! command -v numactl -i all ls &> /dev/null; then
  echo "Warning: numactl not found"
  NUMA_ARGS=""
fi

echo "Starting queue benchmark at `date`"
echo "Running with threads: $THREADS, 5 iterations each for 5 configurations"

AGGFUNNEL_ARGS="COUNTER_TYPE=1 STUMP_CONFIG_TYPE=fixed STUMP_FANOUT_COUNT=6 STUMP_DIRECT_COUNT=0"


# 6a bench
# aggfunnel lcrq
eval $AGGFUNNEL_ARGS $NUMA_ARGS $BENCH --fork -f $RESULTa -t $THREADS -q "LCRQueue/remap.+" $ARGS 2> /dev/null
# hw lprq, lcrq, lscq
eval COUNTER_TYPE=0 $NUMA_ARGS $BENCH --fork -f $RESULTa -t $THREADS -q "LCRQueue/remap.+" $ARGS 2> /dev/null
eval COUNTER_TYPE=0 $NUMA_ARGS $BENCH --fork -f $RESULTa -t $THREADS -q "LPRQueue/remap.+" $ARGS 2> /dev/null
eval COUNTER_TYPE=0 $NUMA_ARGS $BENCH --fork -f $RESULTa -t $THREADS -q "LSCQueue/remap.+" $ARGS 2> /dev/null
# combfunnel lcrq
eval COUNTER_TYPE=3 $NUMA_ARGS $BENCH --fork -f $RESULTa -t $THREADS -q "LCRQueue/remap.+" $ARGS 2> /dev/null

# 6b bench
# aggfunnel lcrq
eval CONSEQ_OPS=4 $AGGFUNNEL_ARGS $NUMA_ARGS $BENCH --fork -f $RESULTb -t $THREADS -q "LCRQueue/remap.+" $ARGS 2> /dev/null
# hw lprq, lcrq, lscq
eval CONSEQ_OPS=4 COUNTER_TYPE=0 $NUMA_ARGS $BENCH --fork -f $RESULTb -t $THREADS -q "LCRQueue/remap.+" $ARGS 2> /dev/null
eval CONSEQ_OPS=4 COUNTER_TYPE=0 $NUMA_ARGS $BENCH --fork -f $RESULTb -t $THREADS -q "LPRQueue/remap.+" $ARGS 2> /dev/null
eval CONSEQ_OPS=4 COUNTER_TYPE=0 $NUMA_ARGS $BENCH --fork -f $RESULTb -t $THREADS -q "LSCQueue/remap.+" $ARGS 2> /dev/null
# combfunnel lcrq
eval CONSEQ_OPS=4 COUNTER_TYPE=3 $NUMA_ARGS $BENCH --fork -f $RESULTb -t $THREADS -q "LCRQueue/remap.+" $ARGS 2> /dev/null


# # 6c bench
# aggfunnel lcrq
eval INIT_SIZE=500 $AGGFUNNEL_ARGS $NUMA_ARGS $BENCH --fork -f $RESULTc -t $THREADS -q "LCRQueue/remap.+" $ARGS 2> /dev/null
# hw lprq, lcrq, lscq
eval INIT_SIZE=500 COUNTER_TYPE=0 $NUMA_ARGS $BENCH --fork -f $RESULTc -t $THREADS -q "LCRQueue/remap.+" $ARGS 2> /dev/null
eval INIT_SIZE=500 COUNTER_TYPE=0 $NUMA_ARGS $BENCH --fork -f $RESULTc -t $THREADS -q "LPRQueue/remap.+" $ARGS 2> /dev/null
eval INIT_SIZE=500 COUNTER_TYPE=0 $NUMA_ARGS $BENCH --fork -f $RESULTc -t $THREADS -q "LSCQueue/remap.+" $ARGS 2> /dev/null
# combfunnel lcrq
eval INIT_SIZE=500 COUNTER_TYPE=3 $NUMA_ARGS $BENCH --fork -f $RESULTc -t $THREADS -q "LCRQueue/remap.+" $ARGS 2> /dev/null



echo "Queue benchmark finished at `date`"


# draw plot
python3 scripts/plotDrawer.py --figure_num 6 --data_path results/queue/task__figure6
