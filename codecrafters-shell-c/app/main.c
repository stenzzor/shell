#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h> 
#include <sys/stat.h>

#define ANSI_RED "\x1b[1;38;5;209m"
#define ANSI_PINK "\x1b[1;95m"
#define ANSI_END "\x1b[0m"

typedef enum {
    ARGUMENT,
    QUOTE
} state;

typedef struct {
    char **argv;
    int argc;
} args_struct;

char *builtins[] = {"echo", "exit", "type", "pwd", "cd"};
int builtinsLength = sizeof(builtins) / sizeof(builtins[0]);
args_struct args;
int args_length = 0;

int parse_command(char *str) {
  // SAFETY: return if input is space-only. Same thing done in main.
  if (str[0] == '\0') {
    return 1;
  }

  // ----------- parsing begins here ------------
  const char *p = str;
  char **argv = malloc(4 * sizeof(char *));
  int arg = 0;
  int length;
  char quote_type;
  int chunk_count = 2;
  state current_state = ARGUMENT;

  while (*p) p++;

  length = p - str;
  
  for (int i = 0; i < length; i++) {
      switch (current_state) {
          case ARGUMENT:
              if (str[i] == ' ') {
                  str[i] = '\0';
                  continue;
              }
              if (str[i] == '\'' || str[i] == '"') {
                  current_state = QUOTE;
                  (str[i] == '"') ? (quote_type = '"') : (quote_type = '\'');
                  str[i] = '\0';
                  argv[arg++] = str + i + 1;
                  continue;
              }
              if (i == 0 || str[i - 1] == '\0') {
                  char *token = str + i;
                  
                  if (token[0] == '$') {
                    char *env = getenv(token + 1);
                    if (env == NULL) {
                      argv[arg++] = token;
                    } else {
                      argv[arg++] = env;
                    }
                  } else {
                    argv[arg++] = token;
                  }
              }

              break;
          
          case QUOTE:
              
              if (str[i] == quote_type && str[i - 1] != '\\') {
                  str[i] = '\0';
                  current_state = ARGUMENT;
                  continue;
              }
              
              if (str[i + 1] == '\0') {
                return i;
              }
              
              break;
      }
      
      if (arg % 4 == 0) {
          argv = realloc(argv, (4 * (++chunk_count)) * sizeof(char *));
      }
  }
  // ----------- parsing ends here ------------
  
  if (arg % 4 == 0) argv = realloc(argv, (4 * (++chunk_count)) * sizeof(char *)); 
      
  if (strcmp(argv[0], "ls") == 0) {
    argv[arg] = "--color=auto";
    argv[arg + 1] = NULL;
  } else {
    argv[arg] = NULL;
  }

  args.argv = argv;
  args.argc = arg;

  return 0;
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
   if (strcmp(args.argv[0], "exit") == 0) {
     exit(0);
   } 
}

void builtin_echo(char *input) {
   if (strcmp(args.argv[0], "echo") == 0) {
     for (int i = 1; i < args.argc; i++) {
       (i == args.argc - 1) ? printf("%s\n", args.argv[i]) : printf("%s ", args.argv[i]);
     }
   }
}

void builtin_type(char *PATH) {
  char *file = args.argv[1];
  
  if (strcmp(args.argv[0], "type") == 0 && file) {
     if (is_builtin(file)) {
         printf("%s is a shell builtin\n", file);
         return;
       }
   
     if (!PATH) {
       perror("Failed to copy PATH");
       return;
     }

     char *path_copy = strdup(PATH);
     if (path_copy == NULL) {
        perror("Failed to allocate memory for PATH copy");
        return;
     }
     char *saveptr = path_copy;
     char *current_path = strtok(path_copy, ":");

     while (current_path) {
       DIR *folder = opendir(current_path);

       if (folder == NULL) {
         perror("");
         current_path = strtok(NULL, ":");
         continue;
       }
       if (folder) {
         struct dirent *entry;
       
         while ((entry = readdir(folder)) != NULL) {
           if (strcmp(entry->d_name, file) == 0) {
             free(saveptr);
             closedir(folder);
             printf("%s is %s/%s\n", file, current_path, entry->d_name);
             return;
           }
         }
         closedir(folder);
       }
       current_path = strtok(NULL, ":");
     }
     free(saveptr);
  }
}

void builtin_pwd() {
  if (strcmp(args.argv[0], "pwd") == 0) {
    char cwd[256];
    getcwd(cwd, sizeof(cwd));
    printf("%s\n", cwd);
  }
}

void builtin_cd(char *HOME) {
  if (strcmp(args.argv[0], "cd") == 0) {
    if (args.argv[1] == NULL || args.argv[1][0] == '~') {
      if (chdir(HOME) == -1) {
        printf("cd: ");
        perror("");
      }
      return;
    }
    
    if (chdir(args.argv[1]) == -1) {
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
  args.argv = malloc(8 * sizeof(char *));

  char input[128];
  char *inputDup = malloc(128);

  while (1) {
    printf("\x1b[38;5;208m$\x1b[0m ");
    if (fgets(input, sizeof(input), stdin) == NULL) exit(0);

    // replacing new line (enter) with null byte
    int last_char = strcspn(input, "\n");
    input[last_char] = '\0';
        
    // SAFETY: if user provides a space-only input, ignore it
    if (input[0] == '\0') continue;
    strcpy(inputDup, input);
    int i = parse_command(inputDup);
    if (i) {
      char *error = malloc(i + 25);
      if (!error) {
        printf("Unclosed quote");
        continue;
      }
      memset(error, '~', i + 25);
      error[i + 25] = '\0';
      error[i + 24] = '^';
      printf(ANSI_RED "error: " ANSI_END "\x1b[1mUnclosed quote\x1b[0m: \n   |    %s\n" ANSI_PINK "%s\n" ANSI_END, input, error);
      free(error);
    }
    
    // SAFETY: handle EOF
    if (args.argv[0] == NULL) continue;
    
    if (!is_builtin(args.argv[0])) {
      pid_t pid = fork();

      if (pid == 0) {
        // child process

        if (execvp(args.argv[0], args.argv) == -1) {
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
      builtin_type(PATH);
      builtin_pwd();
      builtin_cd(HOME);
    }    
  }

  free(inputDup);
  free(args.argv);

  return 0;
}
