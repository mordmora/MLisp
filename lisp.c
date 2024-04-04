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
enum {LVAL_NUM, LVAL_ERR}; //posibles lval types
enum {LERR_ZERO_DIV, LERR_BAD_OP, LERR_BAD_NUM};

typedef struct lval {
    int type;
    long num;
    int err;
} lval;

lval lval_num(long x){
    lval v;
    v.type = LVAL_NUM;
    v.num = x;
    return v;
}

lval lval_err(int x){
    lval v;
    v.type = LVAL_ERR;
    v.err = x;
    return v;
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

void lval_print_err(lval v){
    switch (v.type)
    {
    case LVAL_NUM:
        printf("%li", v.num);
        break;
    case LVAL_ERR:
        if(v.err == LERR_BAD_NUM){
            errorASCII();
            printf("ERROR: --bad-number--\nYou may have entered a very long number.");
        }else if(v.err == LERR_BAD_OP){
            errorASCII();
            printf("ERROR: --bad-op--");
            
        }else if(v.err == LERR_ZERO_DIV){
            errorASCII();
            printf("ERRO: --zero-div--\nZero div is not possible.");
            

        }
        break;

    default:
        break;
    }
}

lval lval_println(lval v){
    lval_print_err(v);
    putchar('\n');
    return v;
}



//arbol abstracto 

lval eval_op(lval x, char* op, lval y) {

  if (x.type == LVAL_ERR){ return x; }
  if (y.type == LVAL_ERR){ return y; }

  if (strcmp(op, "+") == 0) { return lval_num(x.num + y.num); }
  if (strcmp(op, "-") == 0) { return lval_num(x.num - y.num); }
  if (strcmp(op, "*") == 0) { return lval_num(x.num * y.num); }
  if (strcmp(op, "/") == 0) {
     return y.num == 0 ? lval_err(LERR_ZERO_DIV) : lval_num(x.num / y.num); }
  return lval_err(LERR_BAD_OP);
}

lval eval (mpc_ast_t* t){
    if(strstr(t->tag, "number")){
        errno = 0;
        long x = strtol(t->contents, NULL, 10);
        return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_OP);
    }

    char* operator = t->children[1]->contents;

    lval x = eval(t->children[2]);

    int i = 3;
    while(strstr(t->children[i]->tag, "expr")){
        x = eval_op(x, operator, eval(t->children[i]));
        i++;
    }
    return x;
}


int main(int argc, char* argv[]){

    mpc_parser_t* num = mpc_new("number");
    mpc_parser_t* operator = mpc_new("operator");
    mpc_parser_t* expr = mpc_new("expr");
    mpc_parser_t* noLisp = mpc_new("noLisp");

    mpca_lang(MPCA_LANG_DEFAULT, 
    "                                                    \
     number : /-?[0-9]+/;                                \
     operator : '+' | '-' | '*' | '/';                   \
     expr    : <number> |  '(' <operator> <expr>+ ')';   \
     noLisp  : /^/ <operator> <expr>+ /$/;               \
    ", num, operator, expr, noLisp);


    puts("noLisp version 0.0.1");
    puts("press ctrl+c to exit\n");

    while(1){
        char* input = readline("lisp>> ");
        add_history(input);

        mpc_result_t r;
        if(mpc_parse("<stdin>", input, noLisp, &r)){
            lval result = eval(r.output);
            lval_println(result);
            mpc_ast_delete(r.output);
        }else{
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
        free(input);
    }
    mpc_cleanup(4, num, operator, expr, noLisp);
    return 0;
}

