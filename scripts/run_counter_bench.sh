#! /bin/zsh
echo "Starting counter benchmark at `date`"

python3 scripts/taskGenerator.py --default --figure_num 3
python3 scripts/benchmarkRunner.py --task_path local/task_figure3.json
python3 scripts/plotDrawer.py --figure_num 3 --data_path results/counter/task__figure3

python3 scripts/taskGenerator.py --default --figure_num 4
python3 scripts/benchmarkRunner.py --task_path local/task_figure4.json
python3 scripts/plotDrawer.py --figure_num 4 --data_path results/counter/task__figure4

python3 scripts/taskGenerator.py --default --figure_num 5
python3 scripts/benchmarkRunner.py --task_path local/task_figure5.json
python3 scripts/plotDrawer.py --figure_num 5 --data_path results/counter/task__figure5

echo "Counter benchmark finished at `date`"
