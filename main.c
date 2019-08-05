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
typedef struct {
    int type;
    long num;
    int err;
} lval;

// create enumeration of possible lval types
enum {LVAL_NUM, LVAL_ERR};

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
lval lval_num(long x) {
    lval v;
    v.type = LVAL_NUM;
    v.num = x;
    return v;
}

// function to create a new error type lval
lval lval_err(int x) {
    lval v;
    v.type = LVAL_ERR;
    v.err = x;
    return v;
}

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
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    // define parsers
    mpca_lang(
            MPCA_LANG_DEFAULT,
            "\
            number   : /-?[0-9]+/ ;\
            operator : '+' | '-' | '*' | '/' ;\
            expr     : <number> | '(' <operator> <expr>+ ')' ;\
            lispy    : /^/ <operator> <expr>+ /$/ ;\
            ",
            Number,
            Operator,
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
    mpc_cleanup(4, Number, Operator, Expr, Lispy);

    return 0;
}
