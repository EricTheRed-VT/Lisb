#include <stdio.h>
#include <stdlib.h>

/* If compiling on Windows, use these */
#ifdef _WIN32
#include <string.h>

static char buffer[2048];

/* fake readline func */
char* readline(char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char* cpy = malloc (strlen(buffer)+1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy)-1] = '\0';
  return cpy;
}

/* Fake add-history func */
void add_history(char* unused) {}

/* If not on Windows, include editline headers */
#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

int main(int argc, char** argv) {

  /* Print Version and Exit Info */
  puts("Lisb Version 0.0.1");
  puts("Press Ctrl+C to Exit\n");

  /* Infinite loop */
  while (1) {

    /* Output prompt and get input */
    char* input = readline("lisb> ");

    /* add input to history */
    add_history(input);

    /* Echo input back */
    printf("You said: %s\n", input);

    /* Free retrieved input */
    free(input);

  }

  return 0;
}
