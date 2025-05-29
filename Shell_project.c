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

void manejador(int sig) {
    int pid;
    int status;
    while ((pid = waitpid(-1, &status, WUNTRACED | WNOHANG | WCONTINUED)) > 0) {
        if (WIFEXITED(status)) {
            job* j = get_item_bypid(jobList, pid);
            if (j == NULL) exit(-1);
            printf("Background process %s (%d) Exited\n", j->command, pid);
			/*printf("Foreground pid: %d, command: %s, Exited, info: %d\n", pid,
				get_item_bypid(jobList, pid)->command, WEXITSTATUS(status));*/
			delete_job(jobList, j);
        } else if (WIFSIGNALED(status)) {
            job* j = get_item_bypid(jobList, pid);
            if (j == NULL) exit(-1);
            printf("Foreground pid: %d, command: %s, Signaled\n", pid, j->command);

            delete_job(jobList, j);
        } else if (WIFSTOPPED(status)) {
            job* j = get_item_bypid(jobList, pid);
            if (j == NULL) exit(-1);
            j->state = STOPPED;
        } else if (WIFCONTINUED(status)) {
            job* j = get_item_bypid(jobList, pid);
            if (j == NULL) exit(-1);
            printf("Background process %s (%d) continued\n", j->command, j->pgid);
            j->state = BACKGROUND;
        }
    }
}

// Comando fg
void fg(const char* n){
	int num;
	if(n == NULL){
		num=1;
	}else{
		num = atoi(n);
	}

	block_SIGCHLD();
	job* aux = get_item_bypos(jobList, num);
	unblock_SIGCHLD();

	int status;
	if(aux == NULL){
		return;
	}
	int pid = aux->pgid;
	char* command = aux->command;

	tcsetpgrp(STDIN_FILENO, pid);

	aux->state = FOREGROUND;

	killpg(aux->pgid, SIGCONT);

	waitpid(pid, &status, WUNTRACED);

	tcsetpgrp(STDIN_FILENO, getpid());

	if (WIFEXITED(status)) {
		printf("Foreground pid: %d, command: %s, Exited, info: %d\n", pid, command, WEXITSTATUS(status));

		block_SIGCHLD();
		delete_job(jobList, aux);
		unblock_SIGCHLD();
	} else if (WIFSIGNALED(status)) {
		printf("Foreground pid: %d, command: %s, Signaled, info: %d\n", pid, command, WIFSIGNALED(status));

		block_SIGCHLD();
		delete_job(jobList, aux);
		unblock_SIGCHLD();
	} else if (WIFSTOPPED(status)) {
		printf("Foreground pid: %d, command: %s, Suspended, info: %d\n", pid, command, WSTOPSIG(status));
		
		block_SIGCHLD();
		aux->state = STOPPED; // como esta parado cambiamos el estado a STOPPED
		unblock_SIGCHLD();
	}
}

// Comando bg 
void bg(const char* n){
	int num;
	if(n == NULL){
		num=1;
	}else{
		num = atoi(n);
	}

	block_SIGCHLD();
	job* aux = get_item_bypos(jobList, num);
	if(aux == NULL){
		unblock_SIGCHLD();
		return;
	}

	aux->state = BACKGROUND;
	unblock_SIGCHLD();
	killpg(aux->pgid, SIGCONT);

	printf("Background job running ,pid: %d, command: %s\n", aux->pgid, aux->command);
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
	FILE *infile, *outfile; 

	while (1)   /* Program terminates normally inside get_command() after ^D is typed*/
	{   		
		ignore_terminal_signals();
		printf("COMMAND->");
		fflush(stdout);
		signal(SIGCHLD, manejador); // cada vez que se llame al SIGCHLD lo controlamos desde el manejador
		// T2: Ignorar señales del teminal
		get_command(inputBuffer, MAX_LINE, args, &background);  /* get next command */
		
		parse_redirections(args, &file_in, &file_out);

		if(args[0]==NULL) continue;   // if empty command
		
		// T2: Comando interno
		// comando CD
		if(strcmp(args[0], "cd") == 0){
			chdir(args[1]);
			continue;
		}

		// T4: Nuevos comandos internos
		// comando jobs
		if(strcmp(args[0], "jobs") == 0){
			if(empty_list(jobList)){
				printf("Lista de trabajos vacia\n");
			}
			print_job_list(jobList);
			continue;
		}

		// Comando fg
		if(strcmp(args[0], "fg") == 0){
			fg(args[1]);
			continue;
		}

		if(strcmp(args[0], "bg") == 0){
			bg(args[1]);
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

			// T5: redireccionar entrada y salida
			if(file_in != NULL){
				infile = fopen(file_in, "r");
				if(NULL ==  infile){
					printf("\tError, abriendo: %s\n", file_in);
					return (-1);
				}

				int fnum1 = fileno(infile);
				int fnum2 = fileno(stdin);
				int redirection_success = dup2(fnum1, fnum2);

				if(redirection_success == -1){
					printf("\tError: redireccionando entrada\n");
					return (-1);
				}
			}

			if(file_out != NULL){
				outfile = fopen(file_out, "w");
				if(NULL == outfile){
					printf("\tError, abriendo: %s\n", file_out);
					return (-1);
				}

				int fnum1 = fileno(outfile);
				int fnum2 = fileno(stdout);
				int redirection_success = dup2(fnum1, fnum2);

				if(redirection_success == -1){
					printf("\tError: redireccionando entrada\n");
					return (-1);
				}
			}

			execvp(args[0], args);

			// Si execvp falla
			printf("Error, command not found: %s\n", args[0]);
			
			if (infile != NULL) fclose(infile);
            if (outfile != NULL) fclose(outfile);

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
				printf("Background job running... pid: %d, Command: %s\n", pid_fork, args[0]);
				// T3: añadimos trabajo a la lista
				add_job(jobList, new_job(pid_fork, args[0], BACKGROUND));
			}
		}

	} // end while
}
