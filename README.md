# Message Queue

A message queue for inter thread communication:

* lock free (low lock) using `std::atomic` and `std::atomic_exchange_explicit`
(your compiler should support c++11 features).
* blocking or nonblocking receive mode.
* copy everything and share nothing
(allocate memory and copy the message contents).

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
