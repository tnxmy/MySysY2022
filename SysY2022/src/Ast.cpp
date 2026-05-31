/* ============================================================================
 * 文件名称: Ast.cpp
 * 功能描述: AST节点accept方法的实现，将访问者模式分发到具体节点
 * 作者信息: SysY2022 Compiler Team
 * 版本信息: 1.0
 * 所属项目: SysY2022 编译器
 * ============================================================================ */

#include "Ast.h"

using namespace std;

/* CompUnit 节点接受访问者，调用对应的visit方法 */
void CompUnit::accept(Visitor& v)      { v.visit(*this); }
/* ConstDecl 节点接受访问者，调用对应的visit方法 */
void ConstDecl::accept(Visitor& v)     { v.visit(*this); }
/* VarDecl 节点接受访问者，调用对应的visit方法 */
void VarDecl::accept(Visitor& v)       { v.visit(*this); }
/* ConstDef 节点接受访问者，调用对应的visit方法 */
void ConstDef::accept(Visitor& v)     { v.visit(*this); }
/* VarDef 节点接受访问者，调用对应的visit方法 */
void VarDef::accept(Visitor& v)        { v.visit(*this); }
/* FuncDef 节点接受访问者，调用对应的visit方法 */
void FuncDef::accept(Visitor& v)      { v.visit(*this); }
/* FuncFParam 节点接受访问者，调用对应的visit方法 */
void FuncFParam::accept(Visitor& v)   { v.visit(*this); }
/* Block 节点接受访问者，调用对应的visit方法 */
void Block::accept(Visitor& v)        { v.visit(*this); }
/* DeclItem 节点接受访问者，调用对应的visit方法 */
void DeclItem::accept(Visitor& v)     { v.visit(*this); }
/* StmtItem 节点接受访问者，调用对应的visit方法 */
void StmtItem::accept(Visitor& v)     { v.visit(*this); }
/* AssignStmt 节点接受访问者，调用对应的visit方法 */
void AssignStmt::accept(Visitor& v)   { v.visit(*this); }
/* ExpStmt 节点接受访问者，调用对应的visit方法 */
void ExpStmt::accept(Visitor& v)      { v.visit(*this); }
/* IfStmt 节点接受访问者，调用对应的visit方法 */
void IfStmt::accept(Visitor& v)       { v.visit(*this); }
/* WhileStmt 节点接受访问者，调用对应的visit方法 */
void WhileStmt::accept(Visitor& v)    { v.visit(*this); }
/* BreakStmt 节点接受访问者，调用对应的visit方法 */
void BreakStmt::accept(Visitor& v)    { v.visit(*this); }
/* ContinueStmt 节点接受访问者，调用对应的visit方法 */
void ContinueStmt::accept(Visitor& v)  { v.visit(*this); }
/* ReturnStmt 节点接受访问者，调用对应的visit方法 */
void ReturnStmt::accept(Visitor& v)   { v.visit(*this); }
/* LVal 节点接受访问者，调用对应的visit方法 */
void LVal::accept(Visitor& v)         { v.visit(*this); }
/* Number 节点接受访问者，调用对应的visit方法 */
void Number::accept(Visitor& v)       { v.visit(*this); }
/* BinaryExp 节点接受访问者，调用对应的visit方法 */
void BinaryExp::accept(Visitor& v)    { v.visit(*this); }
/* UnaryExp 节点接受访问者，调用对应的visit方法 */
void UnaryExp::accept(Visitor& v)    { v.visit(*this); }
/* CallExp 节点接受访问者，调用对应的visit方法 */
void CallExp::accept(Visitor& v)     { v.visit(*this); }
/* ExprInitVal 节点接受访问者，调用对应的visit方法 */
void ExprInitVal::accept(Visitor& v)  { v.visit(*this); }
/* ArrayInitVal 节点接受访问者，调用对应的visit方法 */
void ArrayInitVal::accept(Visitor& v) { v.visit(*this); }
/* StringLiteral 节点接受访问者，调用对应的visit方法 */
void StringLiteral::accept(Visitor& v) { v.visit(*this); }

void DumpVisitor::print(const string& s) {
    for (int i = 0; i < indent; ++i) cout << "  ";
    cout << s << endl;
}

void DumpVisitor::visit(CompUnit& node) {
    print("CompUnit");
    indent++;
    for (auto item : node.items) item->accept(*this);
    indent--;
}

void DumpVisitor::visit(ConstDecl& node) {
    print("ConstDecl [" + node.btype + "]");
    indent++;
    for (auto d : node.defs) d->accept(*this);
    indent--;
}

void DumpVisitor::visit(VarDecl& node) {
    print("VarDecl [" + node.btype + "]");
    indent++;
    for (auto d : node.defs) d->accept(*this);
    indent--;
}

void DumpVisitor::visit(ConstDef& node) {
    string s = "ConstDef [" + node.name + "]";
    if (!node.dims.empty()) s += " array";
    print(s);
    indent++;
    if (node.initVal) node.initVal->accept(*this);
    indent--;
}

void DumpVisitor::visit(VarDef& node) {
    string s = "VarDef [" + node.name + "]";
    if (!node.dims.empty()) s += " array";
    print(s);
    indent++;
    if (node.initVal) node.initVal->accept(*this);
    indent--;
}

void DumpVisitor::visit(FuncDef& node) {
    print("FuncDef [" + node.retType + " " + node.name + "]");
    indent++;
    for (auto p : node.params) p->accept(*this);
    if (node.body) node.body->accept(*this);
    indent--;
}

void DumpVisitor::visit(FuncFParam& node) {
    string s = "FuncFParam [" + node.btype + " " + node.name + "]";
    if (!node.dims.empty()) s += " array";
    print(s);
}

void DumpVisitor::visit(Block& node) {
    print("Block");
    indent++;
    for (auto item : node.items) item->accept(*this);
    indent--;
}

void DumpVisitor::visit(DeclItem& node) {
    if (node.decl) node.decl->accept(*this);
}

void DumpVisitor::visit(StmtItem& node) {
    if (node.stmt) node.stmt->accept(*this);
}

void DumpVisitor::visit(AssignStmt& node) {
    print("AssignStmt");
    indent++;
    if (node.lval) node.lval->accept(*this);
    if (node.expr) node.expr->accept(*this);
    indent--;
}

void DumpVisitor::visit(ExpStmt& node) {
    print("ExpStmt");
    indent++;
    if (node.expr) node.expr->accept(*this);
    indent--;
}

void DumpVisitor::visit(IfStmt& node) {
    print("IfStmt");
    indent++;
    if (node.cond) node.cond->accept(*this);
    if (node.thenStmt) node.thenStmt->accept(*this);
    if (node.elseStmt) node.elseStmt->accept(*this);
    indent--;
}

void DumpVisitor::visit(WhileStmt& node) {
    print("WhileStmt");
    indent++;
    if (node.cond) node.cond->accept(*this);
    if (node.body) node.body->accept(*this);
    indent--;
}

void DumpVisitor::visit(BreakStmt& node) {
    print("BreakStmt");
}

void DumpVisitor::visit(ContinueStmt& node) {
    print("ContinueStmt");
}

void DumpVisitor::visit(ReturnStmt& node) {
    print("ReturnStmt");
    indent++;
    if (node.expr) node.expr->accept(*this);
    indent--;
}

void DumpVisitor::visit(LVal& node) {
    string s = "LVal [" + node.name + "]";
    if (!node.dims.empty()) s += " dims=" + to_string(node.dims.size());
    print(s);
    indent++;
    for (auto d : node.dims) if (d) d->accept(*this);
    indent--;
}

void DumpVisitor::visit(Number& node) {
    if (node.isFloat) print("Number [float: " + to_string(node.fVal) + "]");
    else              print("Number [int: " + to_string(node.iVal) + "]");
}

void DumpVisitor::visit(BinaryExp& node) {
    print("BinaryExp [" + node.op + "]");
    indent++;
    if (node.lhs) node.lhs->accept(*this);
    if (node.rhs) node.rhs->accept(*this);
    indent--;
}

void DumpVisitor::visit(UnaryExp& node) {
    print("UnaryExp [" + node.op + "]");
    indent++;
    if (node.operand) node.operand->accept(*this);
    indent--;
}

void DumpVisitor::visit(CallExp& node) {
    print("CallExp [" + node.funcName + "]");
    indent++;
    for (auto a : node.args) if (a) a->accept(*this);
    indent--;
}

void DumpVisitor::visit(ExprInitVal& node) {
    print("ExprInitVal");
    indent++;
    if (node.expr) node.expr->accept(*this);
    indent--;
}

void DumpVisitor::visit(ArrayInitVal& node) {
    print("ArrayInitVal [" + to_string(node.vals.size()) + " items]");
    indent++;
    for (auto v : node.vals) if (v) v->accept(*this);
    indent--;
}

void DumpVisitor::visit(StringLiteral& node) {
    print("StringLiteral [\"" + node.value + "\"]");
}