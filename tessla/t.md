python3 -m script -o trace.input


python3 scheduler/gen_scheduler_tessla.py --max-tasks 13 --quantum 1 --mode checks -o scheduler/sched.tessla

java -jar ~/Desktop/tessla.jar interpreter ./scheduler/sched.tessla trace.input

python3 scheduler/gen_scheduler_tessla.py --max-tasks 3 --quantum 1 --mode checks -o scheduler/sched.tessla

for f in scheduler/test/*.input; do
  echo "===== $f ====="
  java -jar ~/Desktop/tessla.jar interpreter scheduler/sched.tessla "$f"
done



python3 scheduler/gen_scheduler_tessla.py --max-tasks 13 --quantum 1 --mode checks -o scheduler/sched.tessla

python3 rtt_to_tessla.py --stdout | java -jar ~/Desktop/tessla.jar interpreter scheduler/sched.tessla