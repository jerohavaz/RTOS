GENERATOR_OPTIONS = {
    "max_tasks": 3,
    "max_semaphores": 2,
}


EXPECTED = {
    "valid_binary_baseline.input": set(),
    "valid_block_release.input": set(),
    "valid_counting_baseline.input": set(),
    "valid_priority_fifo.input": set(),
    "valid_timeout.input": set(),
    "valid_two_semaphores.input": set(),
    "bad_block_not_blocked.input": {
        "violation_sem_blocking_state",
    },
    "bad_count_discontinuity.input": {
        "violation_sem_count_discontinuity",
    },
    "bad_count_out_of_range.input": {
        "violation_sem_count_discontinuity",
        "violation_sem_count_out_of_range",
    },
    "bad_empty_acquire_succeeds.input": {
        "violation_sem_count_discontinuity",
        "violation_sem_empty_acquire_succeeded",
        "violation_sem_invalid_acquire",
    },
    "bad_invalid_create.input": {
        "violation_sem_invalid_create",
    },
    "bad_release_full_succeeds.input": {
        "violation_sem_invalid_release",
    },
    "bad_release_increments_with_waiter.input": {
        "violation_sem_invalid_release",
    },
    "bad_release_missing_wake.input": {
        "violation_sem_missing_wake",
    },
    "bad_spurious_wake.input": {
        "violation_sem_invalid_wait_lifecycle",
    },
    "bad_timeout_reports_success.input": {
        "violation_sem_invalid_acquire",
        "violation_sem_timeout_result",
    },
    "bad_timeout_too_early.input": {
        "violation_sem_timeout_too_early",
    },
    "bad_untracked_semaphore.input": {
        "violation_sem_untracked_semaphore",
    },
    "bad_wake_fifo.input": {
        "violation_sem_wake_fifo",
    },
    "bad_wake_lower_priority.input": {
        "violation_sem_wake_priority",
    },
}
