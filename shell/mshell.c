#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>

#include "config.h"
#include "siparse.h"
#include "utils.h"
#include "sys/stat.h"
#include "builtins.h"

typedef int bool;


#define true 1
#define false 0

#define CONTROLLED_CHILDREN 1000
//#define STDOUT_FILENO STDOUT_FILENO_fileno
//#define STDIN_FILENO STDIN_FILENO_fileno


/*bacground children*/
pid_t finished_background_pids[CONTROLLED_CHILDREN];
int finished_background_status[CONTROLLED_CHILDREN];
/*bacground children*/
bool is_foreground_child();

pid_t foreground_children[CONTROLLED_CHILDREN];

struct sigaction foreground_sigaction;
struct sigaction background_sigaction;

volatile int background_number;
volatile int foreground_number;

void sigchild_handler(int sig_nb){
	int chld_status;
	while(foreground_number){
		pid_t pid = waitpid(-1, &chld_status, WNOHANG);
		if(pid == 0)
			return;
		else if(is_foreground_child(pid))
			--foreground_number;
		else{
			finished_background_status[background_number] = chld_status;
			finished_background_pids[background_number++] = pid;
		}
	}
}

bool is_foreground_child(pid_t child_pid){
	for(int i = 0; i < CONTROLLED_CHILDREN; ++i) // if i < foreground_number
		if(foreground_children[i] == child_pid){
			//printf("%d\n",child_pid );
			return true;
		}
	return false;
}

void block_sigchld(){
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &mask, NULL);
}

void unblock_sigchld(){
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);
	sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

void print_finished_background_number(){
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &mask, NULL);
	//printf("background_number = %d \n", background_number);
	for(int i = 0; i < background_number; ++i){
		if(WIFSIGNALED(finished_background_status[i]))
			printf("Background process %d terminated. (killed by signal %d)\n", 
				finished_background_pids[i]
				,WTERMSIG(finished_background_status[i]));
		else
			printf("Background process %d terminated. (exited with status %d)\n", 
				finished_background_pids[i], 
				finished_background_status[i]);
	}
	fflush(NULL);
	background_number = 0;
	sigprocmask(SIG_UNBLOCK, &mask, NULL);
}


void set_sigchld_handler(){
	foreground_sigaction.sa_handler = sigchild_handler;
	foreground_sigaction.sa_flags = 0;

	sigemptyset(&foreground_sigaction.sa_mask);
	sigaction(SIGCHLD, &foreground_sigaction, NULL);
}

void ignore_sigign(){
	foreground_sigaction.sa_handler = SIG_IGN;
	foreground_sigaction.sa_flags = 0;

	sigemptyset(&foreground_sigaction.sa_mask);
	sigaction(SIGINT, &foreground_sigaction, NULL);
}

void set_default_handlers(){
	background_sigaction.sa_handler = SIG_DFL;
	background_sigaction.sa_flags = 0;

	sigemptyset(&background_sigaction.sa_mask);
	sigaction(SIGCHLD, &background_sigaction, NULL); 

	background_sigaction.sa_handler = SIG_DFL;
	sigaction(SIGINT, &background_sigaction, NULL); 
}

void set_this_background(){
	set_default_handlers();
	int res = setsid();
	if(res == -1) //not handled
		exit(1);
}

redirection* find_in_redirection(redirection* redirections[]){
	struct redirection* result_redirection = NULL;
	for(int i = 0; redirections[i] != NULL; ++i)
		if (IS_RIN(redirections[i]->flags))
			result_redirection = redirections[i];
	return result_redirection;
}

redirection* find_out_redirection(redirection* redirections[]){
	struct redirection* result_redirection = NULL;
	for(int i = 0; redirections[i] != NULL; ++i)
		if (IS_ROUT(redirections[i]->flags) || IS_RAPPEND(redirections[i]->flags)) 
			result_redirection = redirections[i];
	return result_redirection;
}

void handle_openfile_error(const char* filename){
	if(errno == ENOENT)
			fprintf(stderr, "%s: no such file or directory\n", filename);
	else if(errno ==  EACCES)
			fprintf(stderr, "%s: permission denied\n", filename);
	fflush(NULL);
}

void replace_stdin(redirection* redirect){
	if(redirect == NULL)
		return;
	int in_fd = open(redirect->filename, O_RDONLY, S_IRUSR | S_IWUSR | S_IRWXO);
	if(in_fd == -1){
		handle_openfile_error(redirect->filename);
		exit(EXEC_FAILURE);
	}
	else{
		dup2(in_fd, STDIN_FILENO);
		close(in_fd);
	}
}

void replace_stdout(redirection* redirect){
	if(redirect == NULL)
		return;
	int flags = O_WRONLY | O_CREAT;
	if(IS_RAPPEND(redirect->flags))
		flags |= O_APPEND;
	else
		flags |= O_TRUNC;

	int out_fd = open(redirect->filename, flags, S_IRUSR | S_IWUSR | S_IRWXO);

	if(out_fd == -1){
		handle_openfile_error(redirect->filename);
		exit(EXEC_FAILURE);
	}
	else{
		dup2(out_fd, STDOUT_FILENO);
		close(out_fd);
	}
}

bool controll_command(const command* command){
	if(command -> argv[0] == NULL){
		return false; // wrong
	}
	return true; // right
}

bool check_shell_command(const command* command){
	if(command == NULL || *(command->argv) == NULL)
		return false;

	for(int u = 0; builtins_table[u].name != NULL; ++u){
		if(strcmp(builtins_table[u].name, *(command -> argv)) == 0){
			return true;
		}
	}
	return false;
}

void execute_shell_command(const command* command){
	for(int u = 0; builtins_table[u].name != NULL; ++u){
		if(strcmp(builtins_table[u].name, *(command -> argv)) == 0){
			builtins_table[u].fun(command -> argv);
			return;
		}
	}
}

void execute_command(const command* command, int child_pid){
	struct redirection **redirects = command -> redirs;

	if(child_pid == -1)
		exit(EXEC_FAILURE);
	else if(child_pid == 0){
		struct redirection* in_redirection = find_in_redirection(redirects);
		struct redirection* out_redirection = find_out_redirection(redirects);

		fflush(NULL);
		replace_stdin(in_redirection);
		replace_stdout(out_redirection);

		int res = execvp(command -> argv[0], command -> argv);
		if(errno == ENOENT)
			fprintf(stderr, "%s: no such file or directory\n",
				command -> argv[0]);
		else if(errno == EACCES)
			fprintf(stderr, "%s: permission denied\n",
				command -> argv[0]);
		else
			fprintf(stderr, "%s: exec error\n",
				command -> argv[0]);
		fflush(NULL);
		exit(EXEC_FAILURE);
	}
	else{
		return;
	}
}


command* prepare_command(int length, char* bufor){
	char* currentline;
	if(length == 0)
			exit(1);
	
	if(length == MAX_LINE_LENGTH+1){
		fprintf(stderr, "Syntax error.\n");
		char cur = {0};
		while(cur != '\n'){
			char a[1] = {0};
			read(0, a, 1);
			cur = a[0];
		}
		return NULL;
	}
	
	line * line = parseline(bufor);
	command* command = pickfirstcommand(line);
	if(command == NULL 
		|| command -> argv == NULL
		|| command -> argv[0] == NULL)
			return NULL;
	return command;
}

line* prepare_line(int length, char* bufor){
	char* currentline;
	if(length == 0)
			exit(1);
	
	if(length == MAX_LINE_LENGTH+1){
		fprintf(stderr, "Syntax error.\n");
		char cur = {0};
		while(cur != '\n'){
			char a[1] = {0};
			read(0, a, 1);
			cur = a[0];
		}
		return NULL;
	}

	return parseline(bufor);
}

void execute_line(line* line){
//	printparsedline(line);
	if(line == NULL 
		|| *(line -> pipelines) == NULL 
		|| (**(line->pipelines)) == NULL)
		return;
	for(int i = 0; (line -> pipelines)[i] != NULL; ++i){ // for each pipeline
		pipeline cur_pipeline = (line -> pipelines)[i];
		bool syntax_controll = true;
		for(int u = 0; cur_pipeline[u]; ++u)
			if(!controll_command(cur_pipeline[u]))
				if((u == 0 && cur_pipeline[u+1]) || u > 0){
					fprintf(stderr, "Syntax error.\n");
					syntax_controll = false;
					break;
				}
				else { // line is command.
					return;
				}
		if(!syntax_controll)
			continue;
		int prev_fd[2] = {-1, -1};
		block_sigchld();
		for(int u = 0; cur_pipeline[u]; ++u){
			if(u == 0 && check_shell_command(cur_pipeline[u])){
				execute_shell_command(cur_pipeline[u]);
				break;
			}
			int fd[2];
			pipe(fd);
			int child_pid = fork();
			if(child_pid > 0){
				if(prev_fd[0] != -1)
					close(prev_fd[0]);
				if(prev_fd[1] != -1)
					close(prev_fd[1]);
				if(!line -> flags & LINBACKGROUND){
					foreground_children[foreground_number++] = child_pid;
				//	printf("foreground chld %d\n",child_pid );
				}
			}
			else{
				if(line -> flags & LINBACKGROUND){
					set_this_background();
				}

				set_default_handlers();
				if(u == 0){
					if(!cur_pipeline[u+1]){
						close(fd[1]);
						close(fd[0]);
					}
					else{
						close(STDOUT_FILENO);//to delete
						close(fd[0]);
						dup2(fd[1], STDOUT_FILENO);
						close(fd[1]);
					}
				}
				else if(cur_pipeline[u+1]){
					close(STDIN_FILENO);//to delete
					close(STDOUT_FILENO);//to delete
					close(prev_fd[1]);
					close(fd[0]);
					dup2(prev_fd[0], STDIN_FILENO);
					dup2(fd[1], STDOUT_FILENO);
					close(prev_fd[0]);
					close(fd[1]);
				}
				else{
					fflush(NULL);
					close(fd[0]);
					close(fd[1]);
					close(STDIN_FILENO);//to delete
					close(prev_fd[1]);
					dup2(prev_fd[0], STDIN_FILENO);
					close(prev_fd[0]);
				}
				execute_command(cur_pipeline[u], child_pid);
			}
			prev_fd[0] = fd[0];
			prev_fd[1] = fd[1];
		}
		fflush(NULL);
		close(prev_fd[0]);
		close(prev_fd[1]);
		while(foreground_number){
			sigset_t mask;
			sigfillset(&mask);
			sigdelset(&mask, SIGCHLD);
			sigsuspend(&mask);
		}
		unblock_sigchld();
	}
}

/*
void delimitate_background_process(const line* line){
	if(line -> flags && LINBACKGROUND){
		int child_pid = fork();
		if(child_pid > 0) //
			return;

	}
}*/

void buffor_copy(char* tab1, char* tab2, int len1){
	if(tab1 == NULL || len1 == 0)
		return;
	for(int i = 0; i < len1; ++i)
		tab2[i] = tab1[i];
}

void clear_bufor(char* bufor, int length){
	for(int i = 0; i < length; ++i)
		bufor[i] = 0;
}

void read_and_order_executing(bool terminal_mode){
	char* bufor = (char*) calloc(2 * MAX_LINE_LENGTH + 2, sizeof(char));//buffer xD
	char* first_part = NULL;

	int first_part_length = 0;

	while(true){

		if(terminal_mode){
			print_finished_background_number();
			printf(PROMPT_STR);
			fflush(NULL);
		}

		for(int i = 0; i < CONTROLLED_CHILDREN; ++i)
			foreground_children[i] = -1; //clear. Upiekszyc troszke...

		int length = -1;
		do{
			length = read(0, bufor+first_part_length, 
			MAX_LINE_LENGTH + 1 - first_part_length);
			if(length == -1){
				if(errno == EINTR)
					continue;
				else
					exit(0);
			}
		}while(length == -1);

		if(length == 0){
			realloc(bufor, ((2 * MAX_LINE_LENGTH+2)*sizeof(char)));
			exit(0);
		}

		int prev = 0;
		length += first_part_length;
		for(int i = 0; i < length; ++i){	
			if(bufor[i] == '\n' || bufor[i] == 0){
				bufor[i] = 0;
				fflush(NULL);
				if(i == prev){
					++prev;
					continue;
				}
				line* line = prepare_line(i - prev, bufor + prev);
				execute_line(line);	
				prev = i+1;
			}
		}

		if(prev == 0 && length == MAX_LINE_LENGTH+1){
			fprintf(stderr, "Syntax error.\n");
			char cur = {0};
			while(cur != '\n'){
				char a[1] = {0};
				read(0, a, 1);
				cur = a[0];
			}
			first_part_length = 0;
			clear_bufor(bufor, 2*MAX_LINE_LENGTH+2);
			continue;
		}

		if(prev != MAX_LINE_LENGTH+1){
			
			char* new_bufor = (char*) calloc(2 * MAX_LINE_LENGTH+2, sizeof(char));
			first_part_length = length - prev;
			
			buffor_copy(bufor+prev, new_bufor, first_part_length);
			realloc(bufor, ((2 * MAX_LINE_LENGTH+2)*sizeof(char)));
			bufor = new_bufor;
			clear_bufor(bufor+first_part_length+1, 
			2 * MAX_LINE_LENGTH + 2 - first_part_length);
		}
		else{
			first_part_length = 0;
			clear_bufor(bufor, 2 * MAX_LINE_LENGTH+2);
		}

	}
}

void initial_settings(){
	set_sigchld_handler();
	ignore_sigign();

	background_number = 0;

	foreground_number = 0;
}

int	main(int argc, char *argv[])
{
	initial_settings();

	bool terminal_mode = true;

	struct stat status;
	fstat(0, &status);
		
	if(S_ISCHR(status.st_mode) == 0)
		terminal_mode = false;
	read_and_order_executing(terminal_mode);
}