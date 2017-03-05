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

/************************* LVAL *************************/

/* Declare lval struct */
typedef struct lval {
  int type;
  long num;
  char* err;
  char* sym;

  /* for lists of "lval*" */
  int count;
  struct lval** cell;
};

/* Enum of possible lval types */
enum {LVAL_NUM, LVAL_SYM, LVAL_SEXPR, LVAL_ERR};

/* Create a pointer to a number type lval */
lval* lval_num(long x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

/* Create a pointer to an error type lval */
lval* lval_err(char* m) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  v->err = malloc(strlen(m) + 1);
  strcpy(v->err, m);
  return v;
}

/* Create a pointer to a symbol type lval */
lval* lval_sym(char* s) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);
  return v;
}

/* Create a pointer to an empty Sexpr lval */
lval* lval_sexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

/* Add an lval 'x' as a child to a sexpr 'v' */
lval* lval_add(lval* v, lval* x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count-1] = x;
  return v;
}

/* Delete an lval, and all its pointers/data */
void lval_del(laval* v) {
  switch (v-> type) {
    case LVAL_NUM: break;
    case LVAL_ERR: free(v->err); break;
    case LVAL_SYM: free(v->sym); break;
    case LVAL_SEXPR:
      /* delete all children */
      for (int i = 0; i < v->count; i++) {
        lval_del(v->cell[i]);
      }
      free(v->cell);
      break;
  }
  free(v);
}

/* Create a number lval from an AST leaf */
lval* lval_read_num(mpc_ast_t* t) {
  errno = 0;
  long x = strol(t->contents, NULL, 10);
  return errno != ERANGE
    ? lval_num(x)
    : lval_err("Invalid Number");
}

/* create lval tree from AST */
lval* lval_read(mpc_ast_t* t) {
  /* if leaf, assign correct lval type */
  if (strstr(t->tag, "number")) { return lval_read_num(t); }
  if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

  /* if root or sexpr create empty list */
  lval* x = NULL;
  if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
  if (strstr(t->tag, "sexpr")) { x = lval_sexpr(); }

  /* add valid children */
  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
    if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }
    x = lval_add(x, lval_read(t->children[i]));
  }

  return x;
}

/* print an sexpr */
void lval_expr_print(lval* v, char open, char close) {
  putchar(open);

  for (int i = 0; i < v->count; i++)
  {
    lval_print(v->cell[i]);
    if (i != (v->count-1))
    {
      putchar(' ');
    }
  }

  putchar(close);
}

/* print an lval */
void lval_print(lval* v) {
  switch (v->type) {
    case LVAL_NUM: printf("%li", v->num); break;
    case LVAL_ERR: printf("Error: %s", v->err); break;
    case LVAL_SYM: printf("%s", v->sym); break;
    case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
  }
}

/* print an lval and newline */
void lval_println(lval* v) { lval_print(v); putchar('\n'); }

/************************* EVAL FUNCS *************************/

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

/************************* MAIN *************************/

int main(int argc, char** argv) {

  /* Create parsers */
  mpc_parser_t* Number      = mpc_new("number");
  mpc_parser_t* Symbol      = mpc_new("symbol");
  mpc_parser_t* SExpression = mpc_new("sexpr");
  mpc_parser_t* Expression  = mpc_new("expr");
  mpc_parser_t* Lisb        = mpc_new("lisb");

  /* Define grammar */
  mpca_lang(MPCA_LANG_DEFAULT,
    " number    : /-?[0-9]+/                    ;\
      symbol    : '+' | '-' | '*' | '/'         ;\
      sexpr     : '(' <expr>* ')'               ;\
      expr      : <number> | <symbol> | <sexpr> ;\
      polish    : /^/ <expr>* /$/               ;\
    ",
    Number, Symbol, SExpression, Expression, Lisb
  );

  /* Print Version and Exit Info */
  puts("Lisb Version 0.0.1");
  puts("Press Ctrl+C to Exit\n");

  /* prompt/input/response loop */
  while (1) {
    char* input = readline("lisb> ");
    add_history(input);

    /* Parse and evaluate the input */
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
  mpc_cleanup(5, Number, Symbol, SExpression, Expression, Lisb);

  return 0;
} /* end main */

/**************************************************/
