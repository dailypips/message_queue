#include "message_queue.h"
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <pthread.h>

struct Data {
    int sequence;
    int value;
};

typedef MessageQueue<Data, MessageQueueTraits<ReceiveNonblock> > DataQueue;

static void *consumer_routine(void *arg)
{
    using namespace std;

    DataQueue *dq = static_cast<DataQueue*>(arg);
    Data d;
    bool ret;

    while (true) {
        ret = dq->Receive(d);
        if (ret) {
            printf("id=%#lx received sequence=%d, value=%d\n",
                    pthread_self(), d.sequence, d.value);
        } else {
            printf("id=%#lx Receive failure; sleep for 1 sec.\n",
                    pthread_self());
            sleep(1);
        }
    }
    return NULL;
}

static void *producer_routine(void *arg)
{
    using namespace std;

    DataQueue *dq = static_cast<DataQueue*>(arg);
    int sequence = 0;

    while (true) {
        Data d;
        d.sequence = sequence++;
        d.value = sequence;
        dq->Send(d);
        printf("id=%#lx send sequence=%d, value=%d\n",
                pthread_self(), d.sequence, d.value);
        sleep(1);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    using namespace std;

    int ret;
    pthread_t consumer1;
    pthread_t consumer2;
    pthread_t producer1;
    pthread_t producer2;

    DataQueue dataQueue;

    ret = pthread_create(&consumer1, NULL, consumer_routine, &dataQueue);
    if (ret != 0) {
        printf("pthread_create failure.\n");
        exit(EXIT_FAILURE);
    }

    ret = pthread_create(&consumer2, NULL, consumer_routine, &dataQueue);
    if (ret != 0) {
        printf("pthread_create failure.\n");
        exit(EXIT_FAILURE);
    }

    ret = pthread_create(&producer1, NULL, producer_routine, &dataQueue);
    if (ret != 0) {
        printf("pthread_create failure.\n");
        exit(EXIT_FAILURE);
    }

    ret = pthread_create(&producer2, NULL, producer_routine, &dataQueue);
    if (ret != 0) {
        printf("pthread_create failure.\n");
        exit(EXIT_FAILURE);
    }

    while (true) {
        sleep(1);
    }
    return 0;
}
