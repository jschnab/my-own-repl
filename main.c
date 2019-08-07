#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

// if we are compiling on Windows
#ifdef _WIN32
#include <string.h>

static char buffer[2048];

// fake readline function
char* readline(char* prompt) {
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char* cpy = malloc(strlen(buffer) + 1);
    strcpy(cpy, buffer);
    cpy[strl(cpy)-1] = '\0';
    return cpy;
}

// fake add_history function
void add_history(char* unused) {}

// if we are on MacOS or Linux
#else
#include <editline/readline.h>
#include <editline/history.h>
#endif



// declare a new 'lval' struct
typedef struct lval {
    int type;
    long num;
    // error and symbol types have string data
    char* err;
    char* sym;
    int count;
    struct lval** cell;
} lval;

// create enumeration of possible lval types
enum {LVAL_NUM, LVAL_SYM, LVAL_SEXPR, LVAL_ERR};

// create enumeration of possible error types
enum {LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM};

// function to print an 'lval' type
void lval_print(lval v) {
    switch (v.type) {
        // if type is a number print it
        case LVAL_NUM: 
            printf("%li\n", v.num); break;

        case LVAL_ERR:
            if (v.err == LERR_DIV_ZERO)
                printf("Error: Division by zero.\n");

            else if (v.err == LERR_BAD_OP)
                printf("Error: Invalid operator.\n");
            
            else if (v.err == LERR_BAD_NUM)
                printf("Error: Invalid number.\n");

            break;

    }
}

// function to create a new number type lval
lval* lval_num(long x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

// construct a pointer to a new error type lval
lval* lval_err(char* e) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(e) + 1);
    strcpy(v->err, e);
    return v;
}

// construct a pointer to new Symbol lval
lval* lval_sym(char* s) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

// construct a pointer to a new empty Sexpr lval
lval* lval_sexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

// function to delete an lval
void lval_del(lval* v) {
    switch (v->type) {
        case LVAL_NUM:
            break;

        case LVAL_ERR:
            free(v->err);
            break;

        case LVAL_SYM:
            free(v->sym);

        case LVAL_SEXPR:
            int i = 0;
            for (i = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }
            free(v->cell);
        break;
    }

    free(v);
}

lval* lval_read_num(mpc_ast_t* t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
        return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval* lval_read(mpc_ast_t* t) {
    // if Symbol or Number return conversion to this type
    if (strstr(t->tag, "number")) { return lval_read_num(t); }
    if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

    // if root (>) or sexpr then create empty list
    lval* x = NULL;
    if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
    if (strstr(t->tag, "sexpr")) { x = lval_sexpr(); }

    // fill this list with any valid expression contained within
    int i = 0;
    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) {continue;}
        if (strcmp(t->children[i]->contents, ")") == 0) {continue;}
        if (strcmp(t->children[i]->tag, "regex") == 0) {continue;}
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}

lval* lval_add(lval* v, lval* x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count - 1] = x;
    return v;
}

// forward declaration because lval_expr_print() needs it
void lval_print(lval* v);

void lval_expr_print(lval* v, char open, char close) {
    putchar(open);
    int i = 0;
    for (i = 0; i < v->count; i++) {
        // print value contained within
        lval_print(v->cell[i]);
        // don't print trailing space if last element
        if (i != (v->count - 1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

void lval_print(lval* v) {
    switch (v->type) {
        case LVAL_NUM:
            printf("%li", v->num);
            break;

        case LVAL_ERR:
            printf("Error %s", v->err);
            break;

        case LVAL_SYM:
            printf("%s", v->sym);
            break;

        case LVAL_SEXPR:
            lval_expr_print(v, '(', ')');
            break;
    }
}

void lval_println(lval* v) { lval_print(v); putchar('\n'); }

// function to evaluate operators and numbers
lval eval_op(lval x, char* op, lval y) {

    // if x or y is an error return it
    if (x.type == LVAL_ERR) { return x; }
    if (y.type == LVAL_ERR) { return y; }

    if (strcmp(op, "+") == 0) { return lval_num(x.num + y.num); }
    if (strcmp(op, "_") == 0) { return lval_num(x.num - y.num); }
    if (strcmp(op, "*") == 0) { return lval_num(x.num * y.num); }
    if (strcmp(op, "/") == 0) {
        // if divisor is 0 return error
        return y.num == 0 
            ? lval_err(LERR_DIV_ZERO)
            : lval_num(x.num / y.num);
    }

    return lval_err(LERR_BAD_OP);
}

// function to evaluate the tree
lval eval(mpc_ast_t* t) {

    // if tagged as number return directly
    if (strstr(t->tag, "number")) {
        // check if there is some error in conversion
        errno = 0;
        long x = strtol(t->contents, NULL, 10);
        return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
    }

    // operator is always second child
    char* op = t->children[1]->contents;

    // we store the third child in variable x
    lval x = eval(t->children[2]);

    // iterate the remaining children and combine
    int i = 3;
    while (strstr(t->children[i]->tag, "expr")) {
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }

    return x;
}

int main(int argc, char* argv[]) {

    // create parsers
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    // define parsers
    mpca_lang(
            MPCA_LANG_DEFAULT,
            "\
            number   : /-?[0-9]+/ ;\
            symbol   : '+' | '-' | '*' | '/' ;\
            sexpr    : '(' <expr>* ')' ;\
            expr     : <number> |  <symbol> | <sexpr> ;\
            lispy    : /^/ <expr>* /$/ ;\
            ",
            Number,
            Symbol,
            Sexpr,
            Expr,
            Lispy
    );
    
    puts("Welcome to REPL version 0.0.3");
    puts("Press Ctrl+c to exit\n");

    while(1){

        // get user input
        char* input = readline("repl> ");
        add_history(input);

        // parse user input
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            // on success print the evaluated output
            lval result = eval(r.output);
            lval_print(result);
            mpc_ast_delete(r.output);
        }
        else {
            // otherwise print error
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    // undefine and delete parsers
    mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lispy);

    return 0;
}
