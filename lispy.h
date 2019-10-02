#ifndef REPL_HEADER

#include "mpc.h"

typedef struct lval lval;

typedef struct lenv lenv;

struct lenv {
    lenv* par;
    int count;
    char** syms;
    lval** vals;
};

lenv* lenv_new(void);

void lenv_del(lenv* e);

lval* lenv_get(lenv* e, lval* k);

void lenv_put(lenv* e, lval* k, lval* v);

void lenv_def(lenv* e, lval* k, lval* v);

lenv* lenv_copy(lenv* e);

typedef lval*(*lbuiltin)(lenv*, lval*);

typedef struct lval {
    int type;
    // basic
    long num;
    char* err;
    char* sym;
    // functions
    lbuiltin builtin;
    lenv* env;
    lval* formals;
    lval* body;
    // expression
    int count;
    lval** cell;
} lval;

lval* lval_copy(lval* v);

lval* lval_fun(lbuiltin func);

lval* lval_lambda(lval* formals, lval* body);

lenv* lenv_new(void);

void lenv_del(lenv* e);

lval* lenv_get(lenv* e, lval* k);

void lenv_put(lenv* e, lval* k, lval* v);

lval* lval_num(long x);

lval* lval_err(char* fmt, ...);

char* ltype_name(int t);

lval* lval_sym(char* s);

lval* lval_sexpr(void);

lval* lval_qexpr(void);

void lval_del(lval* v);

lval* lval_read_num(mpc_ast_t* t);

lval* lval_add(lval* v, lval* x);

lval* lval_read(mpc_ast_t* t);

void lval_print(lval* v);

void lval_expr_print(lval* v, char open, char close);

void lval_print(lval* v);

void lval_println(lval* v);

lval* lval_eval(lenv* e, lval* v);

lval* lval_pop(lval* v, int i);

lval* lval_take(lval* v, int i);

lval* builtin_add(lenv* e, lval* a);

lval* builtin_sub(lenv* e, lval* a);

lval* builtin_mul(lenv* e, lval* a);

lval* builtin_div(lenv* e, lval* a);

lval* builtin_len(lenv* e, lval* a);

lval* builtin_head(lenv* e, lval* a);

lval* builtin_tail(lenv* e, lval* a);

lval* builtin_list(lenv* e, lval* a);

lval* builtin_eval(lenv* e, lval* a);

lval* builtin_join(lenv* e, lval* a);

lval* builtin_def(lenv* e, lval* a);

lval* builtin_put(lenv* e, lval* a);

lval* builtin_var(lenv* e, lval* a, char* func);

lval* builtin_gt(lenv* e, lval* a);

lval* builtin_lt(lenv* e, lval* a);

lval* builtin_ge(lenv* e, lval* a);

lval* builtin_le(lenv* e, lval* a);

lval* builtin_ord(lenv* e, lval* a, char* op);

lval* builtin_cmp(lenv* e, lval* a, char* op);

lval* builtin_if(lenv* e, lval* a);

int lval_eq(lval* x, lval* y);

lval* lval_join(lval* x, lval* y);

lval* builtin(lval* a, char* func);

lval* builtin_op(lenv* e, lval* a, char* op);

lval* lval_call(lenv*e, lval* f, lval* a);

lval* lval_eval_sexpr(lenv* e, lval* v);

void lenv_add_builtin(lenv* e, char* name, lbuiltin func);

void lenv_add_builtins(lenv* e);

#endif
