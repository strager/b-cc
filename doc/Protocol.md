# B protocol

## In-process protocol

For architectural documentation, see [B distributed
design][Distributed-Design.md].

### Client (DEALER) - Broker (ROUTER)

    Request:
    Frame 1: Request ID (arbitrary 4 bytes).
    Frame 2: UUID of question being asked.
    Frame 3: Question data, serialized per question type's
             specificiation.

    Response:
    Frame 1: Copy of worker request (WORKER_DONE_AND_READY)
             frame 4.

### Worker (DEALER) - Broker (ROUTER)

    Request (WORKER_READY):
    Frame 1: Single byte 0x01.

    Request (WORKER_DONE_AND_READY):
    Frame 1: Single byte 0x02.
    Frame 2-3: Client identity envelope.
    Frame 4: Copy of client request frame 1 (request ID).
    Frame 5: Answer data, serialized per question's answer's
             type specification.

    Request (WORKER_EXIT):
    Frame 1: Single byte 0x03.

    Request (WORKER_ABANDON):
    Frame 1: Single byte 0x04.
    Frame 2-3: Client identity envelope.
    Frame 4-6: Copy of client request frames 1-3.

    Response (WORKER_READY, WORKER_DONE_AND_READY):
    Frame 1-2: Client identity envelope.
    Frame 3-5: Copy of client request frames 1-3.

    Response (WORKER_EXIT, WORKER_ABANDON):
    Frame 1: Empty frame.

### Worker resignation

1. The worker sends a `WORKER_EXIT` request.
2. The worker awaits a response.
3. If the response's first frame is empty, the broker has
   confirmed the exit (i.e. has received `WORKER_EXIT`
   before responding to `WORKER_READY` or
   `WORKER_DONE_AND_READY`).  The worker can safely die.
   Abort these steps.
4. The broker has sent work for the worker to do (in reply
   to `WORKER_READY` or `WORKER_DONE_AND_READY`).  The
   worker sends a `WORKER_ABANDON` request and copies the
   entirety of the broker's response (the workload).
5. The worker awaits a response.
6. The response from the broker must be an empty frame, in
   reply to `WORKER_EXIT`.  The worker can safely die.
   Abort these steps.

[Distributed-Design.md]: Distributed-Design.md
