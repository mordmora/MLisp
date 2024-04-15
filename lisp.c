#include<stdio.h>
#include<stdlib.h>
#include "mpc.h"

#ifdef _WIN32
#include<string.h>

static char buffer[1024];

char* readline(char* prompt){
    fputs(prompt, stdout);
    fgets(buffer, sizeof(buffer), stdin);
    size_t length = strlen(buffer);
    if (length > 0 && buffer[length - 1] == '\n') buffer[--length] = '\0';
    char* cpy = (char*)malloc(length);
    if(cpy == NULL){
        fprintf(stderr, "Memory error.\n");
        exit(EXIT_FAILURE);
    } 
    strcpy(cpy, buffer);
    return cpy;
}

void add_history(char* unused) {}

#else
#include <readline/readline.h>
#include <readline/history.h>

#endif
enum {LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR}; 
enum {LERR_ZERO_DIV, LERR_BAD_OP, LERR_BAD_NUM};

typedef struct lval {
    int type;
    long num;
    char* err;
    char* sym;
    int count; 
    struct lval** cell;
} lval;

//constructor de un numero
lval* lval_num(long x){
    lval* v = (lval*)malloc(sizeof (lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

//constructor de un error
lval* lval_err(char *m){
    lval* v = (lval*)malloc(sizeof (lval));
    v->type = LVAL_ERR;
    v->err = (char*)malloc(strlen(m) + 1);
    strcpy(v->err, m);
    return v;
}

//constructor de un simbolo
lval* lval_sym(char* s){
    lval* v = (lval*)malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = (char*)malloc(strlen(s)+1);
    strcpy(v->sym, s);
    return v;
}

//constructor de un S-EXPRESION vacía

lval* sexpr(void){
    lval* v = (lval*)malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

lval* qexpr(void){
    lval* v = (lval*)malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

void lval_print(lval* v);
lval* lval_eval_sexpr(lval* v);
lval* lval_pop(lval* v, int i);
lval* lval_take(lval* v, int i);
lval* builtin_op(lval* v, char* op);

void lval_delete(lval *v){
    switch (v->type)
    {
    case LVAL_NUM: break;
    case LVAL_ERR: free(v->err); break;
    case LVAL_SYM: free(v->sym); break;
    case LVAL_QEXPR:
    case LVAL_SEXPR: 
        for(int i = 0; i < v->count; i++){
            lval_delete(v->cell[i]);
        }
        free(v->cell);
        break;
    }
    free(v);
}

lval* lval_read_num(mpc_ast_t* t){
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}


//esta funcion reasigna memoria a la lista
lval* lval_add(lval* v, lval* x){
    v->count++;
    v->cell = (lval**)realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count-1] = x;
    return v;
}


lval* lval_read(mpc_ast_t* t){
    //si es simbolo o numero, retorna la conversión a ese tipo
    if(strstr(t->tag, "number")){ return lval_read_num(t); }
    if(strstr(t->tag, "symbol")){ return lval_sym(t->contents); }

    //si el nodo es la raiz o una sexpr se crea una sexpr vacia
    lval* x = NULL;
    if(strcmp(t->tag, ">") == 0){ x = qexpr(); }
    if(strstr(t->tag, "sexpr")){ x = sexpr(); }
    if(strstr(t->tag, "qexpr")){x = qexpr();}
    
    //se rellena con cualquier expresion valida que contenga

    for(int i = 0; i < t->children_num; i++){
        if(strcmp(t->children[i]->contents, "(") == 0){continue;}
        if(strcmp(t->children[i]->contents, ")") == 0){continue;}
        if(strcmp(t->children[i]->contents, "{") == 0){continue;}
        if(strcmp(t->children[i]->contents, "}") == 0){continue;}
        if(strcmp(t->children[i]->tag, "regex") == 0){continue;}
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}


void lval_print_expr(lval* v, char open, char close){
    putchar(open);

    for(int i = 0; i < v->count; i++){
        lval_print(v->cell[i]);
        if(i != v->count - 1){
            putchar(' ');
        }
    }
    putchar(close);
}

void lval_print(lval* v){
    switch(v->type){
        case LVAL_NUM: printf("%li", v->num);
        break;
        case LVAL_ERR: printf("Error: %s", v->err);
        break;
        case LVAL_SYM: printf("%s", v->sym);
        break;
        case LVAL_SEXPR:
        
        lval_print_expr(v, '(', ')');
        puts("Esto es una S-Expr!!!!");
        break;
        case LVAL_QEXPR: 
        lval_print_expr(v, '{', '}');
        
        puts("esto es una Q-Expr!!!!!");
        break;
    }
}

void errorASCII(){
                    
                    printf(" *TSSSS* - your program swallowed a spark!!\n");
                    printf(" ________________________________________\n");
                    printf("        \\   ^__^\n");
                    printf("         \\  (xx)'\'_______\n");
                    printf("            (__)\\        )\\/\\ \n");
                    printf("                ||----w |\n");
                    printf("                ||     || \n");
}


void lval_println(lval* v){
    lval_print(v);
    putchar('\n');
}

lval* lval_eval(lval* v){
    if(v->type == LVAL_SEXPR){
        return lval_eval_sexpr(v);
    }
    return v;
}


lval* lval_eval_sexpr(lval* v){

    //evaluando los hijos
    for(int i = 0; i < v->count; i++){
        v->cell[i] = lval_eval(v->cell[i]);
    }

    //checkeo de errores
    for(int i = 0; i < v->count; i++){
        if(v->cell[i]->type == LVAL_ERR){
            return lval_take(v, i);
        }
    }

    //expresión vacia
    if(v->count == 0){
        return v;
    }

    //una sola expresión
    if(v->count == 1){
        return lval_take(v, 0);
    }

    //asegurandose que el primer elemento es un simbolo
    lval* f = lval_pop(v, 0);
    if(f->type != LVAL_SYM){
        lval_delete(f);
        lval_delete(v);
        return lval_err("S-Expression debe empezar con un simbolo!!!");
    }

    //llama al operador
    lval* result = builtin_op(v, f->sym);
    lval_delete(f);
    return result;
}

lval* lval_pop(lval* v, int i){
    lval* x = v->cell[i];
    memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * v->count-i-1);
    v->count--;
    return x;
}

lval* lval_take(lval* v, int i){
    lval* x = lval_pop(v, i);
    lval_delete(v);
    return x;
}

lval* builtin_op(lval* a, char* op){
    for (int i = 0; i < a->count; i++)
    {
        if(a->cell[i]->type != LVAL_NUM){
            lval_delete(a);
            return lval_err("No se puede operar algo que no es un numero!!!");
        }
    }

    //saca el primer elemento
    lval* x = lval_pop(a, 0);
    //si no hay argumentos o restas, se aplica negacion unaria
    if(strcmp(op, "-") == 0 && a->count == 0){
        x->num = -x->num;
    }

    while(a->count > 0){
        lval* y = lval_pop(a, 0);
        if(strcmp(op, "+") == 0){
            x->num += y->num;
        }
        if(strcmp(op, "-") == 0){
            x->num -= y->num;
        }
        if(strcmp(op, "*") == 0){
            x->num *= y->num;
        }
        if(strcmp(op, "/") == 0){
            if(y->num == 0){
                lval_delete(x);
                lval_delete(y);
                x = lval_err("La división por 0 no está permitida!!!");
                break;
            }
            x->num /= y->num;
        }
        lval_delete(y);
    }
    lval_delete(a);
    return x;
}


int main(int argc, char* argv[]){

    mpc_parser_t* num = mpc_new("number");
    mpc_parser_t* symbol = mpc_new("symbol");
    mpc_parser_t* sexpr = mpc_new("sexpr");
    mpc_parser_t* qexpr = mpc_new("qexpr");
    mpc_parser_t* expr = mpc_new("expr");
    mpc_parser_t* noLisp = mpc_new("noLisp");


    mpca_lang(MPCA_LANG_DEFAULT, 
    "                                                   \
     number : /-?[0-9]+/;                               \
     symbol : '+' | '-' | '*' | '/';                    \
     sexpr    : '(' <expr>* ')' ;                       \
     qexpr  :   '{' <expr>* '}';                        \
     expr   :  <number> | <symbol> | <sexpr> | <qexpr>;          \
     noLisp  : /^/ <expr>* /$/;                         \
    ", num, symbol, expr, qexpr, sexpr, noLisp);


    puts("noLisp version 0.0.1");
    puts("press ctrl+c to exit\n");

    while(1){
        char* input = readline("lisp>> ");
        add_history(input);

        mpc_result_t r;
        if(mpc_parse("<stdin>", input, noLisp, &r)){
            //lval result = eval(r.output);
            lval* x = lval_read(r.output);
            lval_println(x);
            lval_delete(x);
        }else{
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
        free(input);
    }
    mpc_cleanup(6, num, symbol, expr, qexpr, sexpr, noLisp);
    printf("????");
    return 0;
}

