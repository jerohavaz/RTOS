# Expected Violations

Generated with:

```bash
python3 gen_scheduler_tessla.py --max-tasks 3 --quantum 1 -o sched.tessla
```

| File | Expected main violation(s) |
|---|---|
| `valid_baseline.input` | no violations |
| `bad_invalid_transition.input` | `violation_invalid_state_transition` |
| `bad_blocked_running.input` | `violation_blocked_running`, usually also `violation_running_event_inconsistent` |
| `bad_running_event_inconsistent.input` | `violation_running_event_inconsistent` |
| `bad_ready_event_inconsistent.input` | `violation_ready_event_inconsistent` |
| `bad_blocked_event_inconsistent.input` | `violation_blocked_event_inconsistent` |
| `bad_idle_while_ready.input` | `violation_idle_while_ready` |
| `bad_priority.input` | `violation_priority` |
| `bad_running_id_out_of_range.input` | `violation_running_id_out_of_range` |
| `bad_isr_resume.input` | `violation_isr_resume` |
| `bad_rr_quantum.input` | `violation_quantum`, `violation_round_robin` |
| `valid_round_robin.input` | no quantum/RR violation |

Some traces may produce additional consistency violations. That is fine if the main expected violation appears.
