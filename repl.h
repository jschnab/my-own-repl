#ifndef REPL_HEADER

#include "mpc.h"

typedef struct lval {
    int type;
    long num;
    // error and symbol types have string data
    char* err;
    char* sym;
    int count;
    struct lval** cell;
} lval;

lval* lval_num(long x);

lval* lval_err(char* e);

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

lval* lval_eval(lval* v);

lval* lval_pop(lval* v, int i);

lval* lval_take(lval* v, int i);

lval* builtin_len(lval* a);

lval* builtin_head(lval* a);

lval* builtin_tail(lval* a);

lval* builtin_list(lval* a);

lval* builtin_eval(lval* a);

lval* builtin_join(lval* a);

lval* lval_join(lval* x, lval* y);

lval* builtin(lval* a, char* func);

lval* builtin_op(lval* a, char* op);

lval* lval_eval_sexpr(lval* v);

#endif
