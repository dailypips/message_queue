# Message Queue

A message queue for inter thread communication:

* lock free (low lock) using `std::atomic` and `std::atomic_exchange_explicit`
(your compiler should support c++11 features).
* blocking or nonblocking receive mode.
* copy everything and share nothing
(allocate memory and copy the message contents).

## Compilers

* gcc `>=` 4.7
* clang version `>=` 3.3

On Linux, if you want to use clang set the following environment variables
before you run `cmake` command:

    export CC=/usr/bin/clang
    export CXX=/usr/bin/clang++

On Mac OS X, `cmake` chooses clang automatically, but both `__GNUC__` and
`__clang__` macros are defined. so `message_queue` checks `__clang__` macro
first, refer to `message_queue.h` for details.


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
