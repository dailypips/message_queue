#ifndef MESSAGE_QUEUE_INCLUDED
#define MESSAGE_QUEUE_INCLUDED

#include <stdint.h>
#include <pthread.h>

#ifdef __GNUC__
#if (__GNUC__ < 4) || (__GNUC__ == 4 && __GNUC_MINOR__ < 1)
#error "requires gcc version >= 4.1"
#endif
#define CAS(ptr, oldval, newval)\
        __sync_val_compare_and_swap(ptr, oldval, newval)
#else
#error "compare and swap not supported."
#endif

#define MQ_CAS_LOCK(x) do {} while (CAS(&(x), 0, 1) == 1)
#define MQ_CAS_UNLOCK(x) do {CAS(&(x), 1, 0);} while (0)

template<bool> struct CompileTimeAssertion;
template<> struct CompileTimeAssertion<true> {};

#define STATIC_ASSERT(expr, msg)\
{\
    ::CompileTimeAssertion<((expr) != 0)> ASSERTION_##msg;\
    (void)ASSERTION_##msg;\
}


// Assumption:
//  - 32bit pointer and 32bit integer operation is atomic.
//  - cache line size is 64 bytes.
// Consider using std::atomic (e.g., std::atomic<Node*> ...).
// On Linux, you can check cache line size:
// /sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size
//
#define MQ_CACHE_LINE_SIZE 64

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
        receive_wait_(false),
        n_receiver_(0)
    {
        STATIC_ASSERT(((sizeof(T*) + sizeof(bool) + sizeof(Node*)) <
                      MQ_CACHE_LINE_SIZE),
                      Size_Of_Node_Data_Should_Be_Smaller_Than_CacheLine_Size);
        STATIC_ASSERT((sizeof(pthread_mutex_t) < MQ_CACHE_LINE_SIZE),
                      Size_Of_Mutex_Should_Be_Smaller_Than_CacheLine_Size);
        STATIC_ASSERT((sizeof(pthread_cond_t) < MQ_CACHE_LINE_SIZE),
                      Size_Of_Cond_Should_Be_Smaller_Than_CacheLine_Size);

        pthread_mutex_init(&receive_mutex_, NULL);
        pthread_cond_init(&receive_wait_cond_, NULL);
        pthread_mutex_init(&receive_wait_mutex_, NULL);
        first_ = last_ = new Node(NULL);
        sendLock_ = receiveLock_ = 0;
    }

    ~MessageQueue()
    {
        pthread_mutex_destroy(&receive_mutex_);
        pthread_cond_destroy(&receive_wait_cond_);
        pthread_mutex_destroy(&receive_wait_mutex_);

        while (first_ != NULL) {
            Node* tmp = first_;
            first_ = tmp->next_;
            delete tmp->value_;
            delete tmp;
        }
    }

    void Send(const T& t)
    {
        Node* tmp = new Node(new T(t));
        MQ_CAS_LOCK(sendLock_);
        last_->next_ = tmp;
        last_ = tmp;
        MQ_CAS_UNLOCK(sendLock_);

        if (Traits::mode_::flag & MQ_FLAG_RECV_NONBLOCK) {
            return;
        } else {
            pthread_mutex_lock(&receive_wait_mutex_);
            if (receive_wait_) {
                receive_wait_ = false;
                pthread_cond_broadcast(&receive_wait_cond_);
            }
            pthread_mutex_unlock(&receive_wait_mutex_);
        }
    }

    bool Receive(T& data)
    {
        // use low lock (spin w/ CAS).
        if (Traits::mode_::flag & MQ_FLAG_RECV_NONBLOCK) {
            MQ_CAS_LOCK(receiveLock_);
            Node* first = first_;
            Node* next = first_->next_;
            if (next != NULL) {
                T* val = next->value_;
                next->value_ = NULL;
                first_ = next;
                MQ_CAS_UNLOCK(receiveLock_);
                data = *val;
                delete val;
                delete first;
                return true;
            }
            MQ_CAS_UNLOCK(receiveLock_);
            return false;
        } else {
            // use mutex
            do {
                pthread_mutex_lock(&receive_mutex_);
                Node* first = first_;
                Node* next = first_->next_;

                if (next != NULL) {
                    T* val = next->value_;
                    next->value_ = NULL;
                    first_ = next;
                    pthread_mutex_unlock(&receive_mutex_);
                    data = *val;
                    delete val;
                    delete first;
                    return true;
                }
                pthread_mutex_unlock(&receive_mutex_);

                // wait data
                pthread_mutex_lock(&receive_wait_mutex_);
                receive_wait_ = true;
                n_receiver_++;
                while (receive_wait_) {
                    pthread_cond_wait(&receive_wait_cond_,
                                      &receive_wait_mutex_);
                }
                n_receiver_--;
                if (n_receiver_ > 0) {
                    receive_wait_ = true;
                }
                pthread_mutex_unlock(&receive_wait_mutex_);
            } while (1);
        }
    }

private:
    // non-copyable
    MessageQueue(const MessageQueue &other);
    const MessageQueue &operator=(const MessageQueue &other);

    struct Node {
        Node(T* val) : value_(val), next_(NULL) {}
        T* value_;
        Node* next_;
        uint8_t pad[MQ_CACHE_LINE_SIZE - sizeof(T*) - sizeof(bool) -
                    sizeof(Node*)];
    };

    uint8_t pad0[MQ_CACHE_LINE_SIZE];

    pthread_mutex_t receive_mutex_;
    uint8_t pad1[MQ_CACHE_LINE_SIZE - sizeof(receive_mutex_)];

    pthread_cond_t receive_wait_cond_;
    uint8_t pad2[MQ_CACHE_LINE_SIZE - sizeof(receive_wait_cond_)];

    pthread_mutex_t receive_wait_mutex_;
    uint8_t pad3[MQ_CACHE_LINE_SIZE - sizeof(receive_wait_mutex_)];

    bool receive_wait_;
    uint8_t pad4[MQ_CACHE_LINE_SIZE - sizeof(receive_wait_)];

    int n_receiver_;
    uint8_t pad5[MQ_CACHE_LINE_SIZE - sizeof(n_receiver_)];

    Node* volatile first_;
    uint8_t pad6[MQ_CACHE_LINE_SIZE - sizeof(first_)];

    volatile int32_t receiveLock_;
    uint8_t pad7[MQ_CACHE_LINE_SIZE - sizeof(receiveLock_)];

    Node* volatile last_;
    uint8_t pad8[MQ_CACHE_LINE_SIZE - sizeof(last_)];

    volatile int32_t sendLock_;
    uint8_t pad9[MQ_CACHE_LINE_SIZE - sizeof(sendLock_)];
};

#endif // !MESSAGE_QUEUE_INCLUDED
