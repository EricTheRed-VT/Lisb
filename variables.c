/* Quoted-Expression: an expression that is
  not evaluated by standard mechanics.
  (Similar to "'()" in Lisp; Lisb uses "{}" to
  differentiate q-expressions.)
*/

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

/************************* MACROS *************************/

#define LASSERT(args, cond, err) \
  if (!(cond)) { lval_del(args); return lval_err(err); }

/************************* LENV *************************/


/* handle cyclic types */
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

/* define lenv environments */
struct lenv {
  int count;
  char** syms;
  lval** vals;
};

/* create a new lenv */
lenv* lenv_new(void) {
  lenv* e = malloc(sizeof(lenv));
  e->count = 0;
  e->syms = NULL;
  e->vals = NULL;
  return e;
}

/* add a value to an env */
void lenv_put(lenv* e, lval* k, lval* v) {
  /* if key already exists, replace */
  for (int i = 0; i < e->count; i++) {
    if (strcmp(e->syms[i], k->sym) == 0) {
      lval_del(e->vals[i]);
      e->vals[i] = lval_copy(v);
      return;
    }
  }

  /* otherwise add variable to env */
  e->count++;
  e->vals = realloc(e->vals, sizeof(lval*) * e->count);
  e->syms = realloc(e->syms, sizeof(char*) * e->count);

  e->vals[e->count-1] = lval_copy(v);
  e->syms[e->count-1] = malloc(strlen(k->sym) + 1);
  strcpy(e->syms[e->count-1], k->sym);
}

/* copy a value from an env */
lval* lenv_get(lenv* e, lval* k) {
  /* iterate through env and grab value */
  for (int i = 0; i < e->count; i++) {
    if (strcmp(e->syms[i], k->sym) == 0) {
      return lval_copy(e->vals[i]);
    }
  }
  return lval_err("key not in environment");
}

/* delete an lenv */
void lenv_del(lenv* e) {
  for (int i = 0; i < e->count; i++) {
    free(e->syms[i]);
    lval_del(e->vals[i]);
  }
  free(e->syms);
  free(e->vals);
  free(e);
}

/************************* LVAL *************************/

/* Declare lval struct */
struct lval {
  int type;
  long num;
  char* err;
  char* sym;
  lbuiltin fun;

  /* for lists of "lval*" */
  int count;
  struct lval** cell;
} lval;

/* define the function pointer type lbuiltin */
typedef lval* (*lbuiltin)(lenv*, lval*);

/* Enum of possible lval types */
enum {LVAL_ERR, LVAL_NUM, LVAL_SYM,
      LVAL_QEXPR, LVAL_SEXPR, LVAL_FUN};

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

/* Create a pointer to an empty Qexpr lval */
lval* lval_qexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
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

/* create a pointer to a function */
lval* lval_fun(lbuiltin func) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->fun = func;
  return v;
}

/* Add an lval 'x' as a child to a sexpr 'v' */
lval* lval_add(lval* v, lval* x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count-1] = x;
  return v;
}

/* copy an lval */
lval* lval_copy(lval* v) {
  lval* x = malloc(sizeof(lval));
  x-type = v->type;

  switch (v->type) {
    /* copy functions and numbers directly */
    case LVAL_FUN: x->fun = v->fun; break;
    case LVAL_NUM: x->num = v->num; break;

    case LVAL_ERR:
      x->err = malloc(strlen(v->err) + 1);
      strcpy(x->err, v->err); break;
    case LVAL_SYM:
      x->sym = malloc(strlen(v->sym) + 1);
      strcpy(x->sym, v->sym); break;

    /* cpoy lists by copying all sub-exprs */
    case LVAL_SEXPR:
    case LVAL_QEXPR:
      x->count = v->count;
      x->cell = malloc(sizeof(lval*) * x->count);
      for (int i = 0; i < x->count; i++) {
        x->cell[i] = lval_copy(v->cell[i]);
      }
      break;
  }
  return x;
}

/* Delete an lval, and all its pointers/data */
void lval_del(lval* v) {
  switch (v-> type) {
    case LVAL_NUM: break;
    case LVAL_ERR: free(v->err); break;
    case LVAL_SYM: free(v->sym); break;
    case LVAL_FUN: break;
    case LVAL_QEXPR:
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
  long x = strtol(t->contents, NULL, 10);
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
  if (strstr(t->tag, "qexpr")) { x = lval_qexpr(); }
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

/* prep for mutual recursion */
void lval_print(lval* v);

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
    case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
    case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
    case LVAL_FUN: printf("<function>"); break;
  }
}

/* print an lval and newline */
void lval_println(lval* v) { lval_print(v); putchar('\n'); }

/************************* EVAL FUNCS *************************/

lval* lval_eval(lval* v);

/* func to pull out ith child */
lval* lval_pop(lval* v, int i) {
  /* pull out desired child */
  lval* x = v->cell[i];
  /* shift following items */
  memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count-i-1));
  v->count--;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  return x;
}

/* func to only take ith child and delete original expr */
lval* lval_take(lval* v, int i) {
  lval* x = lval_pop(v, i);
  lval_del(v);
  return x;
}

/* perform head command */
lval* builtin_head(lval* a) {
  LASSERT(a, a->count == 1,
          "'head' passed too many arguments");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "'head' passed incorrect type");
  LASSERT(a, a->cell[0]->count != 0,
          "'head' passed empty q-expression");

  lval* v = lval_take(a, 0);
  while (v->count > 1) { lval_del(lval_pop(v, 1)); }
  return v;
}

/* perform tail command */
lval* builtin_tail(lval* a) {
  LASSERT(a, a->count == 1,
          "'tail' passed too many arguments");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "'tail' passed incorrect type");
  LASSERT(a, a->cell[0]->count != 0,
          "'tail' passed empty q-expression");

  lval* v = lval_take(a, 0);
  lval_del(lval_pop(v, 0));
  return v;
}

/* perform list operation (make a q-expr from an s-expr) */
lval* builtin_list(lval* a) {
  a->type = LVAL_QEXPR;
  return a;
}

lval* builtin_eval(lval* a) {
  LASSERT(a, a->count == 1,
          "'eval' passed too many arguments");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "'eval' passed incorrect type");

  lval* x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(x);
}

/* perform join operation (combine two q-exprs) */
lval* lval_join(lval* x, lval* y) {
  /* add each element in y to x */
  while (y->count) {
    x = lval_add(x, lval_pop(y, 0));
  }
  lval_del(y);
  return x;
}

/* process a join s-expr */
lval* builtin_join(lval* a) {
  for (int i = 0; i < a->count; i++) {
    LASSERT(a, a->cell[i]->type == LVAL_QEXPR,
            "'join' passed incorrect type");
  }
  lval* x = lval_pop(a, 0);
  while (a->count) {
    x = lval_join(x, lval_pop(a, 0));
  }
  lval_del(a);
  return x;
}

/* perform an operation */
lval* builtin_op(lval* a, char* op) {

  /* all elements should be numbers */
  for (int i = 0; i < a->count; i++) {
    if (a->cell[i]->type != LVAL_NUM) {
      lval_del(a);
      return lval_err("Can only operate on numbers");
    }
  }
  /* pop first element */
  lval* x = lval_pop(a, 0);

  /* if op is "-" and only one element, perform negation */
  if((strcmp(op, "-") == 0) && a->count == 0) {
    x->num = -x->num;
  }

  /* loop through remaining elements */
  while (a->count > 0) {
    lval* y = lval_pop(a, 0);

    if (strcmp(op, "+") == 0) { x->num += y->num; }
    if (strcmp(op, "-") == 0) { x->num -= y->num; }
    if (strcmp(op, "*") == 0) { x->num *= y->num; }
    if (strcmp(op, "/") == 0) {
      if (y->num == 0) {
        lval_del(x);
        lval_del(y);
        x = lval_err("Division by zero"); break;
      }
      x->num /= y->num;
    }
    lval_del(y);
  }
  lval_del(a);
  return x;
}

/* call appropriate builtin func */
lval* builtin(lval* a, char* func) {
  if (strcmp("list", func) == 0) { return builtin_list(a); }
  if (strcmp("head", func) == 0) { return builtin_head(a); }
  if (strcmp("tail", func) == 0) { return builtin_tail(a); }
  if (strcmp("join", func) == 0) { return builtin_join(a); }
  if (strcmp("eval", func) == 0) { return builtin_eval(a); }
  if (strstr("+-/*", func)) { return builtin_op(a, func); }
  lval_del(a);
  return lval_err("unknown function");
}

/* eval an sexpr */
lval* lval_eval_sexpr(lval* v) {
  /* eval children */
  for (int i = 0; i < v->count; i++) {
    v->cell[i] = lval_eval(v->cell[i]);
  }

  /* if there are errors, return the first error found */
  for (int i = 0; i < v->count; ++i) {
    if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
  }

  /* if empty return self */
  if (v->count == 0) { return v; }

  /* single expression */
  if (v->count == 1) { return lval_take(v, 0); }

  /* first element should be a symbol */
  lval* f = lval_pop(v, 0);
  if (f->type != LVAL_SYM) {
    lval_del(f);
    lval_del(v);
    return lval_err("S-expression does not start with symbol");
  }

  /* call builtin with operator */
  lval* result = builtin(v, f->sym);
  lval_del(f);
  return result;
}

/* eval an expr */
lval* lval_eval(lval* v) {
  /* eval sexprs */
  if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(v); }
  return v;
}

/************************* MAIN *************************/

int main(int argc, char** argv) {

  /* Create parsers */
  mpc_parser_t* Number      = mpc_new("number");
  mpc_parser_t* Symbol      = mpc_new("symbol");
  mpc_parser_t* QExpression = mpc_new("qexpr");
  mpc_parser_t* SExpression = mpc_new("sexpr");
  mpc_parser_t* Expression  = mpc_new("expr");
  mpc_parser_t* Lisb        = mpc_new("lisb");

  /* Define grammar */
  mpca_lang(MPCA_LANG_DEFAULT,
    " number    : /-?[0-9]+/                              ;\
      symbol    : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/        ;\
      qexpr     : '{' <expr>* '}'                         ;\
      sexpr     : '(' <expr>* ')'                         ;\
      expr      : <number> | <symbol> | <qexpr> | <sexpr> ;\
      lisb      : /^/ <expr>* /$/                         ;\
    ",
    Number, Symbol, QExpression, SExpression, Expression, Lisb
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
    if (mpc_parse("<stdin>", input, Lisb, &r)) {
      /* Success: print */
      lval* x = lval_eval(lval_read(r.output));
      lval_println(x);
      lval_del(x);
      mpc_ast_delete(r.output);
    } else {
      /* Failure: Print Error */
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    free(input);
  }

  /* Undefine and Delete parsers */
  mpc_cleanup(6,
              Number, Symbol, QExpression,
              SExpression, Expression, Lisb);

  return 0;
} /* end main */

/**************************************************/
