#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h> 
#include <sys/stat.h>

#define ANSI_RED "\x1b[38;5;9m"
#define ANSI_END "\x1b[0m"

char *builtins[] = {"echo", "exit", "type", "pwd", "cd"};
int builtinsLength = sizeof(builtins) / sizeof(builtins[0]);
char **args = NULL;


void parse_command(char *input) {
  // SAFETY: return if input is space-only. Same thing done in main.
  if (input[0] == '\0') {
    return;
  }

  char *token = strtok(input, " ");
  if (token == NULL) return;
  int arg = 0;
  
  while (token) {
    args[arg++] = token;
    token = strtok(NULL, " ");
  }
  
  if (strcmp(args[0], "ls") == 0) {
    args[arg] = "--color=auto";
    args[arg + 1] = NULL;
  } else {
    args[arg] = NULL;
  }
}

int is_builtin(char *command) {
  for (int i = 0; i < builtinsLength; i++) {
    if (strcmp(builtins[i], command) == 0) {
      return 1;
    }
  }

  return 0;
}

void builtin_exit() {
   if (strcmp(args[0], "exit") == 0) {
     exit(0);
   } 
}

void builtin_echo(char *input) {
   if (strcmp(args[0], "echo") == 0) {
     if (args[1][0] == '$') {
       char *env = getenv(args[1] + 1);
       if (env == NULL) {
         printf("%s\n", input+5);
         return;
       };
       printf("%s\n", env);
     } else {
       printf("%s\n", input+5);
       return;
     }
   }

    if (input[5] == "'" && input[strlen(input) - 1] == "'") {
      input[strcspn(input + 6, "'")] = '\0';
      printf("%s\n", input+6);
    };
}

void builtin_type(char *file, char *PATH) {
  if (strcmp(args[0], "type") == 0 && args[1]) {
     if (is_builtin(file)) {
         printf("%s is a shell builtin\n", file);
         return;
       }
   
     if (!PATH) {
       perror("Failed to copy PATH");
       return;
     }

     char *path_copy = strdup(PATH);
     char *current_path = strtok(path_copy, ":");

     while (current_path) {
       DIR *folder = opendir(current_path);

       if (folder == NULL) {
         fprintf(stderr, "Error accessing %s", current_path);
         perror("");
         current_path = strtok(NULL, ":");
         continue;
       }
       else {
         struct dirent *entry;
       
         while ((entry = readdir(folder)) != NULL) {
           if (strcmp(entry->d_name, file) == 0) {
             free(path_copy);
             closedir(folder);
             printf("%s is %s/%s\n", file, current_path, entry->d_name);
             return;
           }
         }
       }

       closedir(folder);
       current_path = strtok(NULL, ":");
     }

     free(path_copy);
  }
}

void builtin_pwd() {
  if (strcmp(args[0], "pwd") == 0) {
    char cwd[256];
    getcwd(cwd, sizeof(cwd));
    printf("%s\n", cwd);
  }
}

void builtin_cd(char *HOME) {
  if (strcmp(args[0], "cd") == 0) {
    if (args[1] == NULL || args[1][0] == '~') {
      if (chdir(HOME) == -1) {
        printf("cd: ");
        perror("");
      }
      return;
    }
    
    if (chdir(args[1]) == -1) {
      printf("cd: ");
      perror("");
    }    
  }
}


int main(int argc, char *argv[], char *envp[]) {
  // send input directory to stdout, avoid buffering
  
  setbuf(stdout, NULL);
  char *PATH = getenv("PATH");
  char *HOME = getenv("HOME");
  setenv("TERM", "xterm-256color", 1); 
  args = malloc(8 * sizeof(char *));

  char input[128];
  char *inputDup = malloc(128);

  while (1) {
    printf("\x1b[38;5;208m$\x1b[0m ");
    if (fgets(input, sizeof(input), stdin) == NULL) exit(0);

    // replacing new line (enter) with null byte
    input[strcspn(input, "\n")] = '\0';
        
    // SAFETY: if user provides a space-only input, ignore it
    if (input[0] == '\0') continue;
    strcpy(inputDup, input);
    parse_command(inputDup);

    // SAFETY: handle EOF
    if (args[0] == NULL) continue;
    
    if (!is_builtin(args[0])) {
      pid_t pid = fork();

      if (pid == 0) {
        // child process

        if (execvp(args[0], args) == -1) {
          printf(ANSI_RED "%s" ANSI_END ": command not found\n", input);
          exit(1);
        }
      } else if (pid > 0) {
        // parent

        wait(NULL);
      }
    } else {
      builtin_exit();
      builtin_echo(input);
      builtin_type(args[1], PATH);
      builtin_pwd();
      builtin_cd(HOME);
    }    
  }

  free(inputDup);
  free(args);

  return 0;
}
