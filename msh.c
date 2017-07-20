#include "common.h"

/* Global Declaration */
pid_t pid;
int i_flag, c_flag, exit_status;
pdt_t *head = NULL;
char command[200], path[100];

/* Maintain Priority */
void maintain_priority(void)
{
	int flag_1 = 0, flag_2 = 0;
	pdt_t *ptr = head, *last = head;

	if (ptr->link == NULL)
	{
		last->prior = '+';
		return;
	}

	/* Go to last node */
	while (last->link != NULL)
	{
		last->prior = ' ';
		last = last->link;
	}
	last->prior = ' ';

	/* Find the first stop process and give priority '+' */
	if (strcmp(last->state, "Stopped") == 0)
	{
		last->prior = '+';
		last = last->prev;
	
		ptr = last;
		/* Find the second stop process and give priority '-' */
		while (ptr != NULL && strcmp(ptr->state, "Stopped") != 0)
			ptr = ptr->prev;
		
		if (ptr != NULL)
			ptr->prior = '-';
		else
			flag_2 = 1;
	}
	else
	{
		/* Find the first stop process and give priority '+' */
		ptr = last;
		while (ptr != NULL && strcmp(ptr->state, "Stopped") != 0)
			ptr = ptr->prev;

		if (ptr != NULL)
		{
			ptr->prior = '+';
			
			/* Find the second stop process and give priority '-' */
			ptr = ptr->prev;
			while (ptr != NULL && strcmp(ptr->state, "Stopped") != 0)
				ptr = ptr->prev;

			if (ptr != NULL)
				ptr->prior = '-';
			else
				flag_2 = 1;
		}
		else
			flag_1 = 1;
	}

	/* No stop process present then find the 1st runing process from last and give priority '+' */
	if (flag_1)
	{
		ptr = last;
		while (ptr != NULL && strcmp(ptr->state, "Running") != 0)
			ptr = ptr->prev;

		ptr->prior = '+';
		ptr = ptr->prev;

		if (ptr != NULL)
			ptr->prior = '-';
	}

	/* Only one stop process present then find the ruuning process and give the priority '-' */
	if (flag_2)
	{
		ptr = last;
		while (ptr != NULL && strcmp(ptr->state, "Running") != 0)
			ptr = ptr->prev;

		if (ptr != NULL)
			ptr->prior = '-';
	}
}

/* Insert node function */
void insert_node(char *str)
{
	/* Inserting 1st process details */
	if (head == NULL)
	{
		head = malloc(sizeof(pdt_t));
		head->pno = 1;
		head->pid = pid;
		strcpy(head->pname, command);
		strcpy(head->state, str);
		if (strcmp(str, "Running") == 0)
			strcat(head->pname, " &");
		head->prior = '+';
		head->prev = NULL;
		head->link = NULL;
	}
	/* Appending the new process details */
	else
	{
		pdt_t *ptr = head;
		while (ptr->link != NULL)
			ptr = ptr->link;

		ptr->link = malloc(sizeof(pdt_t));
		ptr->link->pno = ptr->pno + 1;
		ptr->link->pid = pid;
		strcpy(ptr->link->pname, command);
		strcpy(ptr->link->state, str);
		if (strcmp(str, "Running") == 0)
			strcat(ptr->link->pname, " &");
		ptr->link->prior = ' ';
		ptr->link->link = NULL;
		ptr->link->prev = ptr;

		/* Maintain the priority */
		maintain_priority();
	}
}

/* Delete Node function */
void delete_node(pid_t dpid)
{
	pdt_t *ptr = head;

	/* Find the node in the list */
	while (ptr != NULL && ptr->pid != dpid)
		ptr = ptr->link;

	/* Node found then remove that node and free the memory */
	if (ptr != NULL)
	{	
		if (ptr != head)
		{
			ptr->prev->link = ptr->link;

			if (ptr->link != NULL)
				ptr->link->prev = ptr->prev;
		}
		else
			head = head->link;

		free(ptr);

		if (head != NULL)
			maintain_priority();
	}
}

/* SIGINT handler */
void int__signal_handler(int signum)
{
	printf("pid : %d\n", pid);
	/* Send kill signal to currently running process  */
	if (pid != 0)
	{
		delete_node(pid);
		kill(pid, SIGKILL);
	}
	/* Print prompt */
	else
	{
		i_flag = 1;
		//printf(MAGENTA "[ %s ] " RESET, prompt);
		//return;
	}
}

/* SIGTSTP handler */
void stop__signal_handler(int signum)
{
	/* Send stop signal to currently running process */
	kill(pid, SIGSTOP);
}

/* SIGCHLD handler */
void child__signal_handler(int signum, siginfo_t *sinfo, void *context)
{
	pdt_t *ptr = head;
	/* Child signal killed set the flag */
	if (sinfo->si_code == CLD_KILLED)
		c_flag = 1;

	/* Child signal killed set the flag and set the exit status */
	if (sinfo->si_code == CLD_EXITED)
	{
		exit_status = sinfo->si_status;
		c_flag = 1;
	
		while (ptr != NULL && ptr->pid != sinfo->si_pid)
			ptr = ptr->link;

		if (ptr != NULL)
		{
			char str[20] = "Exit ";
			sprintf(str + 5, "%d", sinfo->si_status);
			strcpy(ptr->state, str);

			int length = strlen(ptr->pname);

			if (ptr->pname[length-1] == '&' &&  strcmp(command, "fg") == 0)
				c_flag = 1;
			else
				c_flag = 0;
		}
	}

	/* Child signal stopped set the flag and set the state as stopped */
	else if (sinfo->si_code == CLD_STOPPED)
	{
		c_flag = 1;
	
		while (ptr != NULL && ptr->pid != sinfo->si_pid)
			ptr = ptr->link;

		if (ptr != NULL)
		{
			strcpy(ptr->state, "Stopped");
			if (head != NULL)
				maintain_priority();
		}
		else
		{
			insert_node("Stopped"); 

			ptr = head;
			while (ptr->link != NULL)
				ptr = ptr->link;
		}
	
		/* Print process details */
		printf("[%d]%c\t%s\t%s\n", ptr->pno, ptr->prior, ptr->state, ptr->pname);
	}
	
	/* Child signal continued unset the flag and set the state as running */
	else if (sinfo->si_code == CLD_CONTINUED)
	{
		c_flag = 0;

		while (ptr != NULL && ptr->pid != sinfo->si_pid)
			ptr = ptr->link;

		if (ptr != NULL)
		{
			strcpy(ptr->state, "Running");
			maintain_priority();
		}
	}
}

/* Read Inputs Funtion */
int read_inputs(char *buff, char ***argu, int *arguc, int *index)
{
	int i_index = 0, j_index = 0, k_index = 0, count = 0;
	char *string = buff;
	int length = strlen(buff);
	index[k_index++] = 0;

	/* Covert the string into command line arguments and return no.of pipes and store no.of arguments */
	while (i_index <= length)
	{
		if (buff[i_index] == ' ' || buff[i_index] == '\0')
		{
			buff[i_index] = '\0';

			if (j_index == 0)
				*argu = malloc(sizeof(char *));
			else
				*argu = realloc(*argu, (j_index + 1) * sizeof(char *));
					
			if (strcmp(string, "|") == 0)
			{
				index[k_index++] = j_index + 1;
				count++;
				*(*argu + j_index) = NULL;
			}
			else
			{
				*(*argu + j_index) = malloc(strlen(string) + 1);
				strcpy(*(*argu + j_index), string);
			}

			j_index++;
			string = buff + i_index + 1;
		}

		i_index++;
	}

	*argu = realloc(*argu, (j_index + 1) * sizeof(char *));
	*(*argu + j_index) = NULL;
	*arguc = j_index;

	return count;
}

/* Systemcall function */
int call_systemcall(char **argu)
{
	char buff[100];
	
	/* Change Directory */
	if (strcmp(argu[0], "cd") == 0)
	{
		if (argu[1] == NULL)
			chdir("/home/abi");
		else
			chdir(argu[1]);
	}

	/* Get current working directory */
	else if (strcmp(argu[0], "pwd") == 0 && getcwd(buff, 100) != NULL)
		printf("\n%s\n", buff);

	/* Create new directory */
	else if (strcmp(argu[0], "mkdir") == 0)
		mkdir(argu[1], 0777);

	/* Remove directory */	
	else if (strcmp(argu[0], "rmdir") == 0)
		rmdir(argu[1]);

	/* Do nothing return */
	else
		return 0;

	/* Done return */
	return 1;
}

int new_prompt(char *prompt)
{
	/* Set the new prompt by using PS1 */
	if (strncmp("PS1=", command, 4) == 0)
	{
		if (command[4] != ' ' && command[4] != '\t')
			strcpy(prompt, command + 4);
		else
			printf("Invalid Command\n");

		/* Done return */
		return 1;
	}

	/* Do nothing return */
	return 0;
}

/* Back to prompt */
int back_to_prompt(void)
{
	/* command length is zero then print prompt again */
	if (strlen(command) == 0 || i_flag)
	{
		if (i_flag)
		{
			printf("\n");
			i_flag = 0;
		}
		return 1;
	}
	return 0;
}

/* Check echo */
int check_echo(char **argu, char **env)
{
	int index = 0;
	
	if (strcmp(argu[0], "echo") == 0)
	{
		/* Print nothing */
		if (argu[1] == NULL)
			printf("\n");

		/* Print pid */
		else if (strcmp(argu[1], "$$") == 0)
			printf("%d\n", getpid());	
		
		/* Print last exit status */
		else if (strcmp(argu[1], "$?") == 0)
			printf("%d\n", exit_status);	
		
		/* Print the shell name */
		else if (strcmp(argu[1], "$SHELL") == 0)
			printf("%s\n", path);			

		/* Do nothing return */
		else
			return 0;
	}
	/* Do nothing return */
	else 
		return 0;

	/* Done return */
	return 1;
}

/* Fg and Bg process */
int fg_bg_process(void)
{
	pdt_t *ptr = head;
	
	/* Foreground Processes */
	if (strcmp(command, "fg") == 0)
	{
		if (head == NULL)
			printf("fg : Failed...No process in list\n");

		else
		{
			/* Get the process from list */
			while (ptr != NULL && ptr->prior != '+')
				ptr = ptr->link;

			/* Send SIGCONT to that pid and wait for process termination */
			if (ptr != NULL)
			{
				c_flag = 0;
				pid = ptr->pid;
				printf("%s\n", ptr->pname);
				kill(ptr->pid, SIGCONT);

				while (!c_flag);
				c_flag = 0;
			}
		}
	}

	/* Background Processes */
	else if (strcmp(command, "bg") == 0)
	{
		if (head != NULL)
		{
			/* Get the process from list */
			while (ptr != NULL && ptr->prior != '+')
				ptr = ptr->link;

			/* If the process state is stopped then send SIGCONT to that pid */
			if (ptr != NULL && strcmp(ptr->state, "Stopped") == 0)
			{
				strcat(ptr->pname, " &");
				printf("[%d]  %d\n", ptr->pno, ptr->pid);
				kill(ptr->pid, SIGCONT);
			}
			else
				printf("bg : Failed...No backround process\n");
		}
		else
			printf("bg : Failed...No backround process\n");
	}

	/* Print the all Jobs details */
	else if (strcmp(command, "jobs") == 0)
	{
		/* Print the details */
		while (ptr != NULL)
		{
			printf("[%d]%c\t%s\t%s\n", ptr->pno, ptr->prior, ptr->state, ptr->pname);
			ptr = ptr->link;
		}
		
		/* Delete those processes having state as Exit */
		while (1)
		{
			ptr = head;
			while (ptr != NULL && strncmp(ptr->state, "Exit", 4) != 0)
				ptr = ptr->link;
	
			if (ptr != NULL)
				delete_node(ptr->pid);
			else
				break;
		}
	}

	/* Do nothing return */
	else
		return 0;

	/* Done return */
	return 1;
}

/* Minishell Main Function */
int main(int argc, char **argv, char **env)
{
	char **argu;
	int no_pipes, index[100], idx, arguc;
	struct sigaction act_1, act_2, act_3;
	char prompt[20] = "minishell";

	getcwd(path, 100);
	memset(&act_1, 0, sizeof (act_1));
	memset(&act_2, 0, sizeof (act_2));
	memset(&act_3, 0, sizeof (act_3));

	/* SIGINT, SIGTSTP and SIGCHLD handlers */
	
	act_3.sa_flags = SA_SIGINFO | SA_RESTART;
	act_3.sa_sigaction = child__signal_handler;

	/* Register the signal handlers */
	sigaction(SIGCHLD, &act_3, NULL);

	while (1)
	{
		act_2.sa_handler = SIG_IGN;
		sigaction(SIGTSTP, &act_2, NULL);
		
		act_1.sa_handler = SIG_IGN;
		sigaction(SIGTSTP, &act_1, NULL);
		
		/* Set pid and command as 0 */
		pid = 0;
		memset(&command, 0, sizeof(command));

		/* Print Promt */
		printf(MAGENTA "[ %s ] " RESET, prompt);
		scanf("%[^\n]",command);
		__fpurge(stdin);
		
		/* Exit the minishell */
		if (strcmp(command, "exit") == 0)
			break;

		act_1.sa_handler = int__signal_handler;
		sigaction(SIGINT, &act_1, NULL);
		act_2.sa_flags = SA_RESTART;
		act_2.sa_handler = stop__signal_handler;
		sigaction(SIGTSTP, &act_2, NULL);
		
		/* new prompt, back_to_prompt and fg_bg_process funtion calls, on success this will return 1 */
		if (new_prompt(prompt) || back_to_prompt() || fg_bg_process()) 
			continue;
		
		/* Convert the command string into Command line arguments */
		no_pipes = read_inputs(command, &argu, &arguc, index);

		/* Pipes count is zero */
		if (no_pipes == 0)
		{
			/* call_systemcall and check_echo function call, On success this will return 1 */
			if (call_systemcall(argu) || check_echo(argu, env))
				continue;

			else
			{
				/* Fork the child */
				switch (pid = fork())
				{
					/* Error handler */
					case -1:

						perror("fork()");
						exit(-1);

					/* Child Process */
					case 0:

						if (strcmp(argu[arguc - 1], "&") == 0)
							argu[arguc - 1] = NULL;

						if (execvp(argu[0], &argu[0]) == -1)
						{
							printf("%s : Command not found\n", command);
							exit(-1);
						}

					/* Parent process */
					default:

						c_flag = 0;

						/* Process not a backround process then wait for the process termination */
						if (strcmp(argu[arguc - 1], "&") != 0)
						{
							while (!c_flag);
							c_flag = 0;
						}
						/* If backround process then insert that process details (state as running) */
						else
						{
							pid = 0;
							insert_node("Running");

							pdt_t *ptr = head;
							while (ptr->link != NULL)
								ptr = ptr->link;
							printf("[%d]  %d\n", ptr->pno, ptr->pid);
						}
				}

				continue;
			}
		}

		/* Dup the stdin */
		dup2(0, 77);

		/* Declare the no.of pipes */
		int p[no_pipes][2];

		/* Create no.of pipes + 1 child process and run the commands */
		for (idx = 0; no_pipes != 0 && idx < no_pipes + 1; idx++)
		{
			/* Create pipes */
			if (idx != no_pipes && pipe(p[idx]) == -1)
			{
				perror("pipe");
				exit(-1);
			}

			/* For the child */
			switch (pid = fork())
			{
				/* Error handler */
				case -1:

					perror("fork()");
					exit(-2);

				/* Child process */
				case 0:

					/* Dup the stdin and stdout with pipe */
					if (idx == 0)
					{
						close(p[idx][0]);
						dup2(p[idx][1], 1);
					}
					else if (idx != no_pipes)
					{
						close(p[idx][0]);
						dup2(p[idx][1], 1);
					}

					if (idx == no_pipes && strcmp(argu[arguc - 1], "&") == 0)
						argu[arguc - 1] = NULL;

					execvp(argu[index[idx]], argu + index[idx]);
					exit(-1);

				default:

					c_flag = 0;

					/* Wait for the child process termination */
					if (idx != no_pipes)
					{
						while(!c_flag);
						c_flag = 0;
					}

					/* If a process not a backround process then wait for child termination */
					else if (idx == no_pipes && strcmp(argu[arguc - 1], "&") != 0)
					{
						while (!c_flag);
						c_flag = 0;
					}
					/* If backround process then insert that process details as running */
					else
					{
						pid = 0;
						insert_node("Running");
						pdt_t *ptr = head;
						while (ptr->link != NULL)
							ptr = ptr->link;
						printf("[%d]  %d\n", ptr->pno, ptr->pid);
					}

					/* Dup stdout and stdin with pipes */
					if (idx != no_pipes)
					{
						close(p[idx][1]);
						dup2(p[idx][0], 0);
					}
					else
						dup2(77, 0);
			}
		}
	}
	
	return 0;
}

