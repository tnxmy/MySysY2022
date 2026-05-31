/* ============================================================================
 * 文件名称: SemanticAnalyzer.cpp
 * 功能描述: 语义分析器实现，负责类型检查、常量求值和符号表构建
 * 作者信息: SysY2022 Compiler Team
 * 版本信息: 1.0
 * 所属项目: SysY2022 编译器
 * ============================================================================ */

#include "SemanticAnalyzer.h"

/* 根据类型名和维度列表构造SysY类型对象，数组维度必须为常量 */
Type* SemanticAnalyzer::makeType(const std::string& btype, const std::vector<Expr*>& dims) {
        Type* base = nullptr;
        if (btype == "int") base = getIntType();
        else if (btype == "float") base = getFloatType();
        else if (btype == "void") base = getVoidType();
        if (dims.empty()) return base;
        std::vector<int> dimVals;
        for (Expr* e : dims) {
            bool ok = true;
            int val = evalConstExpr(e, ok);
            dimVals.push_back(ok ? val : 0);
        }
        return new ArrayType(base, dimVals);
    }
/* 预声明SysY运行时库函数到符号表，包括getint/putint等IO函数 */
void SemanticAnalyzer::declareSysYLib() {
        symtab.insert("getint", SymbolEntry("getint", new FunctionType(getIntType(), {}), false, 0, FUNC_KIND));
        symtab.insert("getch", SymbolEntry("getch", new FunctionType(getIntType(), {}), false, 0, FUNC_KIND));
        symtab.insert("getarray", SymbolEntry("getarray", new FunctionType(getIntType(), {new ArrayType(getIntType(), {-1})}), false, 0, FUNC_KIND));
        symtab.insert("putint", SymbolEntry("putint", new FunctionType(getVoidType(), {getIntType()}), false, 0, FUNC_KIND));
        symtab.insert("putch", SymbolEntry("putch", new FunctionType(getVoidType(), {getIntType()}), false, 0, FUNC_KIND));
        symtab.insert("putarray", SymbolEntry("putarray", new FunctionType(getVoidType(), {getIntType(), new ArrayType(getIntType(), {-1})}), false, 0, FUNC_KIND));
        std::vector<Type*> putfParams = {new ArrayType(getIntType(), {-1})};
        symtab.insert("putf", SymbolEntry("putf", new FunctionType(getVoidType(), putfParams), false, 0, FUNC_KIND));
        symtab.insert("starttime", SymbolEntry("starttime", new FunctionType(getVoidType(), {}), false, 0, FUNC_KIND));
        symtab.insert("stoptime", SymbolEntry("stoptime", new FunctionType(getVoidType(), {}), false, 0, FUNC_KIND));
    }
/* 输出语义错误信息到stderr并设置错误标志 */
void SemanticAnalyzer::report(const std::string& msg) {
        std::cerr << "[Semantic Error] " << msg << std::endl;
        hasError = true;
    }
/* 编译期常量表达式求值：支持Number、LVal(标量/数组元素常量)、BinaryExp、UnaryExp */
int SemanticAnalyzer::evalConstExpr(Expr* e, bool& ok) {
        if (auto* num = dynamic_cast<Number*>(e)) {
            if (!num->isFloat) { ok = true; return num->iVal; }
        }
        if (auto* lv = dynamic_cast<LVal*>(e)) {
            auto* entry = symtab.lookup(lv->name, VAR_KIND);
            if (!entry || !entry->isConst) { ok = false; return 0; }
            // 允许 INT 标量 或 INT 数组（基类型为 INT 的数组常量）
            bool isIntScalar = (entry->type->getKind() == Type::INT);
            bool isIntArray = (entry->type->getKind() == Type::ARRAY &&
                               static_cast<ArrayType*>(entry->type)->baseType->getKind() == Type::INT);
            if (!isIntScalar && !isIntArray) { ok = false; return 0; }

            if (lv->dims.empty()) {
                if (!isIntScalar) { ok = false; return 0; }
                // 标量常量
                auto it = constValues.find(lv->name);
                if (it != constValues.end() && it->second.isScalar) {
                    ok = true; return it->second.scalarVal;
                }
            } else {
                if (!isIntArray) { ok = false; return 0; }
                // 数组元素常量：先求值所有下标，再查表
                auto* arrTy = static_cast<ArrayType*>(entry->type);
                if (lv->dims.size() > arrTy->dims.size()) { ok = false; return 0; }

                std::vector<int> indices;
                for (Expr* d : lv->dims) {
                    bool idxOk = true;
                    int idx = evalConstExpr(d, idxOk);
                    if (!idxOk || idx < 0) { ok = false; return 0; }
                    indices.push_back(idx);
                }

                // 计算扁平化索引
                int flatIdx = 0;
                for (size_t i = 0; i < indices.size(); ++i) {
                    int stride = 1;
                    for (size_t j = i + 1; j < arrTy->dims.size(); ++j) stride *= arrTy->dims[j];
                    flatIdx += indices[i] * stride;
                }

                auto it = constValues.find(lv->name);
                if (it != constValues.end() && !it->second.isScalar) {
                    if (flatIdx >= 0 && flatIdx < static_cast<int>(it->second.arrayValues.size())) {
                        ok = true;
                        return it->second.arrayValues[flatIdx];
                    }
                }
            }
        }
        if (auto* be = dynamic_cast<BinaryExp*>(e)) {
            int lhs = evalConstExpr(be->lhs, ok);
            if (!ok) return 0;
            int rhs = evalConstExpr(be->rhs, ok);
            if (!ok) return 0;
            if (be->op == "+") return lhs + rhs;
            if (be->op == "-") return lhs - rhs;
            if (be->op == "*") return lhs * rhs;
            if (be->op == "/") {
                if (rhs == 0) { ok = false; report("division by zero in constant expression"); return 0; }
                return lhs / rhs;
            }
            if (be->op == "%") {
                if (rhs == 0) { ok = false; report("division by zero in constant expression"); return 0; }
                return lhs % rhs;
            }
        }
        if (auto* ue = dynamic_cast<UnaryExp*>(e)) {
            if (ue->op == "-") {
                int v = evalConstExpr(ue->operand, ok);
                return ok ? -v : 0;
            }
            if (ue->op == "+") return evalConstExpr(ue->operand, ok);
        }
        ok = false;
        return 0;
    }
/* 递归扁平化常量数组初始化列表，支持嵌套、补零和单值初始化 */
bool SemanticAnalyzer::flattenConstArrayInit(InitVal* init, const std::vector<int>& dims,
                                         std::vector<int>& out, int depth) {
        if (depth == static_cast<int>(dims.size())) {
            if (auto* ei = dynamic_cast<ExprInitVal*>(init)) {
                bool ok = true;
                int val = evalConstExpr(ei->expr, ok);
                if (!ok) return false;
                out.push_back(val);
                return true;
            }
            return false;
        }
        if (auto* av = dynamic_cast<ArrayInitVal*>(init)) {
            int count = 0;
            for (auto v : av->vals) {
                if (!flattenConstArrayInit(v, dims, out, depth + 1)) return false;
                count++;
            }
            // 补零：当前维度剩余元素用 0 填充
            int tailSize = 1;
            for (size_t i = depth + 1; i < dims.size(); ++i) tailSize *= dims[i];
            while (count < dims[depth]) {
                for (int i = 0; i < tailSize; ++i) out.push_back(0);
                count++;
            }
            return true;
        }
        // 单值初始化多维数组的情况（如 const int a[2][2] = 1;）
        if (auto* ei = dynamic_cast<ExprInitVal*>(init)) {
            bool ok = true;
            int val = evalConstExpr(ei->expr, ok);
            if (!ok) return false;
            int total = 1;
            for (size_t i = depth; i < dims.size(); ++i) total *= dims[i];
            for (int i = 0; i < total; ++i) out.push_back(val);
            return true;
        }
        return false;
    }
/* 判断表达式是否为纯常量（不含变量和函数调用） */
bool SemanticAnalyzer::isConstExpr(Expr* e) {
        if (dynamic_cast<Number*>(e)) return true;
        if (auto* be = dynamic_cast<BinaryExp*>(e))
            return isConstExpr(be->lhs) && isConstExpr(be->rhs);
        if (auto* ue = dynamic_cast<UnaryExp*>(e))
            return isConstExpr(ue->operand);
        return false;
    }
/* 检查数组初始化列表的元素类型是否与数组基类型兼容 */
bool SemanticAnalyzer::checkInitType(InitVal* init, Type* baseType, const std::string& arrName) {
        if (auto* av = dynamic_cast<ArrayInitVal*>(init)) {
            bool ok = true;
            for (auto v : av->vals) if (!checkInitType(v, baseType, arrName)) ok = false;
            return ok;
        } else if (auto* ei = dynamic_cast<ExprInitVal*>(init)) {
            if (auto* num = dynamic_cast<Number*>(ei->expr)) {
                bool isFloatArr = (baseType->getKind() == Type::FLOAT);
                if (num->isFloat && !isFloatArr) {
                    report("integer array '" + arrName + "' cannot be initialized with float constant");
                    return false;
                }
                
                return true;
            }
        }
        return true;
    }
/* 检查数组维度是否为正整数常量表达式 */
void SemanticAnalyzer::checkArrayDims(const std::string& name, const std::vector<Expr*>& dims) {
        for (Expr* e : dims) {
            bool ok = true;
            int val = evalConstExpr(e, ok);
            if (!ok) report("array dimension of '" + name + "' must be constant integer expression");
            else if (val <= 0) report("array dimension of '" + name + "' must be positive, got " + std::to_string(val));
        }
    }
/* 检查语句的所有控制流路径是否都包含带表达式的return语句 */
bool SemanticAnalyzer::hasReturnOnAllPaths(Stmt* stmt) {
        if (!stmt) return false;
        if (auto* ret = dynamic_cast<ReturnStmt*>(stmt)) {
            return ret->expr != nullptr;
        }
        if (auto* block = dynamic_cast<Block*>(stmt)) {
            for (auto item : block->items) {
                if (auto* si = dynamic_cast<StmtItem*>(item)) {
                    if (si->stmt && hasReturnOnAllPaths(si->stmt)) return true;
                }
            }
            return false;
        }
        if (auto* ifStmt = dynamic_cast<IfStmt*>(stmt)) {
            bool thenHas = ifStmt->thenStmt && hasReturnOnAllPaths(ifStmt->thenStmt);
            bool elseHas = ifStmt->elseStmt && hasReturnOnAllPaths(ifStmt->elseStmt);
            return thenHas && elseHas;
        }
        if (dynamic_cast<WhileStmt*>(stmt) ||
            dynamic_cast<BreakStmt*>(stmt) ||
            dynamic_cast<ContinueStmt*>(stmt)) {
            return false;
        }
        return false;
    }
/* 编译单元语义分析：声明运行时库并遍历所有顶层定义 */
void SemanticAnalyzer::visit(CompUnit& node) {
        declareSysYLib(); 
        for (auto item : node.items) item->accept(*this);
        if (!foundMain) report("missing main function");
    }
/* 常量声明语义分析：检查维度、类型一致性并插入符号表 */
void SemanticAnalyzer::visit(ConstDecl& node) {
        for (auto def : node.defs) {
            checkArrayDims(def->name, def->dims); 
            if (def->initVal && !def->dims.empty()) {
                Type* base = makeType(node.btype, {});
                checkInitType(def->initVal, base, def->name);
            }
            Type* t = makeType(node.btype, def->dims);
            
            if (def->initVal) def->initVal->accept(*this);
            if (!symtab.insert(def->name, SymbolEntry(def->name, t, true, symtab.getCurrentScopeLevel())))
                report("redefinition of const '" + def->name + "'");
            else {
                // 支持 int 标量、int 数组、float 标量常量
                bool isIntScalar = (t->getKind() == Type::INT);
                bool isIntArray = (t->getKind() == Type::ARRAY &&
                                   static_cast<ArrayType*>(t)->baseType->getKind() == Type::INT);
                bool isFloatScalar = (t->getKind() == Type::FLOAT);
                if (isIntScalar || isIntArray) {
                    if (def->dims.empty()) {
                        // int 标量常量
                        if (auto* ei = dynamic_cast<ExprInitVal*>(def->initVal)) {
                            bool evalOk = true;
                            int val = evalConstExpr(ei->expr, evalOk);
                            if (evalOk) {
                                ConstValue cv;
                                cv.isScalar = true;
                                cv.scalarVal = val;
                                constValues[def->name] = cv;
                            }
                        }
                    } else {
                        // int 常量数组：扁平化保存所有元素值
                        auto* arrTy = static_cast<ArrayType*>(t);
                        std::vector<int> flatVals;
                        if (flattenConstArrayInit(def->initVal, arrTy->dims, flatVals)) {
                            ConstValue cv;
                            cv.isScalar = false;
                            cv.arrayDims = arrTy->dims;
                            cv.arrayValues = flatVals;
                            constValues[def->name] = cv;
                        }
                    }
                } else if (isFloatScalar && def->dims.empty()) {
                    // float 标量常量：直接读取 Number 的 fVal
                    if (auto* ei = dynamic_cast<ExprInitVal*>(def->initVal)) {
                        if (auto* num = dynamic_cast<Number*>(ei->expr)) {
                            ConstValue cv;
                            cv.isScalar = true;
                            cv.isFloat = true;
                            cv.floatVal = num->isFloat ? num->fVal : static_cast<float>(num->iVal);
                            constValues[def->name] = cv;
                        }
                    }
                }
            }
        }
    }
/* 变量声明语义分析：全局变量初值必须为常量表达式 */
void SemanticAnalyzer::visit(VarDecl& node) {
        for (auto def : node.defs) {
            checkArrayDims(def->name, def->dims); 
            if (def->initVal && !def->dims.empty()) {
                Type* base = makeType(node.btype, {});
                checkInitType(def->initVal, base, def->name);
            }
            Type* t = makeType(node.btype, def->dims);
            
            if (symtab.getCurrentScopeLevel() == 0 && def->initVal) {
                if (auto* ei = dynamic_cast<ExprInitVal*>(def->initVal)) {
                    if (!isConstExpr(ei->expr))
                        report("global variable '" + def->name + "' initializer must be constant expression");
                }
            }
            
            if (def->initVal) def->initVal->accept(*this);
            if (!symtab.insert(def->name, SymbolEntry(def->name, t, false, symtab.getCurrentScopeLevel())))
                report("redefinition of variable '" + def->name + "'");
        }
    }
/* 函数定义语义分析：检查main函数签名、参数类型并管理作用域 */
void SemanticAnalyzer::visit(FuncDef& node) {
        if (node.name == "main") {
            if (foundMain) { report("multiple definitions of main"); }
            foundMain = true;
            if (node.retType != "int") report("main must return int");
            if (!node.params.empty()) report("main must take no parameters");
        }
        Type* ret = makeType(node.retType, {});
        std::vector<Type*> paramTypes;
        for (auto p : node.params) {
            std::vector<int> dimVals;
            for (Expr* e : p->dims) {
                if (e == nullptr) dimVals.push_back(-1);
                else {
                    bool ok = true;
                    int val = evalConstExpr(e, ok);
                    if (!ok) report("array dimension of parameter '" + p->name + "' must be constant integer expression");
                    else if (val <= 0) report("array dimension of parameter '" + p->name + "' must be positive");
                    dimVals.push_back(ok && val > 0 ? val : 0);
                }
            }
            Type* pt = makeType(p->btype, {});
            if (!p->dims.empty()) pt = new ArrayType(pt, dimVals);
            paramTypes.push_back(pt);
        }
        if (!symtab.insert(node.name, SymbolEntry(node.name, new FunctionType(ret, paramTypes), false, symtab.getCurrentScopeLevel(), FUNC_KIND)))
            report("redefinition of function '" + node.name + "'");

        currentFuncRetType = node.retType;
        symtab.enterScope();
        for (auto p : node.params) {
            std::vector<int> dimVals;
            for (Expr* e : p->dims) {
                if (e == nullptr) dimVals.push_back(-1);
                else {
                    bool ok = true;
                    int val = evalConstExpr(e, ok);   
                    dimVals.push_back(ok && val > 0 ? val : 0);
                }
            }
            Type* pt = makeType(p->btype, {});
            if (!p->dims.empty()) pt = new ArrayType(pt, dimVals);
            if (!symtab.insert(p->name, SymbolEntry(p->name, pt, false, symtab.getCurrentScopeLevel())))
                report("redefinition of parameter '" + p->name + "'");
        }
        if (node.body) node.body->accept(*this);
        if (node.body && (node.retType == "int" || node.retType == "float")) {
            if (!hasReturnOnAllPaths(node.body))
                report("function '" + node.name + "' may not return a value on all paths");
        }
        symtab.exitScope();
        currentFuncRetType.clear();
    }
/* 语句块语义分析：创建新作用域并遍历内部声明和语句 */
void SemanticAnalyzer::visit(Block& node) {
        symtab.enterScope();
        for (auto item : node.items) item->accept(*this);
        symtab.exitScope();
    }
/* 赋值语句语义分析：检查左值是否为常量（不可赋值） */
void SemanticAnalyzer::visit(AssignStmt& node) {
        if (node.lval) {
            node.lval->accept(*this);
            auto* entry = symtab.lookup(node.lval->name, VAR_KIND);
            if (entry && entry->isConst)
                report("cannot assign to const '" + node.lval->name + "'");
        }
        if (node.expr) node.expr->accept(*this);
    }
/* 左值语义分析：检查变量是否已定义、数组维度是否匹配 */
void SemanticAnalyzer::visit(LVal& node) {
        auto* entry = symtab.lookup(node.name, VAR_KIND);
        if (!entry) { report("undefined '" + node.name + "'"); return; }
        if (entry->type->getKind() == Type::ARRAY) {
            auto* arr = static_cast<ArrayType*>(entry->type);   
            if (node.dims.size() > arr->dims.size())
                report("array '" + node.name + "' expects at most " + std::to_string(arr->dims.size()) +
                       " dims, got " + std::to_string(node.dims.size()));
        } else if (!node.dims.empty()) {
            report("'" + node.name + "' is not an array");
        }
        for (auto d : node.dims) if (d) d->accept(*this);
    }
/* 函数调用语义分析：检查函数存在性、参数数量和数组类型匹配 */
void SemanticAnalyzer::visit(CallExp& node) {
        auto* entry = symtab.lookup(node.funcName, FUNC_KIND);
        if (!entry) { report("undefined function '" + node.funcName + "'"); return; }
        if (entry->type->getKind() != Type::FUNCTION) {
            report("'" + node.funcName + "' is not a function"); return;
        }
        auto* ft = static_cast<FunctionType*>(entry->type);
        if (node.funcName == "putf") {
            if (node.args.empty()) report("putf expects at least 1 argument");
        } else if (node.args.size() != ft->paramTypes.size()) {
            report("function '" + node.funcName + "' expects " +
                   std::to_string(ft->paramTypes.size()) + " args, got " +
                   std::to_string(node.args.size()));
        }
        
        for (size_t i = 0; i < node.args.size() && i < ft->paramTypes.size(); ++i) {
            Type* paramTy = ft->paramTypes[i];
            if (paramTy->getKind() == Type::ARRAY) {
                auto* arrParam = static_cast<ArrayType*>(paramTy);
                if (auto* lv = dynamic_cast<LVal*>(node.args[i])) {
                    auto* argEntry = symtab.lookup(lv->name, VAR_KIND);
                    if (argEntry) {
                        if (argEntry->type->getKind() == Type::ARRAY) {
                            auto* arrArg = static_cast<ArrayType*>(argEntry->type);
                            if (!arrParam->baseType->equals(arrArg->baseType)) {
                                report("array argument type mismatch for parameter " + std::to_string(i+1) +
                                       " of '" + node.funcName + "': expected " + arrParam->baseType->toString() +
                                       ", got " + arrArg->baseType->toString());
                            } else {
                                int argDims = static_cast<int>(arrArg->dims.size());
                                int usedDims = static_cast<int>(lv->dims.size());
                                int remainingDims = argDims - usedDims;
                                int paramDims = static_cast<int>(arrParam->dims.size());
                                if (remainingDims != paramDims) {
                                    report("array argument dimension mismatch for parameter " + std::to_string(i+1) +
                                           " of '" + node.funcName + "': expected " + std::to_string(paramDims) +
                                           " dims, got " + std::to_string(remainingDims));
                                } else {
                                    for (int d = 0; d < paramDims; ++d) {
                                        int paramDim = arrParam->dims[d];
                                        int argDim = arrArg->dims[usedDims + d];
                                        if (paramDim > 0 && argDim > 0 && paramDim != argDim) {
                                            report("array argument size mismatch at dimension " + std::to_string(d+1) +
                                                   " for parameter " + std::to_string(i+1) + " of '" + node.funcName +
                                                   "': expected " + std::to_string(paramDim) +
                                                   ", got " + std::to_string(argDim));
                                            break;
                                        }
                                    }
                                }
                            }
                        } else {
                            report("argument " + std::to_string(i+1) + " of '" + node.funcName +
                                   "' is not an array, expected array type");
                        }
                    }
                }
                else if (!dynamic_cast<StringLiteral*>(node.args[i])) {
                    report("argument " + std::to_string(i+1) + " of '" + node.funcName +
                           "' must be an array variable or array slice");
                }
            }
        }
        
        for (auto a : node.args) if (a) a->accept(*this);
    }
/* if语句语义分析：递归检查条件和分支 */
void SemanticAnalyzer::visit(IfStmt& node) {
        if (node.cond) node.cond->accept(*this);
        if (node.thenStmt) node.thenStmt->accept(*this);
        if (node.elseStmt) node.elseStmt->accept(*this);
    }
/* while语句语义分析：增加循环深度计数 */
void SemanticAnalyzer::visit(WhileStmt& node) {
        if (node.cond) node.cond->accept(*this);
        loopDepth++;
        if (node.body) node.body->accept(*this);
        loopDepth--;
    }
/* break语句语义分析：检查是否在循环内部 */
void SemanticAnalyzer::visit(BreakStmt& node) {
        if (loopDepth == 0) report("break not within loop");
    }
/* continue语句语义分析：检查是否在循环内部 */
void SemanticAnalyzer::visit(ContinueStmt& node) {
        if (loopDepth == 0) report("continue not within loop");
    }
/* return语句语义分析：检查返回值与函数返回类型匹配 */
void SemanticAnalyzer::visit(ReturnStmt& node) {
        if (currentFuncRetType.empty()) { report("return outside function"); return; }
        if (currentFuncRetType == "void") {
            if (node.expr) report("void function should not return value");
        } else {
            if (!node.expr) report("non-void function must return value");
            else node.expr->accept(*this);
        }
    }

void SemanticAnalyzer::visit(BinaryExp& node) {
        if (node.lhs) node.lhs->accept(*this);
        if (node.rhs) node.rhs->accept(*this);
    }

void SemanticAnalyzer::visit(UnaryExp& node) {
        if (node.operand) node.operand->accept(*this);
    }

void SemanticAnalyzer::visit(Number& node) {}

void SemanticAnalyzer::visit(ExprInitVal& node) {}

void SemanticAnalyzer::visit(ArrayInitVal& node) {}

void SemanticAnalyzer::visit(ConstDef& node) {
        if (node.initVal) node.initVal->accept(*this);
    }

void SemanticAnalyzer::visit(VarDef& node) {
        if (node.initVal) node.initVal->accept(*this);
    }

void SemanticAnalyzer::visit(FuncFParam& node) {}

void SemanticAnalyzer::visit(DeclItem& node) {
    if (node.decl) node.decl->accept(*this);
}

void SemanticAnalyzer::visit(StmtItem& node) {
    if (node.stmt) node.stmt->accept(*this);
}

void SemanticAnalyzer::visit(ExpStmt& node) {
    if (node.expr) node.expr->accept(*this);
}

void SemanticAnalyzer::visit(StringLiteral& node) {}
