GENERATOR_OPTIONS = {
    "max_tasks": 3,
    "max_mutexes": 2,
}


EXPECTED = {
    "valid_baseline.input": set(),
    "valid_contention_rejections.input": set(),
    "valid_block_handoff.input": set(),
    "valid_priority_fifo.input": set(),
    "valid_timeout.input": set(),
    "valid_two_mutexes.input": set(),
    "bad_reinitialize_owned.input": {
        "violation_mutex_invalid_create",
    },
    "bad_owner_snapshot.input": {
        "violation_mutex_owner_discontinuity",
    },
    "bad_not_owner_unlock_succeeds.input": {
        "violation_mutex_invalid_unlock",
    },
    "bad_recursive_lock_succeeds.input": {
        "violation_mutex_recursive_lock",
    },
    "bad_locked_acquire_succeeds.input": {
        "violation_mutex_locked_acquire_succeeded",
    },
    "bad_lock_lifecycle.input": {
        "violation_mutex_invalid_lock",
    },
    "bad_block_not_blocked.input": {
        "violation_mutex_blocking_state",
    },
    "bad_timeout_too_early.input": {
        "violation_mutex_timeout_too_early",
    },
    "bad_timeout_reports_success.input": {
        "violation_mutex_invalid_lock",
        "violation_mutex_owner_discontinuity",
        "violation_mutex_timeout_result",
    },
    "bad_unlock_missing_wake.input": {
        "violation_mutex_missing_wake",
    },
    "bad_spurious_wake.input": {
        "violation_mutex_invalid_wait_lifecycle",
    },
    "bad_wake_lower_priority.input": {
        "violation_mutex_wake_priority",
    },
    "bad_wake_fifo.input": {
        "violation_mutex_wake_fifo",
    },
    "bad_untracked_mutex.input": {
        "violation_mutex_untracked_mutex",
    },
}
