# Scheduler/Task TeSSLa Test Traces

Assumes the spec was generated with:

```bash
python3 gen_scheduler_tessla.py --max-tasks 3 --quantum 1 -o sched.tessla
```

Run one trace:

```bash
java -jar ~/Desktop/tessla.jar interpreter sched.tessla bad_blocked_running.input
```

Run all traces:

```bash
for f in test/*.input; do
  echo "===== $f ====="
  java -jar ~/Desktop/tessla.jar interpreter sched.tessla "$f"
done
```

State encoding:

```text
0 = CREATED
1 = READY
2 = RUNNING
3 = BLOCKED
```

Priority convention:

```text
higher numeric value = higher priority
```
