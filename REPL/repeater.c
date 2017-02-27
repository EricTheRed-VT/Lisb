#include <stdio.h>

/* Declare a buffer for user input */
static char input[2048];

int main(int argc, char** argv)
{
  /* Print Version and Exit Info */
  puts("Lisb Version 0.0.1");
  puts("Press Ctrl+C to Exit\n");

  /* Infinite loop */
  while (1) {
    /* Output prompt */
    fputs("lisb> ", stdout);

    /* Read a line of input */
    fgets(input, 2048, stdin);

    /* Echo input back */
    printf("You said: %s", input);
  }

  return 0;
}
