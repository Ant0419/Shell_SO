/**
UNIX Shell Project

Sistemas Operativos
Grados I. Informatica, Computadores & Software
Dept. Arquitectura de Computadores - UMA

Some code adapted from "Fundamentos de Sistemas Operativos", Silberschatz et al.

To compile and run the program:
   $ gcc Shell_project.c job_control.c -o Shell
   $ ./Shell          
	(then type ^D to exit program)

**/

#include "job_control.h"   // remember to compile with module job_control.c 
#include<string.h>

#define MAX_LINE 256 /* 256 chars per line, per command, should be enough. */

job* jobList; // T3: Creamos la lista de trabajos

void manejador(int signal){
	int pid;
	int status;
	
	while ((pid = waitpid(-1, &status, WUNTRACED | WNOHANG)) > 0) {
        if (WIFEXITED(status)) {
            printf("Background process %s (%d) Exited\n",
                   get_item_bypid(jobList, pid)->command, pid);
            delete_job(jobList, get_item_bypid(jobList, pid));
        } else if (WIFSTOPPED(status)) {
            get_item_bypid(jobList, pid)->command = "STOPPED";
        }
    }
}


// -----------------------------------------------------------------------
//                            MAIN          
// -----------------------------------------------------------------------

int main(void)
{
	char inputBuffer[MAX_LINE]; /* buffer to hold the command entered */
	int background;             /* equals 1 if a command is followed by '&' */
	char *args[MAX_LINE/2];     /* command line (of 256) has max of 128 arguments */
	// probably useful variables:
	int pid_fork, pid_wait; 	/* pid for created and waited process */
	int status;             	/* status returned by wait */
	char *file_in, *file_out; 	/* file names for redirection */
	//T3: Inicializamos la lista	
	jobList = new_list("jobList");

	while (1)   /* Program terminates normally inside get_command() after ^D is typed*/
	{   		
		ignore_terminal_signals();
		printf("COMMAND->");
		fflush(stdout);
		signal(SIGCHLD, manejador); // cada vez que se llame al SIGCHLD lo controlamos desde el manejador
		// T2: Ignorar señales del teminal
		get_command(inputBuffer, MAX_LINE, args, &background);  /* get next command */
		
		if(args[0]==NULL) continue;   // if empty command
		
		// T2: Comando interno
		if(strcmp(args[0], "cd") == 0){
			chdir(args[1]);
			continue;
		}

		// T1: Crear proceso hijo
		pid_fork = fork();
		if(pid_fork == 0){
			// Proceso HIJO
			// T2; Nuevo grupo procesos para hijo
			setpgid(getpid(), getpid());
			if(!background){// equivalente a background == 0
				// T2: Dar terminal al hijo
				tcsetpgrp(STDIN_FILENO, getpid());
			}
			// T2: Restaurar señales del terminal
			restore_terminal_signals();
			execvp(args[0], args);

			// Si execvp falla
			printf("Error, command not found: %s\n", args[0]);
			exit(-1);
		}else{
			// Proceso PADRE
			if(!background){ // es lo mismo que background == 0
				// T1: Esperar fin de proceso. Aqui va FOREGROUND -> ejecuta en mi cara
				// en la tarea 2 añadimos WUNTRACED
				pid_wait = waitpid(pid_fork,&status, WUNTRACED);
				
				// Añadimos diferente 
				if (WIFEXITED(status)) {
                    printf("Foreground pid: %d, command: %s, Exited, info: %d\n", pid_wait, args[0],
                           WEXITSTATUS(status));
                } else if (WIFSIGNALED(status)) {
                    printf("Foreground pid: %d, command: %s, Signaled, info: %d\n", pid_wait,
                           args[0], WTERMSIG(status));
                } else if (WIFSTOPPED(status)) {
                    printf("Foreground pid: %d, command: %s, Suspended, info: %d\n", pid_wait,
                           args[0], WSTOPSIG(status));
					// T3: Añadimos comandos si el hijo es suspendido.
					add_job(jobList, new_job(pid_fork, args[0], STOPPED));
                }

				// T2: Recuperar terminal para shell
				tcsetpgrp(STDIN_FILENO, getpid());
				ignore_terminal_signals();
				
			}else{
				// T1: BACKGROUND -> ejecuta en segundo plano -> NO esperar
				printf("Background job running, pid: %d, Command: %s\n", pid_fork, args[0]);
				// T3: añadimos trabajo a la lista
				add_job(jobList, new_job(pid_fork, args[0], BACKGROUND));
			}
		}

	} // end while
}
