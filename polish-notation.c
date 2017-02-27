#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

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

  /* Create parsers */
  mpc_parser_t* Number      = mpc_new("number");
  mpc_parser_t* Operator    = mpc_new("operator");
  mpc_parser_t* Expression  = mpc_new("expr");
  mpc_parser_t* Polish      = mpc_new("polish");

  /* Define grammar */
  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                    \
      number    : /-?[0-9]+/                            ;\
      operator  : '+' | '-' | '*' | '/'                 ;\
      expr      : <number> | '(' <operator> <expr>+ ')' ;\
      polish    : /^/ <operator> <expr>+ /$/            ;\
    ",
    Number, Operator, Expression, Polish
  );


  /* Print Version and Exit Info */
  puts("Lisb Version 0.0.1");
  puts("Press Ctrl+C to Exit\n");

  /* prompt/input/response loop */
  while (1) {
    char* input = readline("lisb> ");
    add_history(input);

    /* Parse the input */
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Polish, &r)) {
      /* Success: print AST */
      mpc_ast_print(r.output);
      mpc_ast_delete(r.output);
    } else {
      /* Failure: Print Error */
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    free(input);
  }

  /* Undefine and Delete parsers */
  mpc_cleanup(4, Number, Operator, Expression, Polish);

  return 0;
} /* end main */
