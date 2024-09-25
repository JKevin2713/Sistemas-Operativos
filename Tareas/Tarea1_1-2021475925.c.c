#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

void CrearProcesos(int nivel, int max_nivel) {
    if (nivel > max_nivel) {
        return; 
    }

    for (int i = 0; i < 5; i++) {
        pid_t pid = fork();
        
        if (pid < 0) {
            perror("fork"); 
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // Child process
            printf("Nivel: %d, PID: %d, PID del padre: %d\n", nivel, getpid(), getppid());
            CrearProcesos(nivel + 1, max_nivel); 
            exit(0); 
        } else {
            int status;
            waitpid(pid, &status, 0);
        }
    }
}

int main() {
    printf("Nivel: 0, PID: %d, PID del padre: %d\n", getpid(), getppid());
    CrearProcesos(1, 3);
    return 0;
}
