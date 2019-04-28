#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define ONE_SECOND 1000000
#define RANGE 10
#define PERIOD 2
#define NUM_THREADS 4

// �������ݽṹ����ͣ������Ϣ
struct Parking_Lot
{
    int *park;                 // ��һ����������bufferģ��ͣ����ͣ��λ
    int capacity;              // ͣ�����ĳ�������
    int occupied;              // ͣ�������г�����Ŀ
    int next_in;               // ��һ�������ĳ���ͣ��λ�ã��� park ���������±��ʾ��
    int next_out;              // ��һ��ȡ�ߵĳ���ͣ��λ�ã��� park ���������±��ʾ��
    int sum_in;                // ��¼ͣ�������복�����ܺ�
    int sum_out;               //��¼��ͣ��������ȥ�ĳ����ܺ�
    int flag;                  //��¼�߳�ID
    pthread_mutex_t lock;      //�������������ýṹ�е����ݱ��̻߳���ķ�ʽʹ��
    pthread_cond_t space;      //��������������ͣ�����Ƿ��п�λ��
    pthread_cond_t car;        //��������������ͣ�����Ƿ��г�
    pthread_barrier_t bar;     //�߳�����
};

static void * producer(void *cp_in);
static void * consumer(void *cp_in);
static void * monitor(void *cp_in);
static void initial(Parking_Lot *cp, int size);

static void initial(Parking_Lot *cp, int size)
{
    cp->occupied = cp->next_in = cp->next_out = cp->sum_in = cp->sum_out = 0;
    cp->capacity = size;        //����ͣ�����Ĵ�С

    cp->park = (int *)malloc(cp->capacity * sizeof(*cp->park));

    // ��ʼ���߳����ϣ�NUM_THREADS ��ʾ�ȴ� NUM_THREADS = 4 ���߳�ͬ��ִ��
    pthread_barrier_init(&cp->bar, NULL, NUM_THREADS);

    if (cp->park == NULL)
    {
        perror("malloc()");
        exit(1);
    }

    srand((unsigned int)getpid());

    pthread_mutex_init(&cp->lock, NULL);        // ��ʼ��ͣ��������
    pthread_cond_init(&cp->space, NULL);        // ��ʼ������ͣ�����Ƿ��п�λ����������
    pthread_cond_init(&cp->car, NULL);          // ��ʼ������ͣ�����Ƿ��г�����������
}

static void* producer(void *park_in)
 {
    Parking_Lot *temp;
    unsigned int seed;
    temp = (Parking_Lot *)park_in;

    // pthread_barrier_wait �����������߳�����ɹ������ȴ������̸߳���
    pthread_barrier_wait(&temp->bar);
    while (1)
    {
        // ���߳��������һ��ʱ�䣬ģ�⳵�������ĵ������
        usleep(rand_r(&seed) % ONE_SECOND);

        pthread_mutex_lock(&temp->lock);

        // ѭ���ȴ�ֱ����ͣ��λ
        while (temp->occupied == temp->capacity)
            pthread_cond_wait(&temp->space, &temp->lock);

        // ����һ�����������������ʶ��
        temp->park[temp->next_in] = rand_r(&seed) % RANGE;

        // ��������������
        temp->flag = pthread_self();
        temp->occupied++;
        temp->next_in++;
        temp->next_in %= temp->capacity;         // ѭ����������ͣ��λ��
        temp->sum_in++;

        // �����е����ڵ��г���ȡ���̣߳������Ƿ��� temp->car ��������
        pthread_cond_signal(&temp->car);

        // �ͷ���
        pthread_mutex_unlock(&temp->lock);

    }
    return ((void *)NULL);

}

static void* consumer(void *park_out)
 {
    Parking_Lot *temp;
    unsigned int seed;
    temp = (Parking_Lot *)park_out;
    pthread_barrier_wait(&temp->bar);
    while(1)
    {
        // ���߳��������һ��ʱ�䣬ģ�⳵�������ĵ������
        usleep(rand_r(&seed) % ONE_SECOND);

        // ��ȡ����ͣ�����ṹ����
        pthread_mutex_lock(&temp->lock);

        /* ���������� temp->occupied ��������ʱ���������Ϊ0��occupied ==0 ����
        pthread_cond_wait ���еĲ�����æ�ȣ��ͷ�����&temp->lock���������߳�ʹ�á�
        ֱ�� &temp->car �����ı�ʱ�ٴν�����ס */
        while (temp->occupied == 0)
            pthread_cond_wait(&temp->car, &temp->lock);

        // ������Ӧ������
        temp->flag = pthread_self();
        temp->occupied--; // ���г�����Ŀ��1
        temp->next_out++;
        temp->next_out %= temp->capacity;
        temp->sum_out++;


        // �����е����ڵ��пտճ�λ���̣߳������Ƿ��� temp->space ��������
        pthread_cond_signal(&temp->space);

        // �ͷű���ͣ�����ṹ����
        pthread_mutex_unlock(&temp->lock);

    }
    return ((void *)NULL);

}

// ���ͣ����״��
static void *monitor(void *park_in)
{
    Parking_Lot *temp;
    temp = (Parking_Lot *)park_in;

    while(1)
    {
        sleep(PERIOD);
        // ��ȡ��
        pthread_mutex_lock(&temp->lock);
        printf("Delta: %d\n", temp->sum_in - temp->sum_out - temp->occupied);
        printf("Number of cars in park: %d\n", temp->occupied);
        // �ͷ���
        pthread_mutex_unlock(&temp->lock);
    }

    return ((void *)NULL);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: %s parksize\n", argv[0]);
        exit(1);
    }
    Parking_Lot mypark;

    initial(&mypark, atoi(argv[1]));        // ��ʼ��ͣ�������ݽṹ

    pthread_t thread_in1, thread_out1, thread_m;               // �����̱߳���
    pthread_t thread_in2, thread_out2;

    pthread_create(&thread_in1, NULL, producer, (void *)&mypark);  // ������ͣ����ͣ���̣߳�������1��
    pthread_create(&thread_out1, NULL, consumer, (void *)&mypark); // ������ͣ����ȡ���̣߳�������1��
    pthread_create(&thread_in2, NULL, producer, (void *)&mypark); // ������ͣ����ͣ���̣߳�������2��
    pthread_create(&thread_out2, NULL, consumer, (void *)&mypark); // ������ͣ����ȡ���̣߳�������2��
    pthread_create(&thread_m, NULL, monitor, (void *)&mypark);  // �������ڼ��ͣ����״�����߳�

    printf("The first producer ID is :%u\n",thread_in1);
    printf("The second producer ID is :%u\n",thread_in2);
    printf("The first consumer ID is :%u\n",thread_out1);
    printf("The second consumer ID is :%u\n",thread_out2);

    // pthread_join �ĵڶ�����������Ϊ NULL����ʾ���������̵߳ķ���״̬�������ȴ�ָ���̣߳���һ������������ֹ
    pthread_join(thread_in1, NULL);
    pthread_join(thread_out1, NULL);
    pthread_join(thread_in2, NULL);
    pthread_join(thread_out2, NULL);
    pthread_join(thread_m, NULL);

    exit(0);
}
