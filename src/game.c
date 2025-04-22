#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mqueue.h>
#include <string.h>
#include <stdbool.h>

#define CHECK(x) \
    do { \
        if ((x) == -1) { \
            perror(#x); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

void lead(mqd_t send, mqd_t get, const char* procname, int N) {
    printf("%s: I've picked a number from 1 to %d. Try to guess it!\n", procname, N);

    srand(time(NULL));
    int number = rand() % (N + 1);
    int response = 2;
    printf("\tNumber: %d\n", number);

    CHECK(mq_send(send, (const char*)&response, sizeof(response), 0));

    int attempt;
    int count = 0;
    clock_t time1 = clock();

    do {
        count++;

        while(1) {
            CHECK(mq_receive(get, (char*)&attempt, sizeof(attempt), NULL)); 
            if (attempt > 0) break;
        }

        if (attempt != number) {
            response = 0;
            printf("%s: No, try again!\n", procname);
        } else {
            response = 1;
            clock_t time2 = clock();
            double time_c = (double)(time2 - time1) / CLOCKS_PER_SEC * 1000;
            printf("%s: Yes, that's correct!\n", procname);
            printf("Attempts: %d\t Time: %.2f ms\n\n", count, time_c);
        }

        CHECK(mq_send(send, (const char*)&response, sizeof(response), 0));
    } while(attempt != number);
}

void guess(mqd_t send, mqd_t get, const char* procname, int N) {
    sleep(2);
    int response;
    srand(time(NULL));

    while(1) {
        CHECK(mq_receive(get, (char*)&response, sizeof(response), NULL)); 
        if (response == 2) break;
    }

    int* used = malloc(N * sizeof(int));
    int used_count = 0;

    while(response != 1) {
        int attempt;
        int found;
        
        do {
            attempt = rand() % N + 1;
            found = 0;

            for (int i = 0; i < used_count; i++) {
                if (used[i] == attempt) {
                    found = 1;
                    break;
                }
            }

            if (!found) {
                used[used_count++] = attempt;
                break;
            }
        } while(1);

        printf("%s: Is it %d?\n", procname, attempt);
        CHECK(mq_send(send, (const char*)&attempt, sizeof(attempt), 0));

        while(1) {
            CHECK(mq_receive(get, (char*)&response, sizeof(response), NULL)); 
            if (response == 1 || response == 0) break;
        }
    }

    free(used);
}

int main(int argc, char* argv[]) {
    int iterations = 0;
    int N = 0;
    
    if (argc > 1) {
        N = abs(atoi(argv[1]));
    } else {
        N = 3;
    }

    if (argc > 2) {
        iterations = abs(atoi(argv[2]));
    } else {
        iterations = 2;
    }

    printf("Iterations: %d\n", iterations);
    printf("Range: %d\n", N);
    
    bool leads = true;
    mqd_t a_mq, b_mq;
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(int);

    pid_t pid = fork();
    CHECK(pid);

    for (int i = 0; i < iterations; i++) {
        if(pid > 0) {
            a_mq = mq_open("/queuea", O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR, &attr);
            b_mq = mq_open("/queueb", O_RDONLY | O_CREAT, S_IRUSR | S_IWUSR, &attr);
            if (leads)
                lead(a_mq, b_mq, "proc1", N);
            else
                guess(a_mq, b_mq, "proc1", N);
        } else {
            a_mq = mq_open("/queuea", O_RDONLY | O_CREAT, S_IRUSR | S_IWUSR, &attr);
            b_mq = mq_open("/queueb", O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR, &attr);
            if (!leads)
                lead(b_mq, a_mq, "proc2", N);
            else
                guess(b_mq, a_mq, "proc2", N);
        }
        leads = !leads;
        mq_close(a_mq);
        mq_close(b_mq);
    }

    mq_unlink("/queuea");
    mq_unlink("/queueb");
    return 0;
}