#Message Queue

A message queue for inter thread communication:

* lock free (low lock) using CAS (compare and swap).
* blocking or nonblocking receive mode.
* copy everything and share nothing (allocate memory and a message is copied).

## Message Queue Traits

Default traits is `MessageQueueTraits<ReceiveBlock>`.
if there's no message in the queue, Receiver is blocked on the queue.

if `MessageQueueTraits<ReceiveNonblock>` is used, `Receive()` call returns
`false` immediately.

## Test

You can build test (sample) programs and find them in `build/bin`:

    cd build
    cmake ..
    make
