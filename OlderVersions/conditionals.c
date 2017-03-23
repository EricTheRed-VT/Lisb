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
  /* basic */
  int type;
  long num;
  char* err;
  char* sym;

  /* function */
  lbuiltin builtin;
  lenv* env;
  lval* formals;
  lval* body;

  /* expression */
  int count;
  lval** cell;
};

/* Enum of possible lval types */
enum {LVAL_ERR, LVAL_NUM, LVAL_SYM,
      LVAL_QEXPR, LVAL_SEXPR, LVAL_FUN};

/* retrieve type name from enum */
char* ltype_name(int t) {
  switch (t) {
    case LVAL_FUN: return "Function";
    case LVAL_NUM: return "Number";
    case LVAL_ERR: return "Error";
    case LVAL_SYM: return "Symbol";
    case LVAL_SEXPR: return "S-Expression";
    case LVAL_QEXPR: return "Q-Expression";
    default: return "Unknown";
  }
}

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
lval* lval_builtin(lbuiltin func) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->builtin = func;
  return v;
}

lenv* lenv_new(void);

/* create a pointer to a user-defined func*/
lval* lval_lambda(lval* formals, lval* body) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->builtin = NULL;
  v->env = lenv_new();
  v->formals = formals;
  v->body = body;
  return v;
}

/* Add an lval 'x' as a child to a sexpr 'v' */
lval* lval_add(lval* v, lval* x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count-1] = x;
  return v;
}

lenv* lenv_copy(lenv* e);

/* copy an lval */
lval* lval_copy(lval* v) {
  lval* x = malloc(sizeof(lval));
  x->type = v->type;

  switch (v->type) {
    /* copy functions and numbers directly */
    case LVAL_FUN:
      if (v->builtin) {
        x->builtin = v->builtin;
      } else {
        x->builtin = NULL;
        x->env = lenv_copy(v->env);
        x->formals = lval_copy(v->formals);
        x->body = lval_copy(v->body);
      }
      break;

    case LVAL_NUM:
      x->num = v->num;
      break;

    case LVAL_ERR:
      x->err = malloc(strlen(v->err) + 1);
      strcpy(x->err, v->err);
      break;

    case LVAL_SYM:
      x->sym = malloc(strlen(v->sym) + 1);
      strcpy(x->sym, v->sym);
      break;

    /* copy lists by copying all sub-exprs */
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

/* combine two q-exprs */
lval* lval_join(lval* x, lval* y) {
  /* add each element in y to x */
  for (int i = 0; i < y->count; i++) {
    x = lval_add(x, y->cell[i]);
  }
  free(y->cell);
  free(y);
  return x;
}

/* func to pull out ith child */
lval* lval_pop(lval* v, int i) {
  /* pull out desired child */
  lval* x = v->cell[i];
  /* shift following items */
  memmove(&v->cell[i], &v->cell[i+1],
          sizeof(lval*) * (v->count-i-1));
  v->count--;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  return x;
}

void lenv_del(lenv* e);

/* Delete an lval, and all its pointers/data */
void lval_del(lval* v) {
  switch (v-> type) {
    case LVAL_NUM: break;
    case LVAL_ERR: free(v->err); break;
    case LVAL_SYM: free(v->sym); break;
    case LVAL_FUN:
      if (!v->builtin) {
        lenv_del(v->env);
        lval_del(v->formals);
        lval_del(v->body);
      }
      break;
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

/* func to only take ith child and delete original expr */
lval* lval_take(lval* v, int i) {
  lval* x = lval_pop(v, i);
  lval_del(v);
  return x;
}

/* prep for mutual recursion */
void lval_print(lval* v);

/* print an sexpr */
void lval_expr_print(lval* v, char open, char close) {
  putchar(open);

  for (int i = 0; i < v->count; i++) {
    lval_print(v->cell[i]);
    if (i != (v->count-1)) {
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
    case LVAL_FUN:
      if (v->builtin) { printf("<builtin>"); }
      else {
        printf("(lambda ");
        lval_print(v->formals);
        putchar(' ');
        lval_print(v->body);
        putchar(')');
      }
      break;
  }
}

/* print an lval and newline */
void lval_println(lval* v) { lval_print(v); putchar('\n'); }

/************************* LENV *************************/

/* define lenv environments */
struct lenv {
  lenv* parent;
  int count;
  char** syms;
  lval** vals;
};

/* create a new lenv */
lenv* lenv_new(void) {
  lenv* e = malloc(sizeof(lenv));
  e->parent = NULL;
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

/* add a global value */
void lenv_put_global(lenv* e, lval* k, lval* v) {
  /* follow parent chain up */
  while (e->parent) { e = e->parent; }
  lenv_put(e, k, v);
}

/* copy a value from an env */
lval* lenv_get(lenv* e, lval* k) {
  /* iterate through env and grab value */
  for (int i = 0; i < e->count; i++) {
    if (strcmp(e->syms[i], k->sym) == 0) {
      return lval_copy(e->vals[i]);
    }
  }

  /* if not found, check parent */
  if (e->parent) {
    return lenv_get(e->parent, k);
  } else {
    return lval_err("key '%s' not in environment", k->sym);
  }
}

/* copy an lenv */
lenv* lenv_copy(lenv* e) {
  lenv* n = malloc(sizeof(lenv));
  n->parent = e->parent;
  n->count = e->count;
  n->syms = malloc(sizeof(char*) * n->count);
  n->vals = malloc(sizeof(lval*) * n->count);

  for (int i = 0; i < e->count; i++) {
    n->syms[i] = malloc(strlen(e->syms[i]) + 1);
    strcpy(n->syms[i], e->syms[i]);
    n->vals[i] = lval_copy(e->vals[i]);
  }

  return n;
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

/************************* MACROS *************************/

#define LASSERT(args, cond, fmt, ...)         \
  if (!(cond)) {                              \
    lval* err = lval_err(fmt, ##__VA_ARGS__); \
    lval_del(args);                           \
    return err;                               \
  }

#define LASSERT_NUM_ARGS(func, args, num)                    \
  LASSERT(args, args->count == num,                     \
          "'%s' passed incorrect number of arguments. " \
          "Expected %i, got %i.",                       \
          func, num, args->count)

#define LASSERT_ARG_TYPE(func, args, index, expect)           \
  LASSERT(args, args->cell[index]->type == expect,        \
          "'%s' passed incorrect type for argument %i. "  \
          "Expected %s, got %s.",                         \
          func, index, ltype_name(expect),                \
          ltype_name(args->cell[index]->type))

#define LASSERT_NOT_EMPTY(func, args, index)    \
  LASSERT(args, args->cell[index]->count != 0,  \
          "'%s' passed {} for argument %i.",    \
          func, index);

/************************* BUILTIN FUNCS *************************/

lval* lval_eval(lenv* e, lval* v);

/* perform head command */
lval* builtin_head(lenv* e, lval* a) {
  LASSERT_NUM_ARGS("head", a, 1);
  LASSERT_ARG_TYPE("head", a, 0, LVAL_QEXPR);
  LASSERT_NOT_EMPTY("head", a, 0);

  lval* v = lval_take(a, 0);
  while (v->count > 1) { lval_del(lval_pop(v, 1)); }
  return v;
}

/* perform tail command */
lval* builtin_tail(lenv* e, lval* a) {
  LASSERT_NUM_ARGS("tail", a, 1);
  LASSERT_ARG_TYPE("tail", a, 0, LVAL_QEXPR);
  LASSERT_NOT_EMPTY("tail", a, 0);

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
  LASSERT_NUM_ARGS("eval", a, 1);
  LASSERT_ARG_TYPE("eval", a, 0, LVAL_QEXPR);

  lval* x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(e, x);
}

/* process a join s-expr */
lval* builtin_join(lenv* e, lval* a) {
  for (int i = 0; i < a->count; i++) {
    LASSERT_ARG_TYPE("join", a, i, LVAL_QEXPR);
  }
  lval* x = lval_pop(a, 0);
  while (a->count) {
    lval* y = lval_pop(a, 0);
    x = lval_join(x, y);
  }
  lval_del(a);
  return x;
}

/* perform an operation */
lval* builtin_op(lenv* e, lval* a, char* op) {

  /* all elements should be numbers */
  for (int i = 0; i < a->count; i++) {
    LASSERT_ARG_TYPE(op, a, i, LVAL_NUM);
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

/* handle equivalence checks */
int lval_eq(lval* x, lval* y) {
  if (x->type != y->type) {return 0;}

  switch (x->type) {
    case LVAL_NUM: return (x->num == y->num);
    case LVAL_ERR: return (strcmp(x->err, y->err) == 0);
    case LVAL_SYM: return (strcmp(x->sym, y->sym) == 0);

    case LVAL_FUN:
      if (x->builtin || y->builtin) {
        return (x->builtin == y->builtin);
      } else {
        return (lval_eq(x->formals, y->formals)
                && lval_eq(x->body, y->body));
      }

    case LVAL_QEXPR:
    case LVAL_SEXPR:
      if (x->count != y->count) {return 0;}
      for (int i = 0; i < x->count; i++) {
        if (!lval_eq(x->cell[i], y->cell[i])) {return 0;}
      }
      return 1;
    break;
  }
  return 0;
}

lval* builtin_cmp(lenv* e, lval* a, char* op) {
  LASSERT_NUM_ARGS(op, a, 2);
  int r;
  if (strcmp(op, "==") == 0) {
    r = lval_eq(a->cell[0], a->cell[1]);
  }
  if (strcmp(op, "!=") == 0) {
    r = !lval_eq(a->cell[0], a->cell[1]);
  }
  lval_del(a);
  return lval_num(r);
}

lval* builtin_eq(lenv* e, lval* a) {
  return builtin_cmp(e, a, "==");
}

lval* builtin_ne(lenv* e, lval* a) {
  return builtin_cmp(e, a, "!=");
}

/* handle other comparison ops, 0 = false, 1 = true */
lval* builtin_ord(lenv* e, lval* a, char* op) {
  LASSERT_NUM_ARGS(op, a, 2);
  LASSERT_ARG_TYPE(op, a, 0, LVAL_NUM);
  LASSERT_ARG_TYPE(op, a, 1, LVAL_NUM);

  int r;
  if (strcmp(op, ">") == 0) {
    r = (a->cell[0]->num > a->cell[1]->num);
  }
  if (strcmp(op, "<") == 0) {
    r = (a->cell[0]->num < a->cell[1]->num);
  }
  if (strcmp(op, ">=") == 0) {
    r = (a->cell[0]->num >= a->cell[1]->num);
  }
  if (strcmp(op, "<=") == 0) {
    r = (a->cell[0]->num <= a->cell[1]->num);
  }

  lval_del(a);
  return lval_num(r);
}

lval* builtin_greater(lenv* e, lval* a) {
  return builtin_ord(e, a, ">");
}

lval* builtin_less(lenv* e, lval* a) {
  return builtin_ord(e, a, "<");
}

lval* builtin_weak_greater(lenv* e, lval* a) {
  return builtin_ord(e, a, ">=");
}

lval* builtin_weak_less(lenv* e, lval* a) {
  return builtin_ord(e, a, "<=");
}

lval* builtin_if(lenv* e, lval* a) {
  LASSERT_NUM_ARGS("if", a, 3);
  LASSERT_ARG_TYPE("if", a, 0, LVAL_NUM);
  LASSERT_ARG_TYPE("if", a, 1, LVAL_QEXPR);
  LASSERT_ARG_TYPE("if", a, 2, LVAL_QEXPR);

  lval* x;

  if (a->cell[0]->num) {
    a->cell[1]->type = LVAL_SEXPR;
    x = lval_eval(e, lval_pop(a, 1));
  } else {
    a->cell[2]->type = LVAL_SEXPR;
    x = lval_eval(e, lval_pop(a, 2));
  }

  lval_del(a);
  return x;
}

/* func to define a new lambda func */
lval* builtin_lambda(lenv* e, lval* a) {
  LASSERT_NUM_ARGS("lambda", a, 2);
  LASSERT_ARG_TYPE("lambda", a, 0, LVAL_QEXPR);
  LASSERT_ARG_TYPE("lambda", a, 1, LVAL_QEXPR);

  /* first arg should only contain symbols */
  for (int i = 0; i < a->cell[0]->count; i++) {
    LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
            "'lambda' can only define symbols. "
            "Expected %s, got %s for formal argument %i.",
            ltype_name(LVAL_SYM),
            ltype_name(a->cell[0]->cell[i]->type), i);
  }

  lval* formals = lval_pop(a, 0);
  lval* body = lval_pop(a, 0);
  lval_del(a);

  return lval_lambda(formals, body);
}

/* func to define a new env variable */
lval* builtin_var(lenv* e, lval* a, char* func) {
  LASSERT_ARG_TYPE(func, a, 0, LVAL_QEXPR);

  /* first arg is a list of symbols */
  lval* syms = a->cell[0];
  for (int i = 0; i < syms->count; i++) {
    LASSERT(a, syms->cell[i]->type == LVAL_SYM,
            "'%s' can only define symbols. "
            "Expected %s, got %s.",
            func, ltype_name(LVAL_SYM),
            ltype_name(syms->cell[i]->type));
  }
  LASSERT(a, syms->count == a->count-1,
          "'%s' requires same number of values and symbols. "
          "Got %i symbols, and %i values",
          func, syms->count, a->count-1);
  /* assign copies of vals to symbols */
  for (int i = 0; i < syms->count; i++) {
    if (strcmp(func, "def") == 0) {
      lenv_put_global(e, syms->cell[i], a->cell[i+1]);
    }
    if (strcmp(func, "=") == 0) {
      lenv_put(e, syms->cell[i], a->cell[i+1]);
    }
  }
  lval_del(a);
  /* return empty expression */
  return lval_sexpr();
}

/* func to define new local variable */
lval* builtin_put(lenv* e, lval* a) {
  return builtin_var(e, a, "=");
}

/* func to define new global variable */
lval* builtin_def(lenv* e, lval* a) {
  return builtin_var(e, a, "def");
}

/* add a func to an env */
void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
  lval* k = lval_sym(name);
  lval* v = lval_builtin(func);
  lenv_put(e, k, v);
  lval_del(k);
  lval_del(v);
}

/* add our builtin funcs to an env */
void lenv_add_builtins(lenv* e) {
  /* list builtins */
  lenv_add_builtin(e, "list", builtin_list);
  lenv_add_builtin(e, "head", builtin_head);
  lenv_add_builtin(e, "tail", builtin_tail);
  lenv_add_builtin(e, "eval", builtin_eval);
  lenv_add_builtin(e, "join", builtin_join);

  /* math builtins */
  lenv_add_builtin(e, "+", builtin_add);
  lenv_add_builtin(e, "-", builtin_sub);
  lenv_add_builtin(e, "*", builtin_mul);
  lenv_add_builtin(e, "/", builtin_div);

  /* comparison and conditional builtins */
  lenv_add_builtin(e, "if", builtin_if);

  lenv_add_builtin(e, "==", builtin_eq);
  lenv_add_builtin(e, "!=", builtin_ne);
  lenv_add_builtin(e, ">", builtin_greater);
  lenv_add_builtin(e, "<", builtin_less);
  lenv_add_builtin(e, ">=", builtin_weak_greater);
  lenv_add_builtin(e, "<=", builtin_weak_less);

  /* variable and function builtins */
  lenv_add_builtin(e, "lambda", builtin_lambda);
  lenv_add_builtin(e, "def", builtin_def);
  lenv_add_builtin(e, "=", builtin_put);
}

/************************* EVAL FUNCS *************************/

/* handle function calls */
lval* lval_call(lenv* e, lval* f, lval* a) {
  /* if a builtin, apply it */
  if (f->builtin){ return f->builtin(e, a); }

  int given = a->count;
  int total = f->formals->count;
  while (a->count) {
    /* throw error if given too many args */
    if (f->formals->count == 0) {
      lval_del(a);
      return lval_err("Too many arguments given. "
                      "Expected %i, given %i.",
                      total, given);
    }
    /* assign next arg to next symbol */
    lval* sym = lval_pop(f->formals, 0);

    /* deal with & for variable num of args */
    if (strcmp(sym->sym, "&") == 0) {
      if(f->formals->count != 1) {
        lval_del(a);
        return lval_err("Invalid format: '&' not "
                        "followed by single symbol.");
      }
      lval* next = lval_pop(f->formals, 0);
      lenv_put(f->env, next, builtin_list(e, a));
      lval_del(sym);
      lval_del(next);
      break;
    }/* end '&' case */

    lval* val = lval_pop(a, 0);
    lenv_put(f->env, sym, val);

    lval_del(sym);
    lval_del(val);
  }
  lval_del(a);

  /* if only '&' remains, bind to empty list */
  if ( (f->formals->count > 0) &&
       (strcmp(f->formals->cell[0]->sym, "&") == 0)
  ) {
    if (f->formals->count != 2) {
      return lval_err("Invalid format: '&' not "
                      "followed by single symbol.");
    }
    lval_del(lval_pop(f->formals, 0));
    lval* sym = lval_pop(f->formals, 0);
    lval* val = lval_qexpr();
    lenv_put(f->env, sym, val);
    lval_del(sym);
    lval_del(val);
  }

  /* check if all formals have been assigned */
  if (f->formals->count == 0) {
    /* evaluate and return */
    f->env->parent = e;
    return builtin_eval( f->env,
      lval_add(lval_sexpr(), lval_copy(f->body))
    );
  } else {
    /* return partially evaluated func */
    return lval_copy(f);
  }
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
  if (v->count == 1) { return lval_eval(e, lval_take(v, 0)); }

  /* first element should be a function */
  lval* f = lval_pop(v, 0);
  if (f->type != LVAL_FUN) {
    lval* err = lval_err(
      "S-Expression must start with a function. "
      "Expected %s, got %s.",
      ltype_name(LVAL_FUN), ltype_name(f->type)
    );
    lval_del(f);
    lval_del(v);
    return err;
  }

  /* call function */
  lval* result = lval_call(e, f, v);
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

/************************* READ FUNCS *************************/


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
