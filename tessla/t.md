python3 -m gen --max-tasks 3 --quantum 1 -o sched.tessla

python3 -m script -o trace.input

java -jar ~/Desktop/tessla.jar interpreter ./scheduler/sched.tessla trace.input



python3 -m script -o trace.input


python3 scheduler/gen_scheduler_tessla.py --max-tasks 13 --quantum 1 -o scheduler/sched.tessla

java -jar ~/Desktop/tessla.jar interpreter ./scheduler/sched.tessla trace.input

python3 scheduler/gen_scheduler_tessla.py --max-tasks 3 --quantum 1 -o scheduler/sched.tessla

for f in scheduler/test/*.input; do
  echo "===== $f ====="
  java -jar ~/Desktop/tessla.jar interpreter scheduler/sched.tessla "$f"
done