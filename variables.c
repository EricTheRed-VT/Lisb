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

#define LASSERT(args, cond, fmt, ...)         \
  if (!(cond)) {                              \
    lval* err = lval_err(fmt, ##__VA_ARGS__); \
    lval_del(args);                           \
    return err;                               \
  }

/************************* LVAL *************************/

/* handle cyclic types */
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

/* define the function pointer type lbuiltin */
typedef lval* (*lbuiltin)(lenv*, lval*);

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
};

/* Enum of possible lval types */
enum {LVAL_ERR, LVAL_NUM, LVAL_SYM,
      LVAL_QEXPR, LVAL_SEXPR, LVAL_FUN};

/* Create a pointer to an error type lval */
lval* lval_err(char* fmt, ...) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;

  /* create a va list */
  va_list va;
  va_start(va, fmt);

  /* printf the error string (511 char max) */
  v->err = malloc(512);
  vsnprintf(v->err, 511, fmt, va);
  v->err = realloc(v->err, strlen(v->err)+1);

  va_end(va);
  return v;
}

/* Create a pointer to a number type lval */
lval* lval_num(long x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
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
  x->type = v->type;

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
    : lval_err("Invalid Number '%s'", t->contents);
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

/************************* LENV *************************/

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
  return lval_err("key '%s' not in environment", k->sym);
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

/************************* EVAL FUNCS *************************/

lval* lval_eval(lenv* e, lval* v);

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
lval* builtin_head(lenv* e, lval* a) {
  LASSERT(a, a->count == 1,
          "'head' passed too many arguments. "
          "Expected %i, got %i",
          1, a->count);
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "'head' passed incorrect type");
  LASSERT(a, a->cell[0]->count != 0,
          "'head' passed empty q-expression");

  lval* v = lval_take(a, 0);
  while (v->count > 1) { lval_del(lval_pop(v, 1)); }
  return v;
}

/* perform tail command */
lval* builtin_tail(lenv* e, lval* a) {
  LASSERT(a, a->count == 1,
          "'tail' passed too many arguments. "
          "Expected %i, got %i",
          1, a->count);
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "'tail' passed incorrect type");
  LASSERT(a, a->cell[0]->count != 0,
          "'tail' passed empty q-expression");

  lval* v = lval_take(a, 0);
  lval_del(lval_pop(v, 0));
  return v;
}

/* perform list operation (make a q-expr from an s-expr) */
lval* builtin_list(lenv* e, lval* a) {
  a->type = LVAL_QEXPR;
  return a;
}

lval* builtin_eval(lenv* e, lval* a) {
  LASSERT(a, a->count == 1,
          "'eval' passed too many arguments. "
          "Expected %i, got %i",
          1, a->count);
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "'eval' passed incorrect type");

  lval* x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(e, x);
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
lval* builtin_join(lenv* e, lval* a) {
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
lval* builtin_op(lenv* e, lval* a, char* op) {

  /* all elements should be numbers */
  for (int i = 0; i < a->count; i++) {
    if (a->cell[i]->type != LVAL_NUM) {
      lval_del(a);
      return lval_err("Can operate on '%s', not a number", a->cell[i]->type);
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

/* built in math funcs */
lval* builtin_add(lenv* e, lval* a) {
  return builtin_op(e, a, "+");
}

lval* builtin_sub(lenv* e, lval* a) {
  return builtin_op(e, a, "-");
}

lval* builtin_mul(lenv* e, lval* a) {
  return builtin_op(e, a, "*");
}

lval* builtin_div(lenv* e, lval* a) {
  return builtin_op(e, a, "/");
}

/* func to define a new env variable */
lval* builtin_def(lenv* e, lval* a) {
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
           "'def' passed incorrect type");

  /* first arg is a list of symbols */
  lval* syms = a->cell[0];
  for (int i = 0; i < syms->count; i++) {
    LASSERT(a, syms->cell[i]->type == LVAL_SYM,
            "'def' can only define symbols");
  }
  LASSERT(a, syms->count == a->count-1,
          "'def' requires same number of values and symbols. "
          "Got %i symbols, and %i values",
          syms->count, a->count-1);
  /* assign copies of vals to symbols */
  for (int i = 0; i < syms->count; i++) {
    lenv_put(e, syms->cell[i], a->cell[i+1]);
  }
  lval_del(a);
  /* return empty expression */
  return lval_sexpr();
}

/* add a func to an env */
void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
  lval* k = lval_sym(name);
  lval* v = lval_fun(func);
  lenv_put(e, k, v);
  lval_del(k);
  lval_del(v);
}

/* add our builtin funcs to an env */
void lenv_add_builtins(lenv* e) {
  lenv_add_builtin(e, "list", builtin_list);
  lenv_add_builtin(e, "head", builtin_head);
  lenv_add_builtin(e, "tail", builtin_tail);
  lenv_add_builtin(e, "eval", builtin_eval);
  lenv_add_builtin(e, "join", builtin_join);

  lenv_add_builtin(e, "+", builtin_add);
  lenv_add_builtin(e, "-", builtin_sub);
  lenv_add_builtin(e, "*", builtin_mul);
  lenv_add_builtin(e, "/", builtin_div);

  lenv_add_builtin(e, "def", builtin_def);
}

/* eval an sexpr */
lval* lval_eval_sexpr(lenv* e, lval* v) {
  /* eval children */
  for (int i = 0; i < v->count; i++) {
    v->cell[i] = lval_eval(e, v->cell[i]);
  }

  /* if there are errors, return the first error found */
  for (int i = 0; i < v->count; i++) {
    if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
  }

  /* if empty return self */
  if (v->count == 0) { return v; }

  /* single expression */
  if (v->count == 1) { return lval_take(v, 0); }

  /* first element should be a function */
  lval* f = lval_pop(v, 0);
  if (f->type != LVAL_FUN) {
    lval_del(f);
    lval_del(v);
    return lval_err("S-expression does not start with function");
  }

  /* call builtin with operator */
  lval* result = f->fun(e, v);
  lval_del(f);
  return result;
}

/* eval an expr */
lval* lval_eval(lenv* e, lval* v) {
  /* eval sexprs */
  if (v->type == LVAL_SYM) {
    lval* x = lenv_get(e, v);
    lval_del(v);
    return x;
  }
  if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }
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

  /* Setup starting env */
  lenv* e = lenv_new();
  lenv_add_builtins(e);

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
      lval* x = lval_eval(e, lval_read(r.output));
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

  lenv_del(e);

  /* Undefine and Delete parsers */
  mpc_cleanup(6,
              Number, Symbol, QExpression,
              SExpression, Expression, Lisb);

  return 0;
} /* end main */

/**************************************************/
