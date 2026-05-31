#ifndef SEMANTIC_ANALYZER_H
#define SEMANTIC_ANALYZER_H
/* ============================================================================
 * 文件名称: SemanticAnalyzer.h
 * 功能描述: 语义分析器类声明，负责类型检查、符号表构建和语义错误检测
 * 作者信息: SysY2022 Compiler Team
 * 版本信息: 1.1  (支持多维常量数组编译期求值)
 * 所属项目: SysY2022 编译器
 * ============================================================================ */


#include <iostream>
#include <string>
#include <vector>
#include <map>
#include "Ast.h"
#include "Type.h"
#include "SymbolTable.h"

/* 常量值存储结构：支持标量常量和数组常量的编译期值（含 int/float） */
struct ConstValue {
    bool isScalar;
    bool isFloat;           // 新增：标记是否为 float 常量
    int scalarVal;
    float floatVal;         // 新增：float 标量值
    std::vector<int> arrayDims;
    std::vector<int> arrayValues;
    ConstValue() : isScalar(true), isFloat(false), scalarVal(0), floatVal(0.0f) {}
};

/* 语义分析器：遍历AST进行类型检查、作用域管理和常量求值 */
class SemanticAnalyzer : public Visitor {
public:

public:
    SymbolTable symtab;
    bool hasError = false;
    int loopDepth = 0;
    std::string currentFuncRetType;
    bool foundMain = false;
    std::map<std::string, ConstValue> constValues;

    Type* makeType(const std::string& btype, const std::vector<Expr*>& dims);

    void declareSysYLib();

    void report(const std::string& msg);

    int evalConstExpr(Expr* e, bool& ok);

    bool isConstExpr(Expr* e);

    bool checkInitType(InitVal* init, Type* baseType, const std::string& arrName);

    void checkArrayDims(const std::string& name, const std::vector<Expr*>& dims);

    bool flattenConstArrayInit(InitVal* init, const std::vector<int>& dims,
                               std::vector<int>& out, int depth = 0);

    bool hasReturnOnAllPaths(Stmt* stmt);

    void visit(CompUnit& node) override;

    void visit(ConstDecl& node) override;

    void visit(VarDecl& node) override;

    void visit(FuncDef& node) override;

    void visit(Block& node) override;

    void visit(AssignStmt& node) override;

    void visit(LVal& node) override;

    void visit(CallExp& node) override;

    void visit(IfStmt& node) override;

    void visit(WhileStmt& node) override;

    void visit(BreakStmt& node) override;

    void visit(ContinueStmt& node) override;

    void visit(ReturnStmt& node) override;

    void visit(BinaryExp& node) override;
    void visit(UnaryExp& node) override;
    void visit(Number& node) override;
    void visit(ExprInitVal& node) override;
    void visit(ArrayInitVal& node) override;
    void visit(ConstDef& node) override;
    void visit(VarDef& node) override;
    void visit(FuncFParam& node) override;
    void visit(DeclItem& node) override;
    void visit(StmtItem& node) override;
    void visit(ExpStmt& node) override;
    void visit(StringLiteral& node) override;

};

#endif 
