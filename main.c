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

long eval_op(long x, char* op, long y) {
    if (strcmp(op, "+") == 0) { return x + y; }
    if (strcmp(op, "_") == 0) { return x - y; }
    if (strcmp(op, "*") == 0) { return x * y; }
    if (strcmp(op, "/") == 0) { return x / y; }
    return 0;
}

long eval(mpc_ast_t* t) {
    // if tagged as number return directly
    if (strstr(t->tag, "number")) {
        return atoi(t->contents);
    }

    // operator is always second child
    char* op = t->children[1]->contents;

    // we store the third child in variable x
    long x = eval(t->children[2]);

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
            long result = eval(r.output);
            printf("%li\n", result);
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
