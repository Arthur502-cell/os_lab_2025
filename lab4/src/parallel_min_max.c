
// ./parallel_min_max --seed 1 --array_size 1000 --pnum 4 --timeout 2

#define _DEFAULT_SOURCE
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <getopt.h>
#include "find_min_max.h"
#include "utils.h"

//
static volatile sig_atomic_t timeout_expired = 0;

static void alarm_handler(int signum) {
    (void)signum;
    timeout_expired = 1;
}

int main(int argc, char **argv) {
    int seed = -1;
    int array_size = -1;
    int pnum = -1;
    int timeout = -1;          
    bool timeout_set = false;
    bool with_files = false;

    
    static struct option options[] = {
        {"seed", required_argument, 0, 0},
        {"array_size", required_argument, 0, 0},
        {"pnum", required_argument, 0, 0},
        {"timeout", required_argument, 0, 0},
        {"by_files", no_argument, 0, 'f'},
        {0, 0, 0, 0}
    };

    while (true) {
        int option_index = 0;

        int c = getopt_long(argc, argv, "f", options, &option_index);

        if (c == -1) {
            break;
        }

        switch (c) {
            case 0:
                switch (option_index) {
                    case 0:
                        seed = atoi(optarg);
                        break;
                    case 1:
                        array_size = atoi(optarg);
                        break;
                    case 2:
                        pnum = atoi(optarg);
                        break;
                    case 3:
                        timeout = atoi(optarg);
                        timeout_set = true;
                        break;
                    default:
                        fprintf(stderr, "Unknown option\n");
                        return 1;
                }
                break;

            case 'f':
                with_files = true;
                break;

            case '?':
                fprintf(stderr, "Unknown or incomplete option\n");
                return 1;

            default:
                return 1;
        }
    }

    if (optind < argc) {
        fprintf(stderr, "Unexpected argument: %s\n", argv[optind]);
        return 1;
    }

    if (seed < 0 || array_size <= 0 || pnum <= 0) {
        fprintf(stderr,
                "Usage: %s --seed NUM --array_size NUM --pnum NUM "
                "[--timeout SEC] [--by_files]\n",
                argv[0]);
        return 1;
    }

    if (timeout_set && timeout <= 0) {
        fprintf(stderr, "Timeout must be a positive number of seconds\n");
        return 1;
    }

    
    int *array = malloc(sizeof(int) * (size_t)array_size);

    if (array == NULL) {
        perror("malloc");
        return 1;
    }

    
    GenerateArray(array, (unsigned int)array_size, (unsigned int)seed);

    
    if (pnum > array_size) {
        pnum = array_size;
    }

    // Массив PID дочерних процессов 
    pid_t *child_pids = calloc((size_t)pnum, sizeof(pid_t));

    if (child_pids == NULL) {
        perror("calloc");
        free(array);
        return 1;
    }

    int (*pipes)[2] = NULL;

    if (!with_files) {
        pipes = malloc(sizeof(int[2]) * (size_t)pnum);

        if (pipes == NULL) {
            perror("malloc");
            free(child_pids);
            free(array);
            return 1;
        }

        for (int i = 0; i < pnum; i++) {
            if (pipe(pipes[i]) == -1) {
                perror("pipe");

                for (int j = 0; j < i; j++) {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }

                free(pipes);
                free(child_pids);
                free(array);
                return 1;
            }
        }
    }

    // Время
    struct timeval start_time;
    gettimeofday(&start_time, NULL);

    int active_child_processes = 0;

    for (int i = 0; i < pnum; i++) {
        pid_t child_pid = fork();

        if (child_pid == -1) {
            perror("fork");
            free(array);
            free(pipes);
            free(child_pids);
            return 1;
        }

        if (child_pid == 0) {
            
            unsigned int begin =
                (unsigned int)((long long)i * array_size / pnum);

            unsigned int end =
                (unsigned int)((long long)(i + 1) * array_size / pnum);

           
            struct MinMax result = GetMinMax(array, begin, end);

            //sleep(30);

            if (with_files) {
                char filename[64];

                snprintf(filename, sizeof(filename),
                         "min_max_%d.tmp", i);

                FILE *file = fopen(filename, "wb");

                if (file == NULL) {
                    perror("fopen");
                    _exit(1);
                }

                size_t written =
                    fwrite(&result, sizeof(result), 1, file);

                if (written != 1) {
                    perror("fwrite");
                    fclose(file);
                    _exit(1);
                }

                if (fclose(file) != 0) {
                    perror("fclose");
                    _exit(1);
                }
            } else {
                // Запись результата в pipe
                for (int j = 0; j < pnum; j++) {
                    close(pipes[j][0]);

                    if (j != i) {
                        close(pipes[j][1]);
                    }
                }

                const char *data = (const char *)&result;
                size_t remaining = sizeof(result);

                while (remaining > 0) {
                    ssize_t written =
                        write(pipes[i][1], data, remaining);

                    if (written == -1) {
                        perror("write");
                        _exit(1);
                    }

                    data += written;
                    remaining -= (size_t)written;
                }

                close(pipes[i][1]);
            }

            
            _exit(0);
        }

        
        child_pids[i] = child_pid;
        active_child_processes++;
    }

    // Закрываем концы pipe, предназначенные для записи
    if (!with_files) {
        for (int i = 0; i < pnum; i++) {
            close(pipes[i][1]);
        }
    }

    // Запуск таймера, если задан таймаут
    if (timeout_set) {
        signal(SIGALRM, alarm_handler);
        alarm((unsigned int)timeout);
    }

    
    int child_error = 0;
    bool timed_out = false;

    while (active_child_processes > 0) {
        int status;
        pid_t finished_pid;

        if (timeout_set) {
            // Неблокирующее ожидание
            finished_pid = waitpid(-1, &status, WNOHANG);

            if (finished_pid == 0) {
                // Никто ещё не завершился
                if (timeout_expired) {
                    // "Убиваем" всех оставшихся детей
                    for (int i = 0; i < pnum; i++) {
                        if (child_pids[i] > 0) {
                            kill(child_pids[i], SIGKILL);
                        }
                    }

                    timed_out = true;

                    // Собираем убитые процессы, чтобы не было зомби
                    while (active_child_processes > 0) {
                        pid_t p = waitpid(-1, &status, 0);

                        if (p == -1) {
                            if (errno == EINTR) {
                                continue;
                            }
                            break;
                        }

                        active_child_processes--;
                    }

                    break;
                }

                usleep(1000);
                continue;
            }
        } else {
            finished_pid = wait(&status);
        }

        if (finished_pid == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("wait");
            child_error = 1;
            break;
        }

        
        for (int i = 0; i < pnum; i++) {
            if (child_pids[i] == finished_pid) {
                child_pids[i] = 0;
                break;
            }
        }

        

        active_child_processes--;
    }

    // Выключаем таймер, если все успели завершиться раньше
    if (timeout_set) {
        alarm(0);
    }

    if (timed_out || child_error) {
        if (timed_out) {
            fprintf(stderr,"Timeout %d s expired: child processes were killed\n", timeout);
        } else {
            fprintf(stderr, "One or more child processes failed\n");
        }

        if (!with_files) {
            for (int i = 0; i < pnum; i++) {
                close(pipes[i][0]);
            }
        } else {
            for (int i = 0; i < pnum; i++) {
                char filename[64];
                snprintf(filename, sizeof(filename), "min_max_%d.tmp", i);
                remove(filename);
            }
        }

        free(pipes);
        free(child_pids);
        free(array);
        return 1;
    }

    
    struct MinMax min_max;
    min_max.min = INT_MAX;
    min_max.max = INT_MIN;

    
    for (int i = 0; i < pnum; i++) {
        struct MinMax result;
        bool read_success = false;

        if (with_files) {
            
            char filename[64];

            snprintf(filename, sizeof(filename),
                     "min_max_%d.tmp", i);

            FILE *file = fopen(filename, "rb");

            if (file == NULL) {
                perror("fopen");
                free(pipes);
                free(child_pids);
                free(array);
                return 1;
            }

            read_success =
                fread(&result, sizeof(result), 1, file) == 1;

            fclose(file);
            remove(filename);
        } else {
            
            char *data = (char *)&result;
            size_t remaining = sizeof(result);

            while (remaining > 0) {
                ssize_t bytes_read =
                    read(pipes[i][0], data, remaining);

                if (bytes_read <= 0) {
                    if (bytes_read == -1) {
                        perror("read");
                    } else {
                        fprintf(stderr, "Unexpected end of pipe\n");
                    }

                    break;
                }

                data += bytes_read;
                remaining -= (size_t)bytes_read;
            }

            read_success = (remaining == 0);
            close(pipes[i][0]);
        }

        if (!read_success) {
            fprintf(stderr, "Failed to read result from child %d\n", i);
            free(pipes);
            free(child_pids);
            free(array);
            return 1;
        }

        
        if (result.min < min_max.min) {
            min_max.min = result.min;
        }

        
        if (result.max > min_max.max) {
            min_max.max = result.max;
        }
    }

    
    struct timeval finish_time;
    gettimeofday(&finish_time, NULL);

    double elapsed_time =
        (finish_time.tv_sec - start_time.tv_sec) * 1000.0;

    elapsed_time +=
        (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

    
    free(pipes);
    free(child_pids);
    free(array);

    
    printf("Min: %d\n", min_max.min);
    printf("Max: %d\n", min_max.max);
    printf("Elapsed time: %fms\n", elapsed_time);

    return 0;
}