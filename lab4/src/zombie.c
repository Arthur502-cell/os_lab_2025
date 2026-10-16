
// gcc zombie.c -o zombie
// ps aux | grep defunct

#define _DEFAULT_SOURCE
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

// Показать дочерние процессы текущего процесса (колонка STAT: Z = зомби)
static void show_children(void) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd),
             "ps -o pid,ppid,stat,comm --ppid %d | grep -v ' sh$'",
             (int)getpid());
    fflush(stdout);
    system(cmd);
}


static void spawn_children(int n) {
    for (int i = 0; i < n; i++) {
        fflush(stdout);  
        pid_t pid = fork();

        if (pid == -1) {
            perror("fork");
            exit(1);
        }

        if (pid == 0) {
            // Дочерний процесс: сразу завершается с кодом i
            printf("  Child %d (PID %d) exits\n", i, (int)getpid());
            fflush(stdout);
            _exit(i);
        }
    }
}

int main(int argc, char **argv) {
    int children = 3;  
    int delay = 10;    

    if (argc > 1) {
        children = atoi(argv[1]);
    }
    if (argc > 2) {
        delay = atoi(argv[2]);
    }
    if (children <= 0 || delay < 0) {
        fprintf(stderr, "Usage: %s [children] [delay_sec]\n", argv[0]);
        return 1;
    }

    printf("Parent PID: %d\n\n", (int)getpid());

    
    printf("=== 1. Children exit, parent does NOT call wait ===\n");
    spawn_children(children);

    sleep(1);  

    printf("\nChildren of the parent (STAT = Z means zombie):\n");
    show_children();

    printf("\nZombies will exist for %d s. In another terminal run:\n", delay);
    printf("  ps -o pid,ppid,stat,comm --ppid %d\n", (int)getpid());
    printf("  or: ps aux | grep defunct\n\n");
    sleep((unsigned int)delay);

   
    printf("=== 2. Parent calls wait() and reaps zombies ===\n");
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, 0)) > 0) {
        if (WIFEXITED(status)) {
            printf("  Reaped PID %d, exit code %d\n",
                   (int)pid, WEXITSTATUS(status));
        }
    }

    printf("\nChildren after wait (no zombies):\n");
    show_children();

    sleep(1);

    printf("\nChildren of the parent (no Z expected):\n");
    show_children();

    printf("\nDone.\n");
    return 0;
}