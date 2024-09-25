#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

#define NUM_HIJOS 8

void crear_y_manejar_procesos() {
    pid_t pid;
    int estado, status;
    int hijos_terminados = 0;
    int numeros[NUM_HIJOS];
    pid_t pids[NUM_HIJOS];
    

    for (int i = 0; i < NUM_HIJOS; i++) {
        pid = fork();
        if (pid == 0) {
            srand(time(NULL) ^ (getpid() << 16));  
            int numero_aleatorio = rand() % 100; 
            printf("Hijo %d (PID %d): Número aleatorio generado: %d\n", i + 1, getpid(), numero_aleatorio);
            exit(numero_aleatorio);  
        } else if (pid < 0) {
            perror("Error al crear el proceso hijo");
            exit(1);
        } else {
            pids[i] = pid; 
        }
    }

    for (int i = 0; i < NUM_HIJOS; i++) {
        pid_t terminated_pid = waitpid(pids[i], &estado, 0); 
        if (terminated_pid > 0 && WIFEXITED(estado)) {
            int num_hijo = hijos_terminados++;
            numeros[num_hijo] = WEXITSTATUS(estado);  
            printf("Proceso hijo con PID %d terminó con el número: %d\n", terminated_pid, numeros[num_hijo]);

            if (hijos_terminados % 2 == 0) {
                pid = fork();
                if (pid == 0) {
                    printf("Proceso impresor: Números obtenidos hasta el momento:\n");
                    for (int j = 0; j < hijos_terminados; j++) {
                        printf("Hijo %d: %d\n", j + 1, numeros[j]);
                    }
                    exit(0);  
                } else if (pid < 0) {
                    perror("Error al crear el proceso impresor");
                    exit(1);
                }
            }
        }
    }

    while (wait(&status) > 0);

    printf("Todos los procesos han terminado.\n");
}

int main() {
    crear_y_manejar_procesos();
    
    return 0;
}
