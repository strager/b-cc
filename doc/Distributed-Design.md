# B distributed design

Concurrency in B is implement using [ZeroMQ][0mq].
Terminology in this document matches closely to terminology
in the ZeroMQ culture.

## In-process concurrency

Each process contains:

* Zero or more ZeroMQ I/O threads,
* One "process broker" thread,
* One or more "worker" threads, and
* One main "client" thread.

The connection between these components in a single process
is depicted in the following diagram:

    #--------#  #--------#  #--------#
    | Client |  | Client |  | Client |
    +--------+  +--------+  +--------+
    | DEALER |  | DEALER |  | DEALER |
    '---+----'  '---+----'  '---+----'
        |           |           |
        '-----------+-----------'
                    |
                    | .-------------------.
                    | |                   |
                .---+-+--.                |
                | ROUTER |  Frontend      |
                +--------+                |
                | Broker |                |
                +--------+                |
                | ROUTER |  Backend       |
                '---+----'                |
                    |                     |
        .-----------+-----------.         |
        |           |           |         |
    .---+----.  .---+----.  .---+----.    |
    |  REQ   |  |  REQ   |  |  REQ   |    |
    +--------+  +--------+  +--------+    |
    | Worker |  | Worker |  | Worker |    |
    #--------#  #--------#  +--------+    |
                            | Client |    |
                            +--------+    |
                            | DEALER |    |
                            '---+----'    |
                                |         |
                                '---------'

This follows the [Load Balancing
Pattern][load-balancing-pattern], with the exception that
clients are DEALERs.  Workers can connect to a broker to ask
for work to be performed.  Connections can be circular: a
worker can connect to a broker (via a client socket) and
that broker can connect back to the same worker.

Because the process broker and workers are in the same
process, reliability is guaranteed.  Additional fault
tolerance beyond what ZeroMQ provides with the `inproc:`
protocol is not necessary.  

### ZeroMQ I/O threads

ZeroMQ allows I/O operations to occur on any number of
dedicated threads.  See the [ZeroMQ documentation on
`ZMQ_IO_THREADS`][ZMQIOTHREADS] for details

### Process broker

The process broker is a ZeroMQ thread which interacts with
other process brokers, workers, clients, and brokers in
other processes.

The process broker is the only ZeroMQ node accessible
outside the process (via the `tcp:` ZeroMQ protocol).

Process brokers primary load balance work (executing a B
rule) to workers or other brokers.

### Worker

Each worker is associated with one process broker.  A worker
accepts work from and gives work to its process broker.

## Cross-process concurrency

TODO

[0mq]: http://zeromq.org/
[ZMQIOTHREADS]: http://api.zeromq.org/3-2:zmq-ctx-set
[load-balancing-pattern]: http://zguide.zeromq.org/page:all#toc69
