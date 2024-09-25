#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#define NUM_CHILDREN 5

void child_process() {
    while (1) {
        sleep(2);
        printf("Hijo PID: %d\n", getpid());
    }
}

int main() {
    pid_t pids[NUM_CHILDREN];
    int status;

    // Crear 5 procesos hijos
    for (int i = 0; i < NUM_CHILDREN; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (pids[i] == 0) {
            // Proceso hijo
            child_process();
            exit(0);
        }
    }

    // Proceso padre duerme 2 segundos entre cada cancelación
    for (int i = 0; i < NUM_CHILDREN; i++) {
        sleep(2);
        kill(pids[i], SIGTERM); // Enviar señal de terminación
        waitpid(pids[i], &status, 0); // Esperar a que el hijo termine
        printf("Hijo PID: %d terminado\n", pids[i]);
    }

    return 0;
}
