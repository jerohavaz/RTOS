GENERATOR_OPTIONS = {
    "max_tasks": 4,
    "queue_capacities": {
        1: 2,
    },
}


EXPECTED = {
    "bad_block_transition.input": {
        "violation_queue_block_state_transition",
    },
    "bad_early_send_timeout.input": {
        "violation_send_timeout_too_early",
    },
    "bad_early_timeout.input": {
        "violation_receive_timeout_too_early",
    },
    "bad_fifo_order.input": {
        "violation_fifo_order",
    },
    "bad_fill_over_capacity.input": {
        "violation_queue_fill_bounds",
    },
    "bad_fill_inconsistent.input": {
        "violation_queue_fill_consistency",
    },
    "bad_handoff_hash.input": {
        "violation_direct_send_consistency",
    },
    "bad_message_integrity.input": {
        "violation_message_integrity",
    },
    "bad_missing_receiver_wake.input": {
        "violation_waiting_receiver_not_woken",
    },
    "bad_missing_receiver_handoff.input": {
        "violation_waiting_receiver_not_handed_off",
    },
    "bad_missing_sender_wake.input": {
        "violation_waiting_sender_not_woken",
    },
    "bad_no_wait_blocks.input": {
        "violation_no_wait_receive_blocked",
    },
    "bad_read_from_empty.input": {
        "violation_read_from_empty_queue",
        "violation_fifo_model_bounds",
        "violation_queue_fill_consistency",
    },
    "bad_send_block_transition.input": {
        "violation_queue_block_state_transition",
    },
    "bad_wake_transition.input": {
        "violation_queue_wake_state_transition",
    },
    "bad_write_to_full.input": {
        "violation_write_to_full_queue",
        "violation_fifo_model_bounds",
        "violation_queue_fill_consistency",
    },
    "valid_buffered_fifo.input": set(),
    "valid_direct_handoff.input": set(),
    "valid_receive_timeout.input": set(),
    "valid_sender_refill.input": set(),
}
