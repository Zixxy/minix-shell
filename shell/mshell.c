#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include "config.h"
#include "siparse.h"
#include "utils.h"
#include "sys/stat.h"
#include "builtins.h"

typedef int bool;


#define true 1
#define false 0
//#define STDOUT_FILENO STDOUT_FILENO_fileno
//#define STDIN_FILENO STDIN_FILENO_fileno

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
	if(errno == EACCES)
			fprintf(stderr, "%s: no such file or directory\n", filename);
	else if(errno ==  ENOENT)
			fprintf(stderr, "%s: permission denied\n", filename);
	fflush(NULL);
}

void replace_STDIN_FILENO(redirection* redirect){
	if(redirect == NULL)
		return;
	int in_fd = open(redirect->filename, O_RDONLY);
	if(in_fd == -1){
		handle_openfile_error(redirect->filename);
		exit(EXEC_FAILURE);
	}
	else
		dup2(in_fd, STDIN_FILENO);
}

void replace_STDOUT_FILENO(redirection* redirect){
	if(redirect == NULL)
		return;
	int flags = O_WRONLY | O_CREAT;
	if(IS_RAPPEND(redirect->flags))
		flags |= O_APPEND;
	else
		flags |= O_TRUNC;

	int out_fd = open(redirect->filename, flags);

	if(out_fd == -1){
		handle_openfile_error(redirect->filename);
		exit(EXEC_FAILURE);
	}
	else
		dup2(out_fd, STDOUT_FILENO);
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
		replace_STDIN_FILENO(in_redirection);
		replace_STDOUT_FILENO(out_redirection);

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

bool control_pipeline(const pipeline pipe){
	for(int u = 0; pipe[u]; ++u)
			if(!controll_command(pipe[u]))
				if((u == 0 && pipe[u+1]) || u > 0){
					fprintf(stderr, "Syntax error.\n");
					return false;
				}
				else  // line is comment.
					return false;	
	return true;
}

void close_each_fd(int* t, int length){ // array ended with -1
	for(int i = 0; i < length; ++i)
		close(t[i]); // handling errors with closing possible
}

void execute_line(line* line){

	if(line == NULL 
		|| *(line -> pipelines) == NULL 
		|| (**(line->pipelines)) == NULL)
		return;
	for(int i = 0; (line -> pipelines)[i] != NULL; ++i){ // for each pipeline
		pipeline cur_pipeline = (line -> pipelines)[i];
		if(!control_pipeline(cur_pipeline))
			continue;int prev_fd[2] = {-1, -1};
		
		int countPipes = 0;
		for(int u = 0; cur_pipeline[u]; ++u){
			if(u == 0 && check_shell_command(cur_pipeline[u])){
				execute_shell_command(cur_pipeline[u]);
				break;
			}
			int fd[2];
			pipe(fd);
			int child_pid = fork();
			++countPipes;
			if(child_pid > 0){
				if(prev_fd[0] != -1)
					close(prev_fd[0]);
				if(prev_fd[1] != -1)
					close(prev_fd[1]);
			}
			else{
				if(u == 0){
					if(!cur_pipeline[u+1]){// jest sam jeden
						close(fd[1]);
						close(fd[0]);
					}
					else{
						fflush(NULL);
						close(STDOUT_FILENO);
						close(fd[0]);
						dup2(fd[1], STDOUT_FILENO); // bierzemy z zwyklego STDIN_FILENO, piszemy do pipa jako STDOUT_FILENO
					}
				}
				else if(cur_pipeline[u+1]){
					close(STDIN_FILENO);
					close(STDOUT_FILENO);
					close(prev_fd[1]);
					close(fd[0]);
					dup2(prev_fd[0], STDIN_FILENO); // wyjscie poprzedniego wejsciem mojego procesu 
					dup2(fd[1], STDOUT_FILENO); // piszemy do nastepnego pipa
				}
				else{
					fflush(NULL);
					close(fd[0]);
					close(fd[1]);
					close(STDIN_FILENO);
					close(prev_fd[1]);
					dup2(prev_fd[0], STDIN_FILENO);
				}
				execute_command(cur_pipeline[u], child_pid);
			}
			prev_fd[0] = fd[0];
			prev_fd[1] = fd[1];
		}
		fflush(NULL);
		close(prev_fd[0]);
		close(prev_fd[1]);
		
		int control_finnished_child = 0;
		while(control_finnished_child < countPipes){
			++control_finnished_child;
			int status = wait(NULL);
			if(status == -1){
				printf("shell process couldnt do waitpid().");
				fflush(NULL);
			}
		}
	}
}

void read_and_order_executing(bool terminal_mode){
	char* bufor = (char*) calloc(2 * MAX_LINE_LENGTH + 2, sizeof(char));
	char* first_part = NULL;

	int first_part_length = 0;

	while(true){
		
		if(terminal_mode){
			printf(PROMPT_STR);
			fflush(NULL);
		}

		int length = read(0, bufor+first_part_length, 
			MAX_LINE_LENGTH + 1 - first_part_length);

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



int	main(int argc, char *argv[])
{
	bool terminal_mode = true;

	struct stat status;
	fstat(0, &status);
		
	if(S_ISCHR(status.st_mode) == 0){
		terminal_mode = false;
	}
	
	read_and_order_executing(terminal_mode);
}
