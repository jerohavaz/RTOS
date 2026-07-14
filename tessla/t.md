python3 -m gen --max-tasks 3 --quantum 1 -o sched.tessla

python3 -m script -o trace.input

java -jar ~/Desktop/tessla.jar interpreter sched.tessla trace.input