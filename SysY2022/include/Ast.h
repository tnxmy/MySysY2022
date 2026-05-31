#ifndef AST_H
#define AST_H
/* ============================================================================
 * 文件名称: Ast.h
 * 功能描述: 抽象语法树(AST)节点定义，包含所有表达式、语句和声明的类结构
 * 作者信息: SysY2022 Compiler Team
 * 版本信息: 1.0
 * 所属项目: SysY2022 编译器
 * ============================================================================ */


#include <string>
#include <vector>
#include <iostream>

/* 访问者模式基类，定义所有 AST 节点的访问接口 */
struct Visitor;

/* AST 抽象基类，所有语法树节点均继承自此类 */
struct AstNode {
    virtual ~AstNode() = default;
    virtual void accept(Visitor& visitor) = 0;
};

/* 表达式基类，所有表达式节点继承此类 */
struct Expr : AstNode {};

/* 左值表达式节点，表示变量或数组元素引用 */
struct LVal : Expr {
    std::string name;
    std::vector<Expr*> dims;
    LVal(const std::string& n) : name(n) {}
    void accept(Visitor& visitor) override;
};

/* 数字字面量节点，支持整数和浮点数 */
struct Number : Expr {
    bool isFloat;
    int iVal;
    float fVal;
    Number(int v) : isFloat(false), iVal(v), fVal(0.0f) {}
    Number(float v) : isFloat(true), iVal(0), fVal(v) {}
    void accept(Visitor& visitor) override;
};

/* 字符串字面量节点，用于格式化输出 */
struct StringLiteral : Expr {
    std::string value;
    StringLiteral(const std::string& v) : value(v) {}
    void accept(Visitor& visitor) override;
};

/* 二元表达式节点，包含操作符、左操作数、右操作数 */
struct BinaryExp : Expr {
    std::string op;
    Expr* lhs;
    Expr* rhs;
    BinaryExp(const std::string& o, Expr* l, Expr* r) : op(o), lhs(l), rhs(r) {}
    void accept(Visitor& visitor) override;
};

/* 一元表达式节点，包含操作符和操作数 */
struct UnaryExp : Expr {
    std::string op;
    Expr* operand;
    UnaryExp(const std::string& o, Expr* e) : op(o), operand(e) {}
    void accept(Visitor& visitor) override;
};

/* 函数调用表达式节点，包含函数名和实参列表 */
struct CallExp : Expr {
    std::string funcName;
    std::vector<Expr*> args;
    CallExp(const std::string& n) : funcName(n) {}
    void accept(Visitor& visitor) override;
};

/* 初始化值基类，用于数组和变量初始化 */
struct InitVal : AstNode {};

/* 表达式初始化值节点，用于标量初始化 */
struct ExprInitVal : InitVal {
    Expr* expr;
    ExprInitVal(Expr* e) : expr(e) {}
    void accept(Visitor& visitor) override;
};

/* 数组初始化值节点，包含子初始化值列表 */
struct ArrayInitVal : InitVal {
    std::vector<InitVal*> vals;
    void accept(Visitor& visitor) override;
};

struct Def : AstNode {};

struct ConstDef : Def {
    std::string name;
    std::string btype;
    std::vector<Expr*> dims;
    InitVal* initVal;
    ConstDef(const std::string& n) : name(n), initVal(nullptr) {}
    void accept(Visitor& visitor) override;
};

struct VarDef : Def {
    std::string name;
    std::string btype;
    std::vector<Expr*> dims;
    InitVal* initVal;
    VarDef(const std::string& n) : name(n), initVal(nullptr) {}
    void accept(Visitor& visitor) override;
};

/* 声明基类，所有声明节点继承此类 */
struct Decl : AstNode {};

/* 常量声明节点，包含基类型和常量定义列表 */
struct ConstDecl : Decl {
    std::string btype;
    std::vector<ConstDef*> defs;
    ConstDecl(const std::string& t) : btype(t) {}
    void accept(Visitor& visitor) override;
};

/* 变量声明节点，包含基类型和变量定义列表 */
struct VarDecl : Decl {
    std::string btype;
    std::vector<VarDef*> defs;
    VarDecl(const std::string& t) : btype(t) {}
    void accept(Visitor& visitor) override;
};

/* 函数形参节点，包含类型、名称、数组维度 */
struct FuncFParam : AstNode {
    std::string btype;
    std::string name;
    std::vector<Expr*> dims;
    FuncFParam(const std::string& t, const std::string& n) : btype(t), name(n) {}
    void accept(Visitor& visitor) override;
};

struct Block;

/* 函数定义节点，包含返回类型、名称、参数、函数体 */
struct FuncDef : AstNode {
    std::string retType;
    std::string name;
    std::vector<FuncFParam*> params;
    Block* body;
    FuncDef(const std::string& t, const std::string& n) : retType(t), name(n), body(nullptr) {}
    void accept(Visitor& visitor) override;
};

/* 语句基类，所有语句节点继承此类 */
struct Stmt : AstNode {};

struct BlockItem : AstNode {};

struct DeclItem : BlockItem {
    Decl* decl;
    DeclItem(Decl* d) : decl(d) {}
    void accept(Visitor& visitor) override;
};

struct StmtItem : BlockItem {
    Stmt* stmt;
    StmtItem(Stmt* s) : stmt(s) {}
    void accept(Visitor& visitor) override;
};

/* 语句块节点，包含声明和语句列表 */
struct Block : Stmt {
    std::vector<BlockItem*> items;
    void accept(Visitor& visitor) override;
};

/* 赋值语句节点，包含左值和表达式 */
struct AssignStmt : Stmt {
    LVal* lval;
    Expr* expr;
    AssignStmt(LVal* l, Expr* e) : lval(l), expr(e) {}
    void accept(Visitor& visitor) override;
};

/* 表达式语句节点，表达式可为空 */
struct ExpStmt : Stmt {
    Expr* expr;
    ExpStmt(Expr* e = nullptr) : expr(e) {}
    void accept(Visitor& visitor) override;
};

/* 条件语句节点，包含条件、then分支、else分支 */
struct IfStmt : Stmt {
    Expr* cond;
    Stmt* thenStmt;
    Stmt* elseStmt;
    IfStmt(Expr* c, Stmt* t, Stmt* e = nullptr) : cond(c), thenStmt(t), elseStmt(e) {}
    void accept(Visitor& visitor) override;
};

/* while循环语句节点，包含条件和循环体 */
struct WhileStmt : Stmt {
    Expr* cond;
    Stmt* body;
    WhileStmt(Expr* c, Stmt* b) : cond(c), body(b) {}
    void accept(Visitor& visitor) override;
};

/* break语句节点，用于跳出循环 */
struct BreakStmt : Stmt {
    void accept(Visitor& visitor) override;
};

/* continue语句节点，用于跳过当前循环迭代 */
struct ContinueStmt : Stmt {
    void accept(Visitor& visitor) override;
};

/* return语句节点，包含可选的返回值表达式 */
struct ReturnStmt : Stmt {
    Expr* expr;
    ReturnStmt(Expr* e = nullptr) : expr(e) {}
    void accept(Visitor& visitor) override;
};

/* 编译单元(根节点)，包含全局声明和函数定义列表 */
struct CompUnit : AstNode {
    std::vector<AstNode*> items;
    void accept(Visitor& visitor) override;
};

struct Visitor {
    virtual ~Visitor() = default;
    virtual void visit(CompUnit& node) = 0;
    virtual void visit(ConstDecl& node) = 0;
    virtual void visit(VarDecl& node) = 0;
    virtual void visit(ConstDef& node) = 0;
    virtual void visit(VarDef& node) = 0;
    virtual void visit(FuncDef& node) = 0;
    virtual void visit(FuncFParam& node) = 0;
    virtual void visit(Block& node) = 0;
    virtual void visit(DeclItem& node) = 0;
    virtual void visit(StmtItem& node) = 0;
    virtual void visit(AssignStmt& node) = 0;
    virtual void visit(ExpStmt& node) = 0;
    virtual void visit(IfStmt& node) = 0;
    virtual void visit(WhileStmt& node) = 0;
    virtual void visit(BreakStmt& node) = 0;
    virtual void visit(ContinueStmt& node) = 0;
    virtual void visit(ReturnStmt& node) = 0;
    virtual void visit(LVal& node) = 0;
    virtual void visit(Number& node) = 0;
    virtual void visit(BinaryExp& node) = 0;
    virtual void visit(UnaryExp& node) = 0;
    virtual void visit(CallExp& node) = 0;
    virtual void visit(ExprInitVal& node) = 0;
    virtual void visit(ArrayInitVal& node) = 0;
    virtual void visit(StringLiteral& node) = 0;
};

/* AST 打印访问者，用于调试输出语法树结构 */
struct DumpVisitor : Visitor {
    int indent = 0;
    void print(const std::string& s);
    void visit(CompUnit& node) override;
    void visit(ConstDecl& node) override;
    void visit(VarDecl& node) override;
    void visit(ConstDef& node) override;
    void visit(VarDef& node) override;
    void visit(FuncDef& node) override;
    void visit(FuncFParam& node) override;
    void visit(Block& node) override;
    void visit(DeclItem& node) override;
    void visit(StmtItem& node) override;
    void visit(AssignStmt& node) override;
    void visit(ExpStmt& node) override;
    void visit(IfStmt& node) override;
    void visit(WhileStmt& node) override;
    void visit(BreakStmt& node) override;
    void visit(ContinueStmt& node) override;
    void visit(ReturnStmt& node) override;
    void visit(LVal& node) override;
    void visit(Number& node) override;
    void visit(BinaryExp& node) override;
    void visit(UnaryExp& node) override;
    void visit(CallExp& node) override;
    void visit(ExprInitVal& node) override;
    void visit(ArrayInitVal& node) override;
    void visit(StringLiteral& node) override;
};

#endif 
