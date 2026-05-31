%{
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "Ast.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int yylex(void);
extern int yylineno;
void yyerror(const char *s);

#ifdef __cplusplus
}
#endif

AstNode* root = nullptr;
%}

%union {
    int     num;
    float   fnum;
    char*   str;
    void*   ptr;
}

%token <str>     IDENT
%token <num>     INT_CONST
%token <fnum>    FLOAT_CONST

%token INT FLOAT VOID CONST IF ELSE WHILE BREAK CONTINUE RETURN
%token PLUS MINUS MUL DIV MOD
%token ASSIGN EQ NEQ LT GT LE GE
%token AND OR NOT
%token SEMICOLON COMMA
%token LPAREN RPAREN LBRACKET RBRACKET LBRACE RBRACE
%token <str> STRING_CONST
%token ERROR

%type <str>  TypeSpec UnaryOp
%type <ptr>  CompUnit CompUnitItemList CompUnitItem
%type <ptr>  Decl ConstDecl VarDecl
%type <ptr>  ConstDef ConstDefList VarDef VarDefList
%type <ptr>  FuncDef FuncFParams FuncFParam FuncFParamList
%type <ptr>  Block BlockItem BlockItemList
%type <ptr>  Stmt
%type <ptr>  Exp Cond LVal PrimaryExp Number UnaryExp FuncRParams FuncRParamList
%type <ptr>  MulExp AddExp RelExp EqExp LAndExp LOrExp
%type <ptr>  ConstExp ConstInitVal ConstInitValList InitVal InitValList
%type <ptr>  DimList

%expect 1

%destructor { free($$); } <str>

%%

CompUnit:
    CompUnitItemList { root = static_cast<AstNode*>($1); $$ = $1; }
    ;

CompUnitItemList:
    CompUnitItem {
        CompUnit* cu = new CompUnit();
        cu->items.push_back(static_cast<AstNode*>($1));
        $$ = cu;
    }
    | CompUnitItemList CompUnitItem {
        CompUnit* cu = static_cast<CompUnit*>($1);
        cu->items.push_back(static_cast<AstNode*>($2));
        $$ = cu;
    }
    ;

CompUnitItem:
    Decl  { $$ = $1; }
    | FuncDef { $$ = $1; }
    ;

Decl:
    ConstDecl { $$ = $1; }
    | VarDecl { $$ = $1; }
    ;

ConstDecl:
    CONST TypeSpec ConstDefList SEMICOLON {
        ConstDecl* cd = static_cast<ConstDecl*>($3);
        cd->btype = std::string($2);
        for (auto def : cd->defs) def->btype = cd->btype;
        free($2);
        $$ = cd;
    }
    ;

VarDecl:
    TypeSpec VarDefList SEMICOLON {
        VarDecl* vd = static_cast<VarDecl*>($2);
        vd->btype = std::string($1);
        for (auto def : vd->defs) def->btype = vd->btype;
        free($1);
        $$ = vd;
    }
    ;

TypeSpec:
    VOID  { $$ = strdup("void"); }
    | INT   { $$ = strdup("int"); }
    | FLOAT { $$ = strdup("float"); }
    ;

ConstDefList:
    ConstDef {
        ConstDecl* cd = new ConstDecl("");
        cd->defs.push_back(static_cast<ConstDef*>($1));
        $$ = cd;
    }
    | ConstDefList COMMA ConstDef {
        ConstDecl* cd = static_cast<ConstDecl*>($1);
        cd->defs.push_back(static_cast<ConstDef*>($3));
        $$ = cd;
    }
    ;

VarDefList:
    VarDef {
        VarDecl* vd = new VarDecl("");
        vd->defs.push_back(static_cast<VarDef*>($1));
        $$ = vd;
    }
    | VarDefList COMMA VarDef {
        VarDecl* vd = static_cast<VarDecl*>($1);
        vd->defs.push_back(static_cast<VarDef*>($3));
        $$ = vd;
    }
    ;

ConstDef:
    IDENT DimList ASSIGN ConstInitVal {
        ConstDef* cd = new ConstDef($1);
        free($1);
        if ($2) {
            std::vector<Expr*>* dims = static_cast<std::vector<Expr*>*>($2);
            for (Expr* e : *dims) cd->dims.push_back(e);
            delete dims;
        }
        cd->initVal = static_cast<InitVal*>($4);
        $$ = cd;
    }
    ;

VarDef:
    IDENT DimList {
        VarDef* vd = new VarDef($1);
        free($1);
        if ($2) {
            std::vector<Expr*>* dims = static_cast<std::vector<Expr*>*>($2);
            for (Expr* e : *dims) vd->dims.push_back(e);
            delete dims;
        }
        $$ = vd;
    }
    | IDENT DimList ASSIGN InitVal {
        VarDef* vd = new VarDef($1);
        free($1);
        if ($2) {
            std::vector<Expr*>* dims = static_cast<std::vector<Expr*>*>($2);
            for (Expr* e : *dims) vd->dims.push_back(e);
            delete dims;
        }
        vd->initVal = static_cast<InitVal*>($4);
        $$ = vd;
    }
    ;

DimList:
    /* empty */ { $$ = nullptr; }
    | DimList LBRACKET ConstExp RBRACKET {
        std::vector<Expr*>* v;
        if ($1) v = static_cast<std::vector<Expr*>*>($1);
        else v = new std::vector<Expr*>();
        v->push_back(static_cast<Expr*>($3));
        $$ = v;
    }
    ;

ConstInitVal:
    ConstExp {
        $$ = new ExprInitVal(static_cast<Expr*>($1));
    }
    | LBRACE RBRACE {
        $$ = new ArrayInitVal();
    }
    | LBRACE ConstInitValList RBRACE {
        $$ = $2;
    }
    ;

ConstInitValList:
    ConstInitVal {
        ArrayInitVal* av = new ArrayInitVal();
        av->vals.push_back(static_cast<InitVal*>($1));
        $$ = av;
    }
    | ConstInitValList COMMA ConstInitVal {
        ArrayInitVal* av = static_cast<ArrayInitVal*>($1);
        av->vals.push_back(static_cast<InitVal*>($3));
        $$ = av;
    }
    ;

InitVal:
    Exp {
        $$ = new ExprInitVal(static_cast<Expr*>($1));
    }
    | LBRACE RBRACE {
        $$ = new ArrayInitVal();
    }
    | LBRACE InitValList RBRACE {
        $$ = $2;
    }
    ;

InitValList:
    InitVal {
        ArrayInitVal* av = new ArrayInitVal();
        av->vals.push_back(static_cast<InitVal*>($1));
        $$ = av;
    }
    | InitValList COMMA InitVal {
        ArrayInitVal* av = static_cast<ArrayInitVal*>($1);
        av->vals.push_back(static_cast<InitVal*>($3));
        $$ = av;
    }
    ;

FuncDef:
    TypeSpec IDENT LPAREN FuncFParams RPAREN Block {
        FuncDef* fd = new FuncDef($1, $2);
        free($1); free($2);
        if ($4) {
            std::vector<FuncFParam*>* params = static_cast<std::vector<FuncFParam*>*>($4);
            fd->params = *params;
            delete params;
        }
        fd->body = static_cast<Block*>($6);
        $$ = fd;
    }
    ;

FuncFParams:
    FuncFParamList { $$ = $1; }
    | /* empty */ { $$ = nullptr; }
    ;

FuncFParamList:
    FuncFParam {
        std::vector<FuncFParam*>* v = new std::vector<FuncFParam*>();
        v->push_back(static_cast<FuncFParam*>($1));
        $$ = v;
    }
    | FuncFParamList COMMA FuncFParam {
        std::vector<FuncFParam*>* v = static_cast<std::vector<FuncFParam*>*>($1);
        v->push_back(static_cast<FuncFParam*>($3));
        $$ = v;
    }
    ;

FuncFParam:
    TypeSpec IDENT {
        $$ = new FuncFParam($1, $2);
        free($1); free($2);
    }
    | TypeSpec IDENT LBRACKET RBRACKET DimList {
        FuncFParam* fp = new FuncFParam($1, $2);
        free($1); free($2);
        fp->dims.push_back(nullptr);
        if ($5) {
            std::vector<Expr*>* dims = static_cast<std::vector<Expr*>*>($5);
            for (Expr* e : *dims) fp->dims.push_back(e);
            delete dims;
        }
        $$ = fp;
    }
    ;

Block:
    LBRACE RBRACE {
        $$ = new Block();
    }
    | LBRACE BlockItemList RBRACE {
        $$ = $2;
    }
    ;

BlockItemList:
    BlockItem {
        Block* b = new Block();
        b->items.push_back(static_cast<BlockItem*>($1));
        $$ = b;
    }
    | BlockItemList BlockItem {
        Block* b = static_cast<Block*>($1);
        b->items.push_back(static_cast<BlockItem*>($2));
        $$ = b;
    }
    ;

BlockItem:
    Decl {
        $$ = new DeclItem(static_cast<Decl*>($1));
    }
    | Stmt {
        $$ = new StmtItem(static_cast<Stmt*>($1));
    }
    ;

Stmt:
    LVal ASSIGN Exp SEMICOLON {
        $$ = new AssignStmt(static_cast<LVal*>($1), static_cast<Expr*>($3));
    }
    | Exp SEMICOLON {
        $$ = new ExpStmt(static_cast<Expr*>($1));
    }
    | SEMICOLON {
        $$ = new ExpStmt();
    }
    | Block {
        $$ = $1;
    }
    | IF LPAREN Cond RPAREN Stmt {
        $$ = new IfStmt(static_cast<Expr*>($3), static_cast<Stmt*>($5));
    }
    | IF LPAREN Cond RPAREN Stmt ELSE Stmt {
        $$ = new IfStmt(static_cast<Expr*>($3), static_cast<Stmt*>($5), static_cast<Stmt*>($7));
    }
    | WHILE LPAREN Cond RPAREN Stmt {
        $$ = new WhileStmt(static_cast<Expr*>($3), static_cast<Stmt*>($5));
    }
    | BREAK SEMICOLON {
        $$ = new BreakStmt();
    }
    | CONTINUE SEMICOLON {
        $$ = new ContinueStmt();
    }
    | RETURN SEMICOLON {
        $$ = new ReturnStmt();
    }
    | RETURN Exp SEMICOLON {
        $$ = new ReturnStmt(static_cast<Expr*>($2));
    }
    ;

Exp:
    AddExp { $$ = $1; }
    ;

Cond:
    LOrExp { $$ = $1; }
    ;

LVal:
    IDENT {
        $$ = new LVal($1);
        free($1);
    }
    | LVal LBRACKET Exp RBRACKET {
        LVal* lv = static_cast<LVal*>($1);
        lv->dims.push_back(static_cast<Expr*>($3));
        $$ = lv;
    }
    ;

PrimaryExp:
    LPAREN Exp RPAREN { $$ = $2; }
    | LVal { $$ = $1; }
    | Number { $$ = $1; }
    | STRING_CONST {
        $$ = new StringLiteral(std::string($1));
        free($1);
    }
    ;

Number:
    INT_CONST   { $$ = new Number($1); }
    | FLOAT_CONST { $$ = new Number($1); }
    ;

UnaryExp:
    PrimaryExp { $$ = $1; }
    | IDENT LPAREN FuncRParams RPAREN {
        CallExp* ce = new CallExp($1);
        free($1);
        if ($3) {
            std::vector<Expr*>* args = static_cast<std::vector<Expr*>*>($3);
            ce->args = *args;
            delete args;
        }
        $$ = ce;
    }
    | UnaryOp UnaryExp {
        $$ = new UnaryExp(std::string($1), static_cast<Expr*>($2));
        free($1);
    }
    ;

UnaryOp:
    PLUS  { $$ = strdup("+"); }
    | MINUS { $$ = strdup("-"); }
    | NOT   { $$ = strdup("!"); }
    ;

FuncRParams:
    FuncRParamList { $$ = $1; }
    | /* empty */ { $$ = nullptr; }
    ;

FuncRParamList:
    Exp {
        std::vector<Expr*>* v = new std::vector<Expr*>();
        v->push_back(static_cast<Expr*>($1));
        $$ = v;
    }
    | FuncRParamList COMMA Exp {
        std::vector<Expr*>* v = static_cast<std::vector<Expr*>*>($1);
        v->push_back(static_cast<Expr*>($3));
        $$ = v;
    }
    ;

MulExp:
    UnaryExp { $$ = $1; }
    | MulExp MUL UnaryExp { $$ = new BinaryExp("*", static_cast<Expr*>($1), static_cast<Expr*>($3)); }
    | MulExp DIV UnaryExp { $$ = new BinaryExp("/", static_cast<Expr*>($1), static_cast<Expr*>($3)); }
    | MulExp MOD UnaryExp { $$ = new BinaryExp("%", static_cast<Expr*>($1), static_cast<Expr*>($3)); }
    ;

AddExp:
    MulExp { $$ = $1; }
    | AddExp PLUS MulExp { $$ = new BinaryExp("+", static_cast<Expr*>($1), static_cast<Expr*>($3)); }
    | AddExp MINUS MulExp { $$ = new BinaryExp("-", static_cast<Expr*>($1), static_cast<Expr*>($3)); }
    ;

RelExp:
    AddExp { $$ = $1; }
    | RelExp LT AddExp { $$ = new BinaryExp("<", static_cast<Expr*>($1), static_cast<Expr*>($3)); }
    | RelExp GT AddExp { $$ = new BinaryExp(">", static_cast<Expr*>($1), static_cast<Expr*>($3)); }
    | RelExp LE AddExp { $$ = new BinaryExp("<=", static_cast<Expr*>($1), static_cast<Expr*>($3)); }
    | RelExp GE AddExp { $$ = new BinaryExp(">=", static_cast<Expr*>($1), static_cast<Expr*>($3)); }
    ;

EqExp:
    RelExp { $$ = $1; }
    | EqExp EQ RelExp { $$ = new BinaryExp("==", static_cast<Expr*>($1), static_cast<Expr*>($3)); }
    | EqExp NEQ RelExp { $$ = new BinaryExp("!=", static_cast<Expr*>($1), static_cast<Expr*>($3)); }
    ;

LAndExp:
    EqExp { $$ = $1; }
    | LAndExp AND EqExp { $$ = new BinaryExp("&&", static_cast<Expr*>($1), static_cast<Expr*>($3)); }
    ;

LOrExp:
    LAndExp { $$ = $1; }
    | LOrExp OR LAndExp { $$ = new BinaryExp("||", static_cast<Expr*>($1), static_cast<Expr*>($3)); }
    ;

ConstExp:
    AddExp { $$ = $1; }
    ;

%%

void yyerror(const char *s) {
    std::cerr << "Error at line " << yylineno << ": " << s << std::endl;
}
