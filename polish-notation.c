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

/* Declare lval struct */
typedef struct {
  int type;
  long num;
  int err;
} lval;

/* Enum of possible lval types */
enum {LVAL_NUM, LVAL_ERR};
/* Enum of possible error types */
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

/* Create number type lval */
lval lval_num(long x) {
  lval v;
  v.type = LVAL_NUM;
  v.num = x;
  return v;
}

/* Create error type lval */
lval lval_err(int x) {
  lval v;
  v.type = LVAL_ERR;
  v.err = x;
  return v;
}

/* print an lval */
void lval_print(lval v) {
  switch (v.type) {
    case LVAL_NUM: printf("%li", v.num); break;
    case LVAL_ERR:
      switch (v.err) {
        case LERR_DIV_ZERO: printf("Error: Division by Zero"); break;
        case LERR_BAD_OP: printf("Error: Invalid Operator"); break;
        case LERR_BAD_NUM: printf("Error: Invalid Number"); break;
      }
  }
}

/* print an lval and newline */
void lval_println(lval v) { lval_print(v); putchar('\n'); }

/* function performs the operations */
lval eval_op(lval x, char* op, lval y) {
  /* If there's an error, return it */
  if (x.type == LVAL_ERR) { return x; }
  if (y.type == LVAL_ERR) { return y; }

  if (strcmp(op, "+") == 0) {return lval_num(x.num + y.num);}
  if (strcmp(op, "-") == 0) {return lval_num(x.num - y.num);}
  if (strcmp(op, "*") == 0) {return lval_num(x.num * y.num);}
  if (strcmp(op, "/") == 0) {
    /* if divisor is zero return error */
    return y.num == 0
      ? lval_err(LERR_DIV_ZERO)
      : lval_num(x.num / y.num);
  }
  return lval_err(LERR_BAD_OP);
}

/* recursive eval function */
lval eval(mpc_ast_t* t) {
  /* If a number, return it */
  if (strstr(t->tag, "number")) {
    /* check for conversion error */
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE
      ? lval_num(x)
      : lval_err(LERR_BAD_NUM);
  }

  /* Second child of an expression is an operation*/
  char* op = t->children[1]->contents;

  /* Grab the third child */
  lval x = eval(t->children[2]);

  /* Iterate remaining children and perform operation */
  int i =3;
  while (strstr(t->children[i]->tag, "expr")) {
    x = eval_op(x, op, eval(t->children[i]));
    i++;
  }
  return x;
}

int main(int argc, char** argv) {

  /* Create parsers */
  mpc_parser_t* Number      = mpc_new("number");
  mpc_parser_t* Operator    = mpc_new("operator");
  mpc_parser_t* Expression  = mpc_new("expr");
  mpc_parser_t* Polish      = mpc_new("polish");

  /* Define grammar */
  mpca_lang(MPCA_LANG_DEFAULT,
    " number    : /-?[0-9]+/                            ;\
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
      /* Success: eval and print */
      lval result = eval(r.output);
      lval_println(result);
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
