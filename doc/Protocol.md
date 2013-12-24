# B protocol

## In-process protocol

For architectural documentation, see [B distributed
design][Distributed-Design.md].

### Client (REQ) - Broker (ROUTER)

    Request:
    Frame 1: UUID of question being asked.
    Frame 2: Question data, serialized per question type's
             specificiation.

    Response:
    Frame 1: Copy of worker request (WORKER_DONE_AND_READY)
             frame 4.

### Worker (REQ) - Broker (ROUTER)

    Request (WORKER_READY):
    Frame 1: Single byte 0x01.

    Request (WORKER_DONE_AND_READY):
    Frame 1: Single byte 0x02.
    Frame 2-3: Client identity envelope.
    Frame 4: Answer data, serialized per question's answer's
             type specification.

    Response:
    Frame 1-2: Client identity envelope.
    Frame 3-4: Copy of client request frames 1-2.

[Distributed-Design.md]: Distributed-Design.md
