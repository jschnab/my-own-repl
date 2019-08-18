#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"
#include "repl.h"

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

    // macros
#define LASSERT(args, cond, err) \
        if (!(cond)) { lval_del(args); return lval_err(err); }

    // create enumeration of possible lval types
    enum {LVAL_NUM, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR, LVAL_ERR};

    // create enumeration of possible error types
    enum {LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM};

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

lval* lval_qexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
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
            break;

        case LVAL_QEXPR:
        case LVAL_SEXPR:
            for (int i = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }
            free(v->cell);
            break;
    }

    free(v);
}

// function to convert an AST node to a number lval
lval* lval_read_num(mpc_ast_t* t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
        return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

// function to add an AST element to the list of element
lval* lval_add(lval* v, lval* x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count - 1] = x;
    return v;
}

// this function converts an AST node and its children to an lval
lval* lval_read(mpc_ast_t* t) {
    // if Symbol or Number return conversion to this type
    if (strstr(t->tag, "number")) { return lval_read_num(t); }
    if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

    // if root (>) or sexpr or qexpr then create empty list
    lval* x = NULL;
    if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
    if (strstr(t->tag, "sexpr")) { x = lval_sexpr(); }
    if (strstr(t->tag, "qexpr")) { x = lval_qexpr(); }

    // fill this list with any valid expression contained within
    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) {continue;}
        if (strcmp(t->children[i]->contents, ")") == 0) {continue;}
        if (strcmp(t->children[i]->contents, "{") == 0) {continue;}
        if (strcmp(t->children[i]->contents, "}") == 0) {continue;}
        if (strcmp(t->children[i]->tag, "regex") == 0) {continue;}
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}

void lval_expr_print(lval* v, char open, char close) {
    putchar(open);
    for (int i = 0; i < v->count; i++) {
        // print value contained within
        lval_print(v->cell[i]);
        // don't print trailing space if last element
        if (i != (v->count - 1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

// function which prints lval
void lval_print(lval* v) {
    switch (v->type) {
        case LVAL_NUM:
            printf("%li", v->num);
            break;

        case LVAL_ERR:
            printf("Error: %s", v->err);
            break;

        case LVAL_SYM:
            printf("%s", v->sym);
            break;

        case LVAL_SEXPR:
            lval_expr_print(v, '(', ')');
            break;

        case LVAL_QEXPR:
            lval_expr_print(v, '{', '}');
            break;
    }
}

// function which prints a line of lval
void lval_println(lval* v) { lval_print(v); putchar('\n'); }

// function to evaluate lval
lval* lval_eval(lval* v) {
    // evaluate S-expressions
    if (v->type == LVAL_SEXPR) {return lval_eval_sexpr(v);}
    // all other lval types remain the same
    return v;
}

// function to get ith element of list
lval* lval_pop(lval* v, int i) {
    // find the item at i
    lval* x = v->cell[i];

    // shift memory after i
    memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count -i -1));

    // decrease item count
    v->count--;

    // reallocate memory used
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    
    return x;
}

// function that pops and deletes
lval* lval_take(lval* v, int i) {
    lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

// function which performs calculations on lval
lval* builtin(lval* a, char* func) {
    if (strcmp("len", func) == 0) { return builtin_len(a); }
    if (strcmp("list", func) == 0) { return builtin_list(a); }
    if (strcmp("head", func) == 0) { return builtin_head(a); }
    if (strcmp("tail", func) == 0) { return builtin_tail(a); }
    if (strcmp("join", func) == 0) { return builtin_join(a); }
    if (strcmp("eval", func) == 0) { return builtin_eval(a); }
    if (strstr("+-/*", func)) { return builtin_op(a, func); }
    lval_del(a);
    return lval_err("unknown function");
}

// function which performs calculations on lval
lval* builtin_op(lval* a, char* op) {
    // ensure all elements of a are numbers
    for (int i = 0; i < a->count; i++) {
        if (a->cell[i]->type != LVAL_NUM) {
            lval_del(a);
            return lval_err("cannot operate on non-number");
        }
    }

    // pop first element
    lval* x = lval_pop(a, 0);

    // if no other elements and subtraction perform unary negation
    if ((strcmp(op, "-") == 0) && a->count == 0) {
        x->num = -x->num;
    }

    // while still elements remaining...
    while (a->count > 0) {
        // ...pop next element
        lval* y = lval_pop(a, 0);

        // ...do mathematical operation
        if (strcmp(op, "+") == 0) {x->num += y->num;}
        if (strcmp(op, "-") == 0) {x->num -= y->num;}
        if (strcmp(op, "*") == 0) {x->num *= y->num;}
        if (strcmp(op, "/") == 0) {
            if (y->num == 0) {
                lval_del(x); lval_del(y);
                x = lval_err("division by zero"); break;
            }
            x->num /= y->num;
        }

        lval_del(y);
    }

    lval_del(a);

    return x;
}

// function that returns the number of elements in a Q-expression
lval* builtin_len(lval* a) {
    // check error condition
    LASSERT(
        a,
        a->count == 1,
        "function 'len' was passed too many arguments"
    );

    LASSERT(
        a,
        a->cell[0]->type == LVAL_QEXPR,
        "function 'len' was passed incorrect type"
    );

    return lval_num((long) a->cell[0]->count);
}

lval* builtin_head(lval* a) {
    // check error condition
    LASSERT(
        a,
        a->count == 1,
        "function 'head' was passed too many arguments"
    );

    LASSERT(
        a,
        a->cell[0]->type == LVAL_QEXPR,
        "function 'head' was passed incorrect type"
    );

    LASSERT(
        a,
        a->cell[0]->count != 0,
        "function 'head' passed {}"
    );

    // otherwise take first arg
    lval* v = lval_take(a, 0);

    // delete all elements that are not head and return
    while (v->count > 1)
        lval_del(lval_pop(v, 1));

    return v;
}

lval* builtin_tail(lval* a) {
    // check error condition
    LASSERT(
        a,
        a->count == 1,
        "function 'tail' was passed too many arguments"
    );

    LASSERT(
        a,
        a->cell[0]->type == LVAL_QEXPR,
        "function 'tail' was passed incorrect type"
    );

    LASSERT(
        a,
        a->cell[0]->count != 0,
        "function 'tail' passed {}"
    );

    lval* v = lval_take(a, 0);
    lval_del(lval_pop(v, 0));

    return v;
}

lval* builtin_list(lval* a) {
    a->type = LVAL_QEXPR;
    return a;
}

lval* builtin_eval(lval* a) {
    LASSERT(
        a,
        a->count == 1,
        "function 'eval' was passed too many arguments"
    );

    LASSERT(
        a,
        a->cell[0]->type == LVAL_QEXPR,
        "function 'eval' was passed incorrect type"
    );

    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;

    return lval_eval(x);
}

lval* builtin_join(lval* a) {
    for (int i = 0; i < a->count; i++) {
        LASSERT(
            a,
            a->cell[i]->type == LVAL_QEXPR,
            "function 'join' was passed incorrect type"
        );
    }

    lval* x = lval_pop(a, 0);

    while (a->count)
        x = lval_join(x, lval_pop(a, 0));

    lval_del(a);

    return x;
}

lval* lval_join(lval* x, lval* y) {
    // for each cell in y add it to x
    while (y->count)
        x = lval_add(x, lval_pop(y, 0));

    // delete the empty y and return x
    lval_del(y);
    return x;
}

// function to evaluate S-expressions (error checking, etc)
lval* lval_eval_sexpr(lval* v) {
    // evaluate children
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(v->cell[i]);
    }

    // error checking
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) {return lval_take(v, i);}
    }

    // empty expression
    if (v->count == 0) {return v;}

    // single expression
    if (v->count == 1) {return lval_take(v, 0);}

    // ensure first element is symbol
    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_SYM) {
        lval_del(f);
        lval_del(v);
        return lval_err("S-expression does not start with symbol.");
    }

    // call builtin with operator
    lval* result = builtin(v, f->sym);
    lval_del(f);

    return result;
}

int main(int argc, char* argv[]) {

    // create parsers
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Qexpr = mpc_new("qexpr");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    // define parsers
    mpca_lang(
            MPCA_LANG_DEFAULT,
            "\
            number   : /-?[0-9]+/ ;\
            symbol   : '+' | '-' | '*' | '/' | \"len\" |\
                       \"list\" | \"head\" | \"tail\"|  \"join\" | \"eval\" ;\
            sexpr    : '(' <expr>* ')' ;\
    	    qexpr    : '{' <expr>* '}' ;\
            expr     : <number> |  <symbol> | <sexpr> | <qexpr> ;\
            lispy    : /^/ <expr>* /$/ ;\
            ",
            Number,
            Symbol,
            Sexpr,
    	    Qexpr,
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
            lval* x = lval_eval(lval_read(r.output));
            lval_println(x);
            lval_del(x);
        }
        else {
            // otherwise print error
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    // undefine and delete parsers
    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

    return 0;
}
