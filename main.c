#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"
#include "lispy.h"

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
#define LASSERT(args, cond, fmt, ...) \
if (!(cond)) { \
    lval* err = lval_err(fmt, ##__VA_ARGS__); \
    lval_del(args); \
    return err; \
}

#define LASSERT_TYPE(func, args, index, expected) \
    LASSERT(args, args->cell[index]->type == expected, \
        "function '%s' passed incorrect type for argument %i " \
        "(got '%s', expected: '%s')", \
        func, index, ltype_name(args->cell[index]->type), ltype_name(expected));

#define LASSERT_NUM(func, args, expected) \
    LASSERT(args, args->count == expected, \
        "function '%s' was passed incorrect number of arguments" \
        "(got %i, expected: %i)", \
        func, args->count, expected);

#define LASSERT_NOT_EMPTY(func, args, index) \
    LASSERT(args, args->cell[index]->count != 0, \
        "function '%s' was passed {} for argument %i", \
        func, index);

// create enumeration of possible lval types
enum {LVAL_NUM, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR, LVAL_ERR, LVAL_FUN};

// create enumeration of possible error types
enum {LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM};

// function to create an lenv
lenv* lenv_new(void) {
    lenv* e = malloc(sizeof(lenv));
    e->par = NULL;
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

// function to delete an lenv
void lenv_del(lenv* e) {
    for (int i = 0; i < e->count; i++) {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
}

lval* lenv_get(lenv* e, lval* k) {
    // iterate over all items in environment
    for (int i = 0; i < e->count; i++) {
        // check if the stored string matches the symbol string
        // if it does, return a copy of the value
        if (strcmp(e->syms[i], k->sym) == 0) {
            return lval_copy(e->vals[i]);
        }
    }
    // if no symbol found, check in parent otherwise return error
    if (e->par)
        return lenv_get(e->par, k);
    else
        return lval_err("unbound symbol '%s'", k->sym);
}


// function to put functions in the local environment
void lenv_put(lenv* e, lval* k, lval* v) {
    // iterate over all items in environment
    // this is to see if variable already exists
    for (int i = 0; i < e->count; i++) {
        // if variable is found, delete item at this position
        // and replace with variable supplied by user
        if (strcmp(e->syms[i], k->sym) == 0) {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }

    // if no existing entry found allocate space for new entry
    e->count++;
    e->vals = realloc(e->vals, sizeof(lval*) * e->count);
    e->syms = realloc(e->syms, sizeof(lval*) * e->count);

    // copy contents of lval and symbol string into new location
    e->vals[e->count - 1] = lval_copy(v);
    e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
    strcpy(e->syms[e->count - 1], k->sym);
}


// function to put functions in the global environment
void lenv_def(lenv* e, lval* k, lval* v) {
    // iterate until 'e' has no parent
    while (e->par)
        e = e->par;
    // put value in e
    lenv_put(e, k, v);
}


// function to copy environments
lenv* lenv_copy(lenv* e) {
    lenv* n = malloc(sizeof(lenv));
    n->par = e->par;
    n->count = e->count;
    n->syms = malloc(sizeof(char*) * n->count);
    n->vals = malloc(sizeof(lval*) * n->count);
    for (int i = 0; i < n->count; i++) {
        n->syms[i] = malloc(strlen(e->syms[i]) + 1);
        strcpy(n->syms[i], e->syms[i]);
        n->vals[i] = lval_copy(e->vals[i]);
    }
    return n;
}

// function to create a new number type lval
lval* lval_num(long x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

// construct a pointer to a new error type lval
lval* lval_err(char* fmt, ...) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;

    // create a va list and initialize it
    va_list va;
    va_start(va, fmt);

    // allocate 512 bytes of space
    v->err = malloc(512);

    // print the error string with a maximum of 511 characters
    vsnprintf(v->err, 511, fmt, va);

    // reallocate to number of bytes actually used
    v->err = realloc(v->err, strlen(v->err) + 1);

    // cleanup our va list
    va_end(va);

    return v;
}

// function to construct a user-defined 'lval' function
lval* lval_lambda(lval* formals, lval* body) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUN;

    // set builtin to Null
    v->builtin = NULL;

    // build the new environment
    v->env = lenv_new();

    // set formal and body
    v->formals = formals;
    v->body = body;

    return v;
}

// function which maps a function to its string representation
char* ltype_name(int t) {
    switch(t) {
        case LVAL_FUN: return "Function";
        case LVAL_NUM: return "Number";
        case LVAL_ERR: return "Error";
        case LVAL_SYM: return "Symbol";
        case LVAL_SEXPR: return "S-Expression";
        case LVAL_QEXPR: return "Q-Expression";
        default: return "Unknown";
    }
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

lval* lval_fun(lbuiltin func) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->builtin = func;
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
            for (int i = 0; i < v->count; i++)
                lval_del(v->cell[i]);
            free(v->cell);
            break;

        case LVAL_FUN:
            if (!v->builtin) {
                lenv_del(v->env);
                lval_del(v->formals);
                lval_del(v->body);
            }
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

        case LVAL_FUN:
            if (v->builtin)
                printf("<builtin>");
            else {
                printf("(\\ ");
                lval_print(v->formals);
                putchar(' ');
                lval_print(v->body);
                putchar(')');
            }
            break;
    }
}

// function which prints a line of lval
void lval_println(lval* v) { lval_print(v); putchar('\n'); }

// function to evaluate lval
lval* lval_eval(lenv* e, lval* v) {
    if (v->type == LVAL_SYM) {
        lval* x = lenv_get(e, v);
        lval_del(v);
        return x;
    }

    // evaluate S-expressions
    if (v->type == LVAL_SEXPR) {return lval_eval_sexpr(e, v);}

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

lval* lval_copy(lval* v) {
    lval* x = malloc(sizeof(lval));
    x->type = v->type;

    switch (v->type) {

        // copy functions and number directly
        case LVAL_FUN:
            if (v->builtin)
                x->builtin = v->builtin;
            else {
                x->builtin = NULL;
                x->env = lenv_copy(v->env);
                x->formals = lval_copy(v->formals);
                x->body = lval_copy(v->body);
            }
            break;

        case LVAL_NUM:
            x->num = v->num;
            break;

        // copy string using malloc and strcpy
        case LVAL_ERR:
            x->err = malloc(strlen(v->err) + 1);
            strcpy(x->err, v->err);
            break;

        case LVAL_SYM:
            x->sym = malloc(strlen(v->sym) + 1);
            strcpy(x->sym, v->sym);
            break;

        // copy lists by copying each sub-expression
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->count = v->count;
            x->cell = malloc(sizeof(lval*) * x->count);
            for (int i = 0; i < x->count; i++)
                x->cell[i] = lval_copy(v->cell[i]);
            break;
    }

    return x;
}


// function that pops and deletes
lval* lval_take(lval* v, int i) {
    lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

int lval_eq(lval* x, lval* y) {
    // different types are always unequal
    if (x->type != y->type) { return 0; }

    // compare type
    switch (x->type) {
        // compare number value
        case LVAL_NUM: return (x->num == y->num);

        // compare string values
        case LVAL_ERR: return (strcmp(x->err, y->err) == 0);
        case LVAL_SYM: return (strcmp(x->sym, y->sym) == 0);

        // if builtin compare, otherwise compare formals and body
        case LVAL_FUN:
            if (x->builtin || y->builtin) {
                return x->builtin == y->builtin;
            } else {
                return lval_eq(x->formals, y->formals)
                    && lval_eq(x->body, y->body);
            }

        // if list compare every individual element
        case LVAL_QEXPR:
        case LVAL_SEXPR:
            if (x->count != y->count) { return 0; }
            for (int i = 0; i < x->count; i++) {
                // if any element not equal then whole list not equal
                if (!lval_eq(x->cell[i], y->cell[i])) { return 0; }
            }
            return 1;
        break;
    }
    return 0;
}


// function which performs calculations on lval
lval* builtin_op(lenv* e, lval* a, char* op) {
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


lval* builtin_def(lenv* e, lval* a) {
    return builtin_var(e, a, "def");
}


lval* builtin_put(lenv* e, lval* a) {
    return builtin_var(e, a, "=");
}


lval* builtin_gt(lenv* e, lval* a) {
    return builtin_ord(e, a, ">");
}


lval* builtin_lt(lenv* e, lval* a) {
    return builtin_ord(e, a, "<");
}


lval* builtin_ge(lenv* e, lval* a) {
    return builtin_ord(e, a, ">=");
}

lval* builtin_le(lenv* e, lval* a) {
    return builtin_ord(e, a, "<=");
}


lval* builtin_cmp(lenv* e, lval* a, char* op) {
    LASSERT_NUM(op, a, 2);
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


// function to perform conditionals
lval* builtin_if(lenv* e, lval* a) {
    LASSERT_NUM("if", a, 3);
    LASSERT_TYPE("if", a, 0, LVAL_NUM);
    LASSERT_TYPE("if", a, 1, LVAL_QEXPR);
    LASSERT_TYPE("if", a, 2, LVAL_QEXPR);

    // mark both expression as evaluable
    lval* x;
    a->cell[1]->type = LVAL_SEXPR;
    a->cell[2]->type = LVAL_SEXPR;

    if (a->cell[0]->num) {
        // if condition is true evaluate first expression
        x = lval_eval(e, lval_pop(a, 1));
    } else {
        // otherwise evaluate second expression
        x = lval_eval(e, lval_pop(a, 2));
    }

    // delete argument list and return
    lval_del(a);
    return x;
}


// function to define your own variables
lval* builtin_var(lenv* e, lval* a, char* func) {
    LASSERT_TYPE(func, a, 0, LVAL_QEXPR);

    // first argument is symbol list
    lval* syms = a->cell[0];

    // ensure all elements of first list are symbols
    for (int i = 0; i < syms->count; i++) {
        LASSERT(
            a, syms->cell[i]->type == LVAL_SYM,
            "function '%s' cannot define non-symbol "
            "(got '%s', expected: '%s')", func,
            ltype_name(syms->cell[i]->type),
            ltype_name(LVAL_SYM)
        );
    }

    // check correct number of symbols and values
    LASSERT(
        a, syms->count == a->count - 1,
        "function '%s' cannot define incorrect number of values to symbols"
        " (got %i, expected: %i)", func,
        syms->count, a->count - 1
    );

    // assign copies of values to symbols
    for (int i = 0; i < syms->count; i++) {
        // if 'def' define globally
        if (strcmp(func, "def") == 0)
            lenv_def(e, syms->cell[i], a->cell[i + 1]);

        // if '=' define locally
        if (strcmp(func, "=") == 0)
            lenv_put(e, syms->cell[i], a->cell[i + 1]);
    }

    lval_del(a);
    return lval_sexpr();
}


// function which defines a lambda function
lval* builtin_lambda(lenv* e, lval* a) {
    // check two arguments are Q-Expressions
    LASSERT_NUM("\\", a, 2);
    LASSERT_TYPE("\\", a, 0, LVAL_QEXPR);
    LASSERT_TYPE("\\", a, 1, LVAL_QEXPR);

    // check first Q-Expression contains only symbols
    for (int i = 0; i < a->count; i++) {
        LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
            "cannot define non-symbol (got '%s', expected: '%s')",
            ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM));
    }

    // pop first two arguments and pass them to lval_lambda
    lval* formals = lval_pop(a, 0);
    lval* body = lval_pop(a, 0);
    lval_del(a);

    return lval_lambda(formals, body);
}


// function which registers the builtin function with an environment
void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
    lval* k = lval_sym(name);
    lval* v = lval_fun(func);
    lenv_put(e, k, v);
    lval_del(k);
    lval_del(v);
}


// function which registers all builtin functions with an environment
void lenv_add_builtins(lenv* e) {
    // list functions
    lenv_add_builtin(e, "len", builtin_len);
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);
    lenv_add_builtin(e, "def", builtin_def);
    lenv_add_builtin(e, "=", builtin_put);
    lenv_add_builtin(e, "\\", builtin_lambda);

    // mathematical functions
    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "/", builtin_div);

    // comparison functions
    lenv_add_builtin(e, "if", builtin_if);
    lenv_add_builtin(e, "==", builtin_eq);
    lenv_add_builtin(e, "!=", builtin_ne);
    lenv_add_builtin(e, ">", builtin_gt);
    lenv_add_builtin(e, "<", builtin_lt);
    lenv_add_builtin(e, ">=", builtin_ge);
    lenv_add_builtin(e, "<=", builtin_le);
}


// function that returns the number of elements in a Q-expression
lval* builtin_len(lenv* e, lval* a) {
    // check error condition
    LASSERT(
        a,
        a->count == 1,
        "function 'len' was passed too many arguments "
        "(got %i, expected: %i)",
        a->count, 1
    );

    LASSERT(
        a,
        a->cell[0]->type == LVAL_QEXPR,
        "function 'len' was passed incorrect type "
        "(got '%s', expected '%s')",
        ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR)
    );

    return lval_num((long) a->cell[0]->count);
}

lval* builtin_head(lenv* e, lval* a) {
    // check error condition
    LASSERT(
        a,
        a->count == 1,
        "function 'head' was passed too many arguments "
        "(got %i, expect %i)", a->count, 1
    );

    LASSERT(
        a,
        a->cell[0]->type == LVAL_QEXPR,
        "function 'head' was passed incorrect type "
        "(got '%s', expected '%s')",
        ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR)

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

lval* builtin_tail(lenv* e, lval* a) {
    // check error condition
    LASSERT(
        a,
        a->count == 1,
        "function 'head' was passed too many arguments "
        "(got %i, expect %i)", a->count, 1

    );

    LASSERT(
        a,
        a->cell[0]->type == LVAL_QEXPR,
        "function 'tail' was passed incorrect type "
        "(got '%s', expected '%s')",
        ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR)

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

lval* builtin_list(lenv* e, lval* a) {
    a->type = LVAL_QEXPR;
    return a;
}

lval* builtin_eval(lenv* e, lval* a) {
    LASSERT(
        a,
        a->count == 1,
        "function 'eval' was passed too many arguments "
        "(got %i, expect %i)", a->count, 1
    );

    LASSERT(
        a,
        a->cell[0]->type == LVAL_QEXPR,
        "function 'eval' was passed incorrect type "
        "(got '%s', expected '%s')",
        ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR)

    );

    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;

    return lval_eval(e, x);
}

lval* builtin_join(lenv* e, lval* a) {
    for (int i = 0; i < a->count; i++) {
        LASSERT(
            a,
            a->cell[i]->type == LVAL_QEXPR,
            "function 'join' was passed incorrect type "
            "(got '%s', expected '%s')",
            ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR)

        );
    }

    lval* x = lval_pop(a, 0);

    while (a->count)
        x = lval_join(x, lval_pop(a, 0));

    lval_del(a);

    return x;
}


// fonction to perform number comparisons
lval* builtin_ord(lenv* e, lval* a, char* op) {
    LASSERT_NUM(op, a, 2);
    LASSERT_TYPE(op, a, 0, LVAL_NUM);
    LASSERT_TYPE(op, a, 1, LVAL_NUM);

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


lval* lval_join(lval* x, lval* y) {
    // for each cell in y add it to x
    while (y->count)
        x = lval_add(x, lval_pop(y, 0));

    // delete the empty y and return x
    lval_del(y);
    return x;
}

// function to evaluate S-expressions (error checking, etc)
lval* lval_eval_sexpr(lenv* e, lval* v) {
    // evaluate children
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(e, v->cell[i]);
    }

    // error checking
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) {return lval_take(v, i);}
    }

    // empty expression
    if (v->count == 0) {return v;}

    // single expression
    if (v->count == 1) {return lval_take(v, 0);}

    // ensure first element is a function after evaluation
    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_FUN) {
        lval* err = lval_err(
            "S-Expression starts with incorrect type "
            "(got '%s', expected: '%s')",
            ltype_name(f->type), ltype_name(LVAL_FUN));
        lval_del(f);
        lval_del(v);
        return err;
    }

    // call function
    lval* result = lval_call(e, f, v);
    lval_del(f);

    return result;
}


// function which calls a function
lval* lval_call(lenv* e, lval* f, lval* a) {
    // if builtin then simply apply that
    if (f->builtin)
        return f->builtin(e, a);

    // record argument counts
    int given = a->count;
    int total = f->formals->count;

    // while arguments still remain to be processed
    while (a->count) {
        // if we've ran out of formal arguments to bind
        if (f->formals->count == 0) {
            lval_del(a);
            return lval_err(
                "function passed too many arguments "
                "(got %i, expected: %i)", given, total
            );
        }

        // pop the first symbol from the formals
        lval* sym = lval_pop(f->formals, 0);

        // special case to deal with '&'
        if (strcmp(sym->sym, "&") == 0) {
            // ensure '&' is followed by another symbol
            if (f->formals->count != 1) {
                lval_del(a);
                return lval_err("function format invalid, "
                    "symbol '&' not followed by single symbol");
            }

            // next formal should be bound to remaining arguments
            lval* nsym = lval_pop(f->formals, 0);
            lenv_put(f->env, nsym, builtin_list(e, a));
            lval_del(sym);
            lval_del(nsym);
            break;
        }

        // pop the next argument from the list
        lval* val = lval_pop(a, 0);

        // bind a copy into the function's environment
        lenv_put(f->env, sym, val);

        // delete symbol and value
        lval_del(sym);
        lval_del(val);

    }

    // argument list is now bound so can be cleaned up
    lval_del(a);

    // if '&' remains in formal list then bind to empty list
    if (f->formals->count > 0 &&
        strcmp(f->formals->cell[0]->sym, "&") == 0) {

        // check to ensure '&' is not passed invalidly
        if (f->formals->count !=2) {
            return lval_err("function format invalid, "
                "symbol '&' not followed by single symbol");
        }

        // pop and delete '&' symbol
        lval_del(lval_pop(f->formals, 0));

        // pop next symbol and create empty list
        lval* sym = lval_pop(f->formals, 0);
        lval* val = lval_qexpr();

        // bind to environment and delete
        lenv_put(f->env, sym, val);
        lval_del(sym);
        lval_del(val);
    }

    // if all formal have been bound, then evaluate
    if (f->formals->count == 0) {
        // set environment parent to evaluation environment
        f->env->par = e;

        // evaluate and return
        return builtin_eval(
            f->env,
            lval_add(lval_sexpr(), lval_copy(f->body))
        );
    }
    else
        // otherwise return partially evaluated function
        return lval_copy(f);
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
            symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;\
            sexpr    : '(' <expr>* ')' ;\
    	    qexpr    : '{' <expr>* '}' ;\
            expr     : <number> | <symbol> | <sexpr> | <qexpr> ;\
            lispy    : /^/ <expr>* /$/ ;\
            ",
            Number,
            Symbol,
            Sexpr,
    	    Qexpr,
            Expr,
            Lispy
    );
    
    puts("Welcome to REPL version 0.0.13");
    puts("Press Ctrl+c to exit\n");

    // create an environment and register builtin functions
    lenv* e = lenv_new();
    lenv_add_builtins(e);

    while(1){

        // get user input
        char* input = readline("lispy> ");
        add_history(input);

        // parse user input
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {

            // on success print the evaluated output
            lval* x = lval_eval(e, lval_read(r.output));
            lval_println(x);
            lval_del(x);
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
    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

    // delete env
    lenv_del(e);

    return 0;
}
