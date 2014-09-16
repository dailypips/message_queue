#ifndef MESSAGE_QUEUE_INCLUDED
#define MESSAGE_QUEUE_INCLUDED

// Assumption:
//  - cache line size is 64 bytes.
// On Linux, you can check cache line size:
// /sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size
//
#define MQ_CACHE_LINE_SIZE 64

// for c++11 features.
#ifdef __clang__
#if (__clang_major__ < 3) || ((__clang_major__ == 3) && (__clang_minor__ < 3))
#error "requires clang version >= 3.3"
#endif
#endif // #ifdef __clang__

#ifdef __GNUC__
#if (__GNUC__ < 4) || (__GNUC__ == 4 && __GNUC_MINOR__ < 8)
#error "requires gcc version >= 4.8"
#endif
#endif // #ifdef __GNUC__

#include <cstdint>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#define MQ_LOCK(x) do {} \
    while (std::atomic_exchange_explicit((&x), true, \
             std::memory_order_acquire))

#define MQ_UNLOCK(x) std::atomic_exchange_explicit((&x), false, \
             std::memory_order_release)

#define MQ_FLAG_RECV_BLOCK     (1 << 0)
#define MQ_FLAG_RECV_NONBLOCK  (1 << 1)

class ReceiveBlock {};
class ReceiveNonblock {};

struct ReceiveBlockType {
    enum {flag = MQ_FLAG_RECV_BLOCK};
};
struct ReceiveNonblockType {
    enum {flag = MQ_FLAG_RECV_NONBLOCK};
};

template <typename T>
struct MessageQueueTraits;

template <>
struct MessageQueueTraits<ReceiveBlock>
{
    typedef ReceiveBlockType mode_;
};

template <>
struct MessageQueueTraits<ReceiveNonblock>
{
    typedef ReceiveNonblockType mode_;
};

template <typename T, typename Traits = MessageQueueTraits<ReceiveBlock> >
struct MessageQueue {
public:
    MessageQueue() :
        receive_mutex_(),
        receive_wait_cond_(),
        receive_wait_mutex_(),
        n_receiver_(0),
        first_(),
        receiveLock_(),
        last_(),
        sendLock_()
    {
        static_assert(
            ((sizeof(T*) + sizeof(bool) + sizeof(std::atomic<Node*>)) <
            MQ_CACHE_LINE_SIZE),
            "the size of node data should be smaller than cache line size");
        static_assert((sizeof(std::mutex) < MQ_CACHE_LINE_SIZE),
            "the size of mutex should be smaller than cache line size");
        static_assert((sizeof(std::condition_variable) < MQ_CACHE_LINE_SIZE),
            "the size of condition variable should be smaller than cache line "
            "size");

        first_ = last_ = new Node(nullptr);
        sendLock_ = receiveLock_ = false;
    }

    ~MessageQueue()
    {
        while (first_ != nullptr) {
            Node* tmp = first_.load();
            first_.store(tmp->next_);
            delete tmp->value_;
            delete tmp;
        }
    }

    void Send(const T& t)
    {
        Node* tmp = new Node(new T(t));
        MQ_LOCK(sendLock_);
        (last_.load())->next_ = tmp;
        last_ = tmp;
        MQ_UNLOCK(sendLock_);

        if (Traits::mode_::flag & MQ_FLAG_RECV_NONBLOCK) {
            return;
        } else {
            std::lock_guard<std::mutex> lk(receive_wait_mutex_);
            if (n_receiver_ > 0) {
                receive_wait_cond_.notify_all();
            }
        }
    }

    bool Receive(T& data)
    {
        // use low lock (spin w/ CAS).
        if (Traits::mode_::flag & MQ_FLAG_RECV_NONBLOCK) {
            MQ_LOCK(receiveLock_);
            Node* first = first_;
            Node* next = (first_.load())->next_;
            if (next != nullptr) {
                T* val = next->value_;
                next->value_ = nullptr;
                first_ = next;
                MQ_UNLOCK(receiveLock_);
                data = *val;
                delete val;
                delete first;
                return true;
            }
            MQ_UNLOCK(receiveLock_);
            return false;
        } else {
            // use mutex
            do {
                {
                    std::lock_guard<std::mutex> lk(receive_mutex_);
                    Node* first = first_;
                    Node* next = (first_.load())->next_;

                    if (next != nullptr) {
                        T* val = next->value_;
                        next->value_ = nullptr;
                        first_ = next;
                        data = *val;
                        delete val;
                        delete first;
                        return true;
                    }
                }

                // wait data
                {
                    std::unique_lock<std::mutex> lk(receive_wait_mutex_);
                    n_receiver_++;
                    receive_wait_cond_.wait(lk,
                            [this]{return (n_receiver_ > 0);});
                    n_receiver_--;
                }
            } while (1);
        }
    }

private:
    // non-copyable
    MessageQueue(const MessageQueue &other);
    const MessageQueue &operator=(const MessageQueue &other);

    struct Node {
        Node(T* val) : value_(val), next_(nullptr) {}
        T* value_;
        std::atomic<Node*> next_;
        uint8_t pad[MQ_CACHE_LINE_SIZE - sizeof(T*) -
                    sizeof(std::atomic<Node*>)];
    };

    uint8_t pad0[MQ_CACHE_LINE_SIZE];

    std::mutex receive_mutex_;
    uint8_t pad1[MQ_CACHE_LINE_SIZE - sizeof(receive_mutex_)];

    std::condition_variable receive_wait_cond_;
    uint8_t pad2[MQ_CACHE_LINE_SIZE - sizeof(receive_wait_cond_)];

    std::mutex receive_wait_mutex_;
    uint8_t pad3[MQ_CACHE_LINE_SIZE - sizeof(receive_wait_mutex_)];

    int n_receiver_;
    uint8_t pad5[MQ_CACHE_LINE_SIZE - sizeof(n_receiver_)];

    std::atomic<Node*> first_;
    uint8_t pad6[MQ_CACHE_LINE_SIZE - sizeof(first_)];

    std::atomic<bool> receiveLock_;
    uint8_t pad7[MQ_CACHE_LINE_SIZE - sizeof(receiveLock_)];

    std::atomic<Node*> last_;
    uint8_t pad8[MQ_CACHE_LINE_SIZE - sizeof(last_)];

    std::atomic<bool> sendLock_;
    uint8_t pad9[MQ_CACHE_LINE_SIZE - sizeof(sendLock_)];
};

#endif // !MESSAGE_QUEUE_INCLUDED
