/* ============================================================================
 * 文件名称: IrBuilder.cpp
 * 功能描述: LLVM IR生成器实现，将AST转换为LLVM IR并支持RISC-V目标代码生成
 * 作者信息: SysY2022 Compiler Team
 * 版本信息: 1.0
 * 所属项目: SysY2022 编译器
 * ============================================================================ */

#include "IrBuilder.h"
#include <iostream>
#include <functional>
#include <llvm/IR/Verifier.h>
#include <llvm/TargetParser/Triple.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>

using namespace std;

/* 编译期求值数组维度表达式，支持标量常量和数组元素常量 */
static int evalDimExpr(Expr* e, const std::map<std::string, ConstValue>& constValues) {
    if (auto* num = dynamic_cast<Number*>(e)) return num->iVal;
    if (auto* lv = dynamic_cast<LVal*>(e)) {
        auto it = constValues.find(lv->name);
        if (it != constValues.end()) {
            if (lv->dims.empty() && it->second.isScalar) {
                if (it->second.isFloat) return static_cast<int>(it->second.floatVal);
                return it->second.scalarVal;
            }
            if (!lv->dims.empty() && !it->second.isScalar) {
                // 数组元素常量：计算扁平化索引
                const auto& dims = it->second.arrayDims;
                if (lv->dims.size() > dims.size()) return 0;
                int flatIdx = 0;
                for (size_t i = 0; i < lv->dims.size(); ++i) {
                    int idx = evalDimExpr(lv->dims[i], constValues);
                    if (idx < 0) return 0;
                    int stride = 1;
                    for (size_t j = i + 1; j < dims.size(); ++j) stride *= dims[j];
                    flatIdx += idx * stride;
                }
                if (flatIdx >= 0 && flatIdx < static_cast<int>(it->second.arrayValues.size())) {
                    return it->second.arrayValues[flatIdx];
                }
            }
        }
    }
    if (auto* be = dynamic_cast<BinaryExp*>(e)) {
        int lhs = evalDimExpr(be->lhs, constValues);
        int rhs = evalDimExpr(be->rhs, constValues);
        if (be->op == "+") return lhs + rhs;
        if (be->op == "-") return lhs - rhs;
        if (be->op == "*") return lhs * rhs;
        if (be->op == "/") return rhs != 0 ? lhs / rhs : 1;
        if (be->op == "%") return rhs != 0 ? lhs % rhs : 1;
    }
    if (auto* ue = dynamic_cast<UnaryExp*>(e)) {
        if (ue->op == "-") return -evalDimExpr(ue->operand, constValues);
        if (ue->op == "+") return evalDimExpr(ue->operand, constValues);
    }
    return 0;
}

/* 将常量表达式求值为LLVM Constant对象，支持标量/数组元素常量 */
static llvm::Constant* evalExprToLLVMConstant(Expr* e,
    const std::map<std::string, ConstValue>& constValues,
    llvm::LLVMContext& ctx, llvm::Type* ty) {
    if (auto* num = dynamic_cast<Number*>(e)) {
        if (ty->isIntegerTy()) {
            double val = num->isFloat ? num->fVal : static_cast<double>(num->iVal);
            return llvm::ConstantInt::get(ty, static_cast<int64_t>(val));
        } else if (ty->isFloatTy()) {
            double val = num->isFloat ? num->fVal : static_cast<double>(num->iVal);
            return llvm::ConstantFP::get(ty, val);
        }
    }
    if (auto* lv = dynamic_cast<LVal*>(e)) {
        auto it = constValues.find(lv->name);
        if (it != constValues.end()) {
            int val = 0;
            if (lv->dims.empty() && it->second.isScalar) {
                if (it->second.isFloat) {
                    if (ty->isFloatTy()) return llvm::ConstantFP::get(ty, it->second.floatVal);
                    else if (ty->isIntegerTy()) return llvm::ConstantInt::get(ty, static_cast<int>(it->second.floatVal));
                }
                val = it->second.scalarVal;
            } else if (!lv->dims.empty() && !it->second.isScalar) {
                const auto& dims = it->second.arrayDims;
                if (lv->dims.size() <= dims.size()) {
                    int flatIdx = 0;
                    for (size_t i = 0; i < lv->dims.size(); ++i) {
                        int idx = evalDimExpr(lv->dims[i], constValues);
                        if (idx < 0) return nullptr;
                        int stride = 1;
                        for (size_t j = i + 1; j < dims.size(); ++j) stride *= dims[j];
                        flatIdx += idx * stride;
                    }
                    if (flatIdx >= 0 && flatIdx < static_cast<int>(it->second.arrayValues.size())) {
                        val = it->second.arrayValues[flatIdx];
                    }
                }
            }
            if (ty->isIntegerTy()) return llvm::ConstantInt::get(ty, val);
            else if (ty->isFloatTy()) return llvm::ConstantFP::get(ty, static_cast<double>(val));
        }
    }
    if (auto* be = dynamic_cast<BinaryExp*>(e)) {
        auto* L = evalExprToLLVMConstant(be->lhs, constValues, ctx, ty);
        auto* R = evalExprToLLVMConstant(be->rhs, constValues, ctx, ty);
        if (!L || !R) return nullptr;
        if (ty->isIntegerTy()) {
            int lval = static_cast<llvm::ConstantInt*>(L)->getSExtValue();
            int rval = static_cast<llvm::ConstantInt*>(R)->getSExtValue();
            int result = 0;
            if (be->op == "+") result = lval + rval;
            else if (be->op == "-") result = lval - rval;
            else if (be->op == "*") result = lval * rval;
            else if (be->op == "/") result = rval != 0 ? lval / rval : 0;
            else if (be->op == "%") result = rval != 0 ? lval % rval : 0;
            return llvm::ConstantInt::get(ty, result);
        } else if (ty->isFloatTy()) {
            double lval = static_cast<llvm::ConstantFP*>(L)->getValueAPF().convertToDouble();
            double rval = static_cast<llvm::ConstantFP*>(R)->getValueAPF().convertToDouble();
            double result = 0;
            if (be->op == "+") result = lval + rval;
            else if (be->op == "-") result = lval - rval;
            else if (be->op == "*") result = lval * rval;
            else if (be->op == "/") result = rval != 0 ? lval / rval : 0;
            return llvm::ConstantFP::get(ty, result);
        }
    }
    if (auto* ue = dynamic_cast<UnaryExp*>(e)) {
        auto* V = evalExprToLLVMConstant(ue->operand, constValues, ctx, ty);
        if (!V) return nullptr;
        if (ty->isIntegerTy()) {
            int v = static_cast<llvm::ConstantInt*>(V)->getSExtValue();
            if (ue->op == "-") return llvm::ConstantInt::get(ty, -v);
            if (ue->op == "+") return V;
        } else if (ty->isFloatTy()) {
            double v = static_cast<llvm::ConstantFP*>(V)->getValueAPF().convertToDouble();
            if (ue->op == "-") return llvm::ConstantFP::get(ty, -v);
            if (ue->op == "+") return V;
        }
    }
    return nullptr;
}

/* 辅助函数：根据类型名构造SysY类型（供局部使用） */
static Type* makeType(const string& btype, const vector<Expr*>& dims,
    const std::map<std::string, ConstValue>& constValues) {
    Type* base = nullptr;
    if (btype == "int") base = getIntType();
    else if (btype == "float") base = getFloatType();
    else if (btype == "void") base = getVoidType();
    if (dims.empty()) return base;
    vector<int> dimVals;
    for (Expr* e : dims) {
        dimVals.push_back(evalDimExpr(e, constValues));
    }
    return new ArrayType(base, dimVals);
}

/* 从符号表获取标识符对应的LLVM Value指针 */
llvm::Value* IrBuilder::getLLVMValue(const string& name) {
    auto* entry = symtab.lookup(name, VAR_KIND);
    if (entry && entry->llvmValue) return static_cast<llvm::Value*>(entry->llvmValue);
    return nullptr;
}

/* 将LLVM Value指针保存到符号表条目 */
void IrBuilder::setLLVMValue(const string& name, llvm::Value* val) {
    auto* entry = symtab.lookup(name, VAR_KIND);
    if (entry) entry->llvmValue = val;
}

/* 将SysY类型转换为对应的LLVM IR类型 */
llvm::Type* IrBuilder::toLLVMType(Type* ty) {
    if (!ty) return llvm::Type::getInt32Ty(context);
    return ty->toLLVM(context);
}

/* 生成LLVM load指令，从内存地址读取值 */
llvm::Value* IrBuilder::createLoad(llvm::Value* ptr, Type* ty) {
    if (!ptr) return nullptr;
    return builder.CreateLoad(toLLVMType(ty), ptr, "load");
}

/* 短路求值条件跳转生成：支持 && 和 || 的惰性求值 */
void IrBuilder::createCondBr(Expr* cond, llvm::BasicBlock* trueBB, llvm::BasicBlock* falseBB) {
    if (auto* be = dynamic_cast<BinaryExp*>(cond)) {
        if (be->op == "||") {
            
            llvm::BasicBlock* rhsBB = llvm::BasicBlock::Create(context, "lor.rhs", currentFunction);
            createCondBr(be->lhs, trueBB, rhsBB);
            builder.SetInsertPoint(rhsBB);
            createCondBr(be->rhs, trueBB, falseBB);
            return;
        } else if (be->op == "&&") {
            
            llvm::BasicBlock* rhsBB = llvm::BasicBlock::Create(context, "land.rhs", currentFunction);
            createCondBr(be->lhs, rhsBB, falseBB);
            builder.SetInsertPoint(rhsBB);
            createCondBr(be->rhs, trueBB, falseBB);
            return;
        }
    }

    
    cond->accept(*this);
    llvm::Value* val = lastValue;
    if (!val) {
        builder.CreateBr(falseBB);
        return;
    }

    llvm::Value* cmp = nullptr;
    if (val->getType()->isFloatTy()) {
        cmp = builder.CreateFCmpONE(val, llvm::ConstantFP::get(val->getType(), 0.0), "cmp");
    } else {
        llvm::Value* zero = llvm::ConstantInt::get(val->getType(), 0);
        cmp = builder.CreateICmpNE(val, zero, "cmp");
    }
    builder.CreateCondBr(cmp, trueBB, falseBB);
}

/* 类型转换：int->float 或 float->int（SIToFP/FPToSI） */
llvm::Value* IrBuilder::createCast(llvm::Value* val, llvm::Type* targetLLVM) {
    if (!val || !targetLLVM || val->getType() == targetLLVM) return val;
    if (val->getType()->isIntegerTy() && targetLLVM->isFloatTy()) {
        return builder.CreateSIToFP(val, targetLLVM, "sitofp");
    } else if (val->getType()->isFloatTy() && targetLLVM->isIntegerTy()) {
        return builder.CreateFPToSI(val, targetLLVM, "fptosi");
    }
    return val;
}

/* 将嵌套数组初始化器完全扁平化为标量Constant列表 */
static void flattenToScalars(InitVal* init, std::vector<llvm::Constant*>& out,
                             llvm::Type* elemTy, llvm::Constant* zero,
                             const std::map<std::string, ConstValue>& constValues,
                             llvm::LLVMContext& ctx) {
    if (auto* av = dynamic_cast<ArrayInitVal*>(init)) {
        for (auto v : av->vals) {
            flattenToScalars(v, out, elemTy, zero, constValues, ctx);
        }
    } else if (auto* ei = dynamic_cast<ExprInitVal*>(init)) {
        if (auto* num = dynamic_cast<Number*>(ei->expr)) {
            if (num->isFloat) out.push_back(llvm::ConstantFP::get(elemTy, num->fVal));
            else out.push_back(llvm::ConstantInt::get(elemTy, num->iVal));
        } else {
            llvm::Constant* c = evalExprToLLVMConstant(ei->expr, constValues, ctx, elemTy);
            if (c) out.push_back(c);
            else out.push_back(zero);
        }
    } else {
        out.push_back(zero);
    }
}

/* 递归按LLVM数组维度分组构建嵌套ConstantArray */
static llvm::Constant* buildNestedArray(llvm::Type* ty,
    const std::vector<llvm::Constant*>& flat, size_t& pos) {
    if (auto* arrTy = llvm::dyn_cast<llvm::ArrayType>(ty)) {
        unsigned numElems = arrTy->getNumElements();
        llvm::Type* elemTy = arrTy->getElementType();
        std::vector<llvm::Constant*> nested;
        for (unsigned i = 0; i < numElems; ++i) {
            nested.push_back(buildNestedArray(elemTy, flat, pos));
        }
        return llvm::ConstantArray::get(arrTy, nested);
    } else {
        if (pos < flat.size()) return flat[pos++];
        return llvm::ConstantInt::get(ty, 0);
    }
}

/* 构建嵌套数组LLVM类型 [d0 x [d1 x ... x base]] */
llvm::Type* IrBuilder::getNestedArrayLLVMType(ArrayType* arrTy) {
    llvm::Type* inner = arrTy->baseType->toLLVM(context);
    for (int i = static_cast<int>(arrTy->dims.size()) - 1; i >= 0; --i) {
        if (arrTy->dims[i] > 0) {
            inner = llvm::ArrayType::get(inner, arrTy->dims[i]);
        }
    }
    return inner;
}

/* 计算数组元素GEP指针，支持多维数组和退化数组参数 */
llvm::Value* IrBuilder::getArrayElementPtr(LVal& node, SymbolEntry* entry) {
    llvm::Value* basePtr = static_cast<llvm::Value*>(entry->llvmValue);
    if (!basePtr) return nullptr;

    auto* arrTy = dynamic_cast<ArrayType*>(entry->type);
    if (!arrTy) return nullptr;

    std::vector<llvm::Value*> indices;
    bool isDecayed = (!arrTy->dims.empty() && arrTy->dims[0] == -1);

    if (!isDecayed) {
        indices.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0));
    }

    for (Expr* dimExpr : node.dims) {
        dimExpr->accept(*this);
        llvm::Value* idx = lastValue;
        if (!idx) return nullptr;
        if (idx->getType()->isFloatTy()) {
            idx = builder.CreateFPToSI(idx, llvm::Type::getInt32Ty(context), "fptosi");
        }
        indices.push_back(idx);
    }

    llvm::Type* pointeeType = nullptr;
    if (isDecayed) {
        llvm::Type* inner = arrTy->baseType->toLLVM(context);
        for (int i = static_cast<int>(arrTy->dims.size()) - 1; i > 0; --i) {
            if (arrTy->dims[i] > 0) inner = llvm::ArrayType::get(inner, arrTy->dims[i]);
        }
        pointeeType = inner;
    } else {
        pointeeType = getNestedArrayLLVMType(arrTy);
    }

    return builder.CreateGEP(pointeeType, basePtr, indices, "gep");
}

/* 编译器入口：遍历AST生成完整LLVM IR模块 */
bool IrBuilder::generateIR(AstNode* root) {
    if (!root) return false;
    root->accept(*this);
    return !hasError;
}

/* 将生成的LLVM IR输出为文本格式 */
void IrBuilder::printIR(ostream& out) {
    string str;
    llvm::raw_string_ostream os(str);
    module.print(os, nullptr);
    out << os.str();
}

/* 阶段10：运行LLVM O2优化管道（循环优化、内联等） */
bool IrBuilder::optimize() {
    llvm::PassBuilder pb;
    llvm::LoopAnalysisManager lam;
    llvm::FunctionAnalysisManager fam;
    llvm::CGSCCAnalysisManager cgam;
    llvm::ModuleAnalysisManager mam;
    
    pb.registerModuleAnalyses(mam);
    pb.registerCGSCCAnalyses(cgam);
    pb.registerFunctionAnalyses(fam);
    pb.registerLoopAnalyses(lam);
    pb.crossRegisterProxies(lam, fam, cgam, mam);
    
    llvm::ModulePassManager mpm = pb.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O3);
    mpm.run(module, mam);
    return true;
}

/* 阶段10：生成RISC-V 64位汇编代码文件 */
bool IrBuilder::emitAssembly(const std::string& filename) {
    std::string error;
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget("riscv64", error);
    if (!target) {
        std::cerr << "[IR Error] Cannot lookup RISC-V target: " << error << std::endl;
        return false;
    }

    llvm::TargetOptions opt;
    llvm::Triple triple("riscv64-unknown-linux-gnu");
    llvm::TargetMachine* tm = target->createTargetMachine(
        triple, "generic-rv64", "+m,+a,+f,+d,+c",
        opt, llvm::Reloc::Static, llvm::CodeModel::Small,
        llvm::CodeGenOptLevel::Default);

    if (!tm) {
        std::cerr << "[IR Error] Cannot create RISC-V TargetMachine" << std::endl;
        return false;
    }

    module.setDataLayout(tm->createDataLayout());
    module.setTargetTriple(llvm::Triple("riscv64-unknown-linux-gnu"));

    std::error_code ec;
    llvm::raw_fd_ostream dest(filename, ec, llvm::sys::fs::OF_Text);
    if (ec) {
        std::cerr << "[IR Error] Cannot open output file: " << ec.message() << std::endl;
        delete tm;
        return false;
    }

    llvm::legacy::PassManager pass;
    if (tm->addPassesToEmitFile(pass, dest, nullptr, llvm::CodeGenFileType::AssemblyFile)) {
        std::cerr << "[IR Error] TargetMachine cannot emit assembly" << std::endl;
        delete tm;
        return false;
    }

    pass.run(module);
    dest.flush();
    delete tm;
    return true;
}

/* 预声明SysY运行时库函数到LLVM模块符号表 */
void IrBuilder::declareSysYLib() {
    llvm::Type* i32Ty = llvm::Type::getInt32Ty(context);
    llvm::Type* voidTy = llvm::Type::getVoidTy(context);
    llvm::Type* ptrTy = llvm::PointerType::get(context, 0);

    auto declare = [&](const std::string& name, llvm::Type* ret,
                       const std::vector<llvm::Type*>& params, bool vararg = false) {
        llvm::FunctionType* ft = llvm::FunctionType::get(ret, params, vararg);
        llvm::Function* func = llvm::Function::Create(
            ft, llvm::Function::ExternalLinkage, name, module);
        Type* sysyRet = (ret == i32Ty) ? getIntType() :
                          (ret == voidTy ? getVoidType() : getIntType());
        std::vector<Type*> sysyParams;
        for (auto* p : params) {
            if (p == ptrTy) sysyParams.push_back(new ArrayType(getIntType(), {-1}));
            else if (p == i32Ty) sysyParams.push_back(getIntType());
            else sysyParams.push_back(getIntType());
        }
        symtab.insert(name, SymbolEntry(name, new FunctionType(sysyRet, sysyParams), false, 0, FUNC_KIND));
        auto* entry = symtab.lookup(name, FUNC_KIND);
        if (entry) entry->llvmValue = func;
    };

    declare("getint",    i32Ty, {});
    declare("getch",     i32Ty, {});
    declare("getarray",  i32Ty, {ptrTy});
    declare("putint",    voidTy, {i32Ty});
    declare("putch",     voidTy, {i32Ty});
    declare("putarray",  voidTy, {i32Ty, ptrTy});
    declare("putf",      voidTy, {ptrTy}, true);
    declare("starttime", voidTy, {});
    declare("stoptime",  voidTy, {});
}

/* 编译单元IR生成：声明运行时库并遍历所有定义 */
void IrBuilder::visit(CompUnit& node) {
    declareSysYLib(); 
    for (auto item : node.items) item->accept(*this);
}

/* 函数定义IR生成：创建LLVM函数、参数alloca、生成函数体 */
void IrBuilder::visit(FuncDef& node) {
    Type* ret = makeType(node.retType, {}, constValues);
    vector<Type*> paramTypes;
    for (auto p : node.params) {
        vector<int> dimVals;
        for (Expr* e : p->dims) {
            if (e == nullptr) dimVals.push_back(-1);
            else dimVals.push_back(evalDimExpr(e, constValues));
        }
        Type* pt = makeType(p->btype, {}, constValues);
        if (!p->dims.empty()) pt = new ArrayType(pt, dimVals);
        paramTypes.push_back(pt);
    }
    Type* funcType = new FunctionType(ret, paramTypes);
    symtab.insert(node.name, SymbolEntry(node.name, funcType, false, symtab.getCurrentScopeLevel(), FUNC_KIND));

    llvm::Type* retTy = toLLVMType(ret);
    vector<llvm::Type*> paramTys;
    for (auto pt : paramTypes) {
        if (pt->getKind() == Type::ARRAY) {
            auto* arr = static_cast<ArrayType*>(pt);
            paramTys.push_back(llvm::PointerType::get(context, 0));   
        } else {
            paramTys.push_back(toLLVMType(pt));
        }
    }
    llvm::FunctionType* llvmFT = llvm::FunctionType::get(retTy, paramTys, false);

    llvm::Function* func = llvm::Function::Create(
        llvmFT, llvm::Function::ExternalLinkage, node.name, module);

    auto* funcEntry = symtab.lookup(node.name, FUNC_KIND);
    if (funcEntry) funcEntry->llvmValue = func;   
    
    llvm::Function* prevFunc = currentFunction;
    Type* prevRet = currentFuncRetType;
    currentFuncRetType = ret;
    currentFunction = func;

    llvm::BasicBlock* entryBB = llvm::BasicBlock::Create(context, "entry", func);
    builder.SetInsertPoint(entryBB);

    symtab.enterScope();

    size_t idx = 0;
    for (auto& arg : func->args()) {
        if (idx < node.params.size()) {
            auto* param = node.params[idx];
            arg.setName(param->name);
            Type* pType = paramTypes[idx];

            if (pType->getKind() == Type::ARRAY) {
                
                symtab.insert(param->name, SymbolEntry(param->name, pType, false, symtab.getCurrentScopeLevel()));
                setLLVMValue(param->name, &arg);
            } else {
                
                llvm::Value* alloca = builder.CreateAlloca(toLLVMType(pType), nullptr, param->name + ".addr");
                builder.CreateStore(&arg, alloca);
                symtab.insert(param->name, SymbolEntry(param->name, pType, false, symtab.getCurrentScopeLevel()));
                setLLVMValue(param->name, alloca);
            }
        }
        idx++;
    }

    if (node.body) node.body->accept(*this);

    if (!builder.GetInsertBlock()->getTerminator()) {
        if (ret->getKind() == Type::VOID) {
            builder.CreateRetVoid();
        } else if (ret->getKind() == Type::FLOAT) {
            builder.CreateRet(llvm::ConstantFP::get(llvm::Type::getFloatTy(context), 0.0));
        } else {
            builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0));
        }
    }

    if (llvm::verifyFunction(*func, &llvm::errs())) {
        cerr << "[IR Error] Function verification failed: " << node.name << endl;
        hasError = true;
    }
    symtab.exitScope();
    currentFuncRetType = prevRet;
    currentFunction = prevFunc;
}

/* 语句块IR生成：创建新作用域 */
void IrBuilder::visit(Block& node) {
    symtab.enterScope();
    for (auto item : node.items) item->accept(*this);
    symtab.exitScope();
}

void IrBuilder::visit(ConstDecl& node) {
    for (auto def : node.defs) def->accept(*this);
}

void IrBuilder::visit(VarDecl& node) {
    for (auto def : node.defs) def->accept(*this);
}

/* 常量定义IR生成：全局常量用ConstantArray，局部常量用alloca+store */
void IrBuilder::visit(ConstDef& node) {
    Type* t = makeType(node.btype, node.dims, constValues);
    llvm::Type* ty = toLLVMType(t);

    if (t->getKind() == Type::ARRAY) {
        auto* arrTy = static_cast<ArrayType*>(t);
        llvm::Type* nestedTy = getNestedArrayLLVMType(arrTy);

        if (symtab.getCurrentScopeLevel() == 0) {
            
            llvm::Constant* init = llvm::ConstantAggregateZero::get(nestedTy);
            if (node.initVal) {
              std::vector<llvm::Constant*> elems;
              llvm::Constant* zero = (arrTy->baseType->getKind() == Type::FLOAT) ?
                  static_cast<llvm::Constant*>(llvm::ConstantFP::get(arrTy->baseType->toLLVM(context), 0.0)) :
                  static_cast<llvm::Constant*>(llvm::ConstantInt::get(arrTy->baseType->toLLVM(context), 0));
              
              
              flattenToScalars(node.initVal, elems, arrTy->baseType->toLLVM(context), zero, constValues, context);
              
              
              int totalElems = 1;
              for (int d : arrTy->dims) totalElems *= d;
              while (static_cast<int>(elems.size()) < totalElems) {
                  elems.push_back(zero);
              }
              
              
              size_t pos = 0;
              init = buildNestedArray(nestedTy, elems, pos);
            }
            auto* gv = new llvm::GlobalVariable(module, nestedTy, true,
                llvm::GlobalValue::ExternalLinkage, init, node.name);
            symtab.insert(node.name, SymbolEntry(node.name, t, true, 0));
            setLLVMValue(node.name, gv);
        } else {
            
            llvm::Value* alloca = builder.CreateAlloca(nestedTy, nullptr, node.name);
            
            if (node.initVal) {
                std::vector<llvm::Value*> initValues;
                llvm::Value* zero = (arrTy->baseType->getKind() == Type::FLOAT) ?
                    static_cast<llvm::Value*>(llvm::ConstantFP::get(arrTy->baseType->toLLVM(context), 0.0)) :
                    static_cast<llvm::Value*>(llvm::ConstantInt::get(arrTy->baseType->toLLVM(context), 0));
                
                std::function<void(InitVal*, int)> flattenValues = [&](InitVal* iv, int depth) {
                    if (depth == static_cast<int>(arrTy->dims.size())) {
                        if (auto* ei = dynamic_cast<ExprInitVal*>(iv)) {
                            ei->expr->accept(*this);
                            initValues.push_back(lastValue ? lastValue : zero);
                        } else {
                            initValues.push_back(zero);
                        }
                        return;
                    }
                    if (auto* av = dynamic_cast<ArrayInitVal*>(iv)) {
                        int count = 0;
                        for (auto v : av->vals) {
                            flattenValues(v, depth + 1);
                            count++;
                        }
                        int tailSize = 1;
                        for (int i = depth + 1; i < static_cast<int>(arrTy->dims.size()); ++i) tailSize *= arrTy->dims[i];
                        while (count < arrTy->dims[depth]) {
                            for (int i = 0; i < tailSize; ++i) initValues.push_back(zero);
                            count++;
                        }
                    } else {
                        if (auto* ei = dynamic_cast<ExprInitVal*>(iv)) {
                            ei->expr->accept(*this);
                            initValues.push_back(lastValue ? lastValue : zero);
                        } else {
                            initValues.push_back(zero);
                        }
                    }
                };
                flattenValues(node.initVal, 0);
                
                int totalElems = 1;
                for (int d : arrTy->dims) totalElems *= d;
                
                for (int idx = 0; idx < totalElems && idx < static_cast<int>(initValues.size()); ++idx) {
                    std::vector<llvm::Value*> indices;
                    indices.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0));
                    int remaining = idx;
                    for (int d = 0; d < static_cast<int>(arrTy->dims.size()); ++d) {
                        int stride = 1;
                        for (int j = d + 1; j < static_cast<int>(arrTy->dims.size()); ++j) stride *= arrTy->dims[j];
                        int coord = remaining / stride;
                        remaining = remaining % stride;
                        indices.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), coord));
                    }
                    llvm::Value* elemPtr = builder.CreateGEP(nestedTy, alloca, indices, "init.gep");
                    builder.CreateStore(initValues[idx], elemPtr);
                }
            }
            symtab.insert(node.name, SymbolEntry(node.name, t, true, symtab.getCurrentScopeLevel()));
            setLLVMValue(node.name, alloca);
        }
        return;
    }

    
    llvm::Value* initVal = nullptr;
    llvm::Constant* initConst = nullptr;
    if (node.initVal) {
        if (auto* ei = dynamic_cast<ExprInitVal*>(node.initVal)) {
            
            if (t->getKind() == Type::INT && node.dims.empty()) {
                int val = evalDimExpr(ei->expr, constValues);
                ConstValue cv; cv.isScalar = true; cv.scalarVal = val; constValues[node.name] = cv;
            }
            if (symtab.getCurrentScopeLevel() == 0) {
                
                initConst = evalExprToLLVMConstant(ei->expr, constValues, context, ty);
            } else {
                ei->expr->accept(*this);
                initVal = lastValue;
            }
        }
    }

    if (symtab.getCurrentScopeLevel() == 0) {
        if (!initConst) initConst = llvm::ConstantInt::get(ty, 0);
        auto* gv = new llvm::GlobalVariable(module, ty, true, llvm::GlobalValue::ExternalLinkage,
                                            initConst, node.name);
        symtab.insert(node.name, SymbolEntry(node.name, t, true, 0));
        setLLVMValue(node.name, gv);
    } else {
        llvm::Value* alloca = builder.CreateAlloca(ty, nullptr, node.name);
        if (initVal) {
            initVal = createCast(initVal, ty);
            builder.CreateStore(initVal, alloca);
        }
        symtab.insert(node.name, SymbolEntry(node.name, t, true, symtab.getCurrentScopeLevel()));
        setLLVMValue(node.name, alloca);
    }
}

/* 变量定义IR生成：全局变量用GlobalVariable，局部变量用alloca */
void IrBuilder::visit(VarDef& node) {
    Type* t = makeType(node.btype, node.dims, constValues);
    llvm::Type* ty = toLLVMType(t);

    if (t->getKind() == Type::ARRAY) {
        auto* arrTy = static_cast<ArrayType*>(t);
        llvm::Type* nestedTy = getNestedArrayLLVMType(arrTy);

        if (symtab.getCurrentScopeLevel() == 0) {
            
            llvm::Constant* init = llvm::ConstantAggregateZero::get(nestedTy);
            if (node.initVal) {
              std::vector<llvm::Constant*> elems;
              llvm::Constant* zero = (arrTy->baseType->getKind() == Type::FLOAT) ?
                  static_cast<llvm::Constant*>(llvm::ConstantFP::get(arrTy->baseType->toLLVM(context), 0.0)) :
                  static_cast<llvm::Constant*>(llvm::ConstantInt::get(arrTy->baseType->toLLVM(context), 0));
              
              flattenToScalars(node.initVal, elems, arrTy->baseType->toLLVM(context), zero, constValues, context);
              
              int totalElems = 1;
              for (int d : arrTy->dims) totalElems *= d;
              while (static_cast<int>(elems.size()) < totalElems) {
                  elems.push_back(zero);
              }
              
              size_t pos = 0;
              init = buildNestedArray(nestedTy, elems, pos);
            }
            auto* gv = new llvm::GlobalVariable(module, nestedTy, false,
                llvm::GlobalValue::ExternalLinkage, init, node.name);
            symtab.insert(node.name, SymbolEntry(node.name, t, false, 0));
            setLLVMValue(node.name, gv);
        } else {
            
            llvm::Value* alloca = builder.CreateAlloca(nestedTy, nullptr, node.name);
            
            if (node.initVal) {
                std::vector<llvm::Value*> initValues;
                llvm::Value* zero = (arrTy->baseType->getKind() == Type::FLOAT) ?
                    static_cast<llvm::Value*>(llvm::ConstantFP::get(arrTy->baseType->toLLVM(context), 0.0)) :
                    static_cast<llvm::Value*>(llvm::ConstantInt::get(arrTy->baseType->toLLVM(context), 0));
                
                
                std::function<void(InitVal*)> flattenValues = [&](InitVal* iv) {
                    if (auto* av = dynamic_cast<ArrayInitVal*>(iv)) {
                        for (auto v : av->vals) flattenValues(v);
                    } else if (auto* ei = dynamic_cast<ExprInitVal*>(iv)) {
                        ei->expr->accept(*this);
                        initValues.push_back(lastValue ? lastValue : zero);
                    } else {
                        initValues.push_back(zero);
                    }
                };
                flattenValues(node.initVal);
                
                int totalElems = 1;
                for (int d : arrTy->dims) totalElems *= d;
                
                
                while (static_cast<int>(initValues.size()) < totalElems) {
                    initValues.push_back(zero);
                }
                
                
                
                bool allZero = true;
                for (auto* v : initValues) {
                    if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(v)) {
                        if (!ci->isZero()) { allZero = false; break; }
                    } else if (auto* cf = llvm::dyn_cast<llvm::ConstantFP>(v)) {
                        if (!cf->isZero()) { allZero = false; break; }
                    } else {
                        allZero = false; break;
                    }
                }

                if (allZero && !initValues.empty()) {
                    int elemSize = (arrTy->baseType->getKind() == Type::FLOAT) ? 4 : 4;
                    int totalBytes = totalElems * elemSize;
                    llvm::Type* i8Ty = llvm::Type::getInt8Ty(context);
                    llvm::Type* i64Ty = llvm::Type::getInt64Ty(context);
                    llvm::Value* memsetSize = llvm::ConstantInt::get(i64Ty, totalBytes);
                    llvm::Value* zeroI8 = llvm::ConstantInt::get(i8Ty, 0);
                    builder.CreateMemSet(alloca, zeroI8, memsetSize, llvm::MaybeAlign(elemSize));
                } else {
                    
                    for (int idx = 0; idx < totalElems && idx < static_cast<int>(initValues.size()); ++idx) {
                        std::vector<llvm::Value*> indices;
                        indices.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0));
                        int remaining = idx;
                        for (int d = 0; d < static_cast<int>(arrTy->dims.size()); ++d) {
                            int stride = 1;
                            for (int j = d + 1; j < static_cast<int>(arrTy->dims.size()); ++j) stride *= arrTy->dims[j];
                            int coord = remaining / stride;
                            remaining = remaining % stride;
                            indices.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), coord));
                        }
                        llvm::Value* elemPtr = builder.CreateGEP(nestedTy, alloca, indices, "init.gep");
                        builder.CreateStore(initValues[idx], elemPtr);
                    }
                }
                
            }
            symtab.insert(node.name, SymbolEntry(node.name, t, false, symtab.getCurrentScopeLevel()));
            setLLVMValue(node.name, alloca);
        }
        return;
    }

    
    llvm::Value* initVal = nullptr;
    llvm::Constant* initConst = nullptr;
    if (node.initVal) {
        if (auto* ei = dynamic_cast<ExprInitVal*>(node.initVal)) {
            if (symtab.getCurrentScopeLevel() == 0) {
                initConst = evalExprToLLVMConstant(ei->expr, constValues, context, ty);
            } else {
                ei->expr->accept(*this);
                initVal = lastValue;
            }
        }
    }

    if (symtab.getCurrentScopeLevel() == 0) {
        if (!initConst) initConst = llvm::ConstantInt::get(ty, 0);
        auto* gv = new llvm::GlobalVariable(module, ty, false, llvm::GlobalValue::ExternalLinkage,
                                            initConst, node.name);
        symtab.insert(node.name, SymbolEntry(node.name, t, false, 0));
        setLLVMValue(node.name, gv);
    } else {
        llvm::Value* alloca = builder.CreateAlloca(ty, nullptr, node.name);
        if (initVal) {
            initVal = createCast(initVal, ty);
            builder.CreateStore(initVal, alloca);
        }
        symtab.insert(node.name, SymbolEntry(node.name, t, false, symtab.getCurrentScopeLevel()));
        setLLVMValue(node.name, alloca);
    }
}

void IrBuilder::visit(FuncFParam& node) {}

/* if语句IR生成：创建then/else/merge基本块和条件分支 */
void IrBuilder::visit(IfStmt& node) {
    llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(context, "if.then", currentFunction);
    llvm::BasicBlock* elseBB = llvm::BasicBlock::Create(context, "if.else", currentFunction);
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "if.end", currentFunction);

    createCondBr(node.cond, thenBB, elseBB);

    
    builder.SetInsertPoint(thenBB);
    if (node.thenStmt) node.thenStmt->accept(*this);
    if (!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(mergeBB);

    
    builder.SetInsertPoint(elseBB);
    if (node.elseStmt) node.elseStmt->accept(*this);
    if (!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(mergeBB);

    
    builder.SetInsertPoint(mergeBB);
}

/* while语句IR生成：创建cond/body/exit基本块和循环跳转 */
void IrBuilder::visit(WhileStmt& node) {
    llvm::BasicBlock* condBB = llvm::BasicBlock::Create(context, "while.cond", currentFunction);
    llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(context, "while.body", currentFunction);
    llvm::BasicBlock* exitBB = llvm::BasicBlock::Create(context, "while.end", currentFunction);

    loopStack.push_back({condBB, exitBB});

    builder.CreateBr(condBB);

    builder.SetInsertPoint(condBB);
    createCondBr(node.cond, bodyBB, exitBB);

    builder.SetInsertPoint(bodyBB);
    if (node.body) node.body->accept(*this);
    if (!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(condBB);

    builder.SetInsertPoint(exitBB);
    loopStack.pop_back();
}

/* break语句IR生成：跳转到当前循环的exit块 */
void IrBuilder::visit(BreakStmt& node) {
    if (loopStack.empty()) {
        cerr << "[IR Error] break outside loop" << endl;
        hasError = true; return;
    }
    builder.CreateBr(loopStack.back().second);
    llvm::BasicBlock* unreachable = llvm::BasicBlock::Create(context, "break.unreachable", currentFunction);
    builder.SetInsertPoint(unreachable);
}

/* continue语句IR生成：跳转到当前循环的cond块 */
void IrBuilder::visit(ContinueStmt& node) {
    if (loopStack.empty()) {
        cerr << "[IR Error] continue outside loop" << endl;
        hasError = true; return;
    }
    builder.CreateBr(loopStack.back().first);
    llvm::BasicBlock* unreachable = llvm::BasicBlock::Create(context, "continue.unreachable", currentFunction);
    builder.SetInsertPoint(unreachable);
}

/* 赋值语句IR生成：计算地址、求值、类型转换、store */
void IrBuilder::visit(AssignStmt& node) {
    if (!node.lval || !node.expr) return;

    auto* entry = symtab.lookup(node.lval->name, VAR_KIND);
    if (!entry) {
        cerr << "[IR Error] variable '" << node.lval->name << "' not found for assignment" << endl;
        hasError = true; return;
    }

    llvm::Value* ptr = nullptr;
    if (entry->type->getKind() == Type::ARRAY && !node.lval->dims.empty()) {
        ptr = getArrayElementPtr(*node.lval, entry);
    } else {
        ptr = static_cast<llvm::Value*>(entry->llvmValue);
    }

    if (!ptr) {
        cerr << "[IR Error] cannot get address for assignment" << endl;
        hasError = true; return;
    }

    node.expr->accept(*this);
    llvm::Value* val = lastValue;
    if (!val) return;

    
    Type* targetType = entry->type;
    if (entry->type->getKind() == Type::ARRAY && !node.lval->dims.empty()
        && node.lval->dims.size() == static_cast<ArrayType*>(entry->type)->dims.size()) {
        targetType = static_cast<ArrayType*>(entry->type)->baseType;
    }
    val = createCast(val, toLLVMType(targetType));

    builder.CreateStore(val, ptr);
}

void IrBuilder::visit(ExpStmt& node) {
    if (node.expr) node.expr->accept(*this);
}

/* return语句IR生成：创建ret指令和不可达基本块 */
void IrBuilder::visit(ReturnStmt& node) {
    if (node.expr) {
        node.expr->accept(*this);
        llvm::Value* val = lastValue;
        if (val) {
            
            if (currentFuncRetType) {
                llvm::Type* retLLVM = toLLVMType(currentFuncRetType);
                val = createCast(val, retLLVM);
            }
            builder.CreateRet(val);
        } else {
            builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0));
        }
    } else {
        if (currentFuncRetType && currentFuncRetType->getKind() == Type::VOID) {
            builder.CreateRetVoid();
        } else {
            builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0));
        }
    }
    
    llvm::BasicBlock* unreachable = llvm::BasicBlock::Create(context, "unreachable", currentFunction);
    builder.SetInsertPoint(unreachable);
}

/* 左值IR生成：变量load或数组元素GEP+load */
void IrBuilder::visit(LVal& node) {
    auto* entry = symtab.lookup(node.name, VAR_KIND);
    if (!entry) {
        cerr << "[IR Error] undefined variable '" << node.name << "'" << endl;
        hasError = true; lastValue = nullptr; return;
    }

    llvm::Value* ptr = static_cast<llvm::Value*>(entry->llvmValue);
    if (!ptr) {
        cerr << "[IR Error] variable '" << node.name << "' has no LLVM value" << endl;
        hasError = true; lastValue = nullptr; return;
    }

    if (entry->type->getKind() == Type::ARRAY) {
        auto* arrTy = static_cast<ArrayType*>(entry->type);
        if (node.dims.empty()) {
            
            lastValue = ptr;
        } else {
            llvm::Value* elemPtr = getArrayElementPtr(node, entry);
            if (!elemPtr) {
                hasError = true; lastValue = nullptr; return;
            }
            
            if (node.dims.size() == arrTy->dims.size()) {
                lastValue = createLoad(elemPtr, arrTy->baseType);
            } else {
                lastValue = elemPtr;
            }
        }
    } else {
        if (!node.dims.empty()) {
            cerr << "[IR Error] '" << node.name << "' is not an array" << endl;
            hasError = true; lastValue = nullptr; return;
        }
        lastValue = createLoad(ptr, entry->type);
    }
}

/* 数字字面量IR生成：创建ConstantInt或ConstantFP */
void IrBuilder::visit(Number& node) {
    if (node.isFloat) lastValue = llvm::ConstantFP::get(llvm::Type::getFloatTy(context), node.fVal);
    else lastValue = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), node.iVal);
}

/* 二元表达式IR生成：支持算术、关系和逻辑运算，含短路求值 */
void IrBuilder::visit(BinaryExp& node) {
    
    if (node.op == "&&" || node.op == "||") {
        if (!currentFunction) {
            cerr << "[IR Error] && || outside function" << endl;
            hasError = true; lastValue = nullptr; return;
        }
        
        node.lhs->accept(*this);
        llvm::Value* L = lastValue;
        if (!L) { lastValue = nullptr; return; }
        
        llvm::Value* lhsBool = nullptr;
        if (L->getType()->isFloatTy()) {
            lhsBool = builder.CreateFCmpONE(L, llvm::ConstantFP::get(L->getType(), 0.0), "cmp");
        } else {
            lhsBool = builder.CreateICmpNE(L, llvm::ConstantInt::get(L->getType(), 0), "cmp");
        }
        
        llvm::BasicBlock* lhsBB = builder.GetInsertBlock();
        llvm::BasicBlock* rhsBB = llvm::BasicBlock::Create(context, 
            node.op == "&&" ? "land.rhs" : "lor.rhs", currentFunction);
        llvm::BasicBlock* endBB = llvm::BasicBlock::Create(context,
            node.op == "&&" ? "land.end" : "lor.end", currentFunction);
        
        if (node.op == "&&") {
            builder.CreateCondBr(lhsBool, rhsBB, endBB);
        } else {
            builder.CreateCondBr(lhsBool, endBB, rhsBB);
        }
        
        builder.SetInsertPoint(rhsBB);
        node.rhs->accept(*this);
        llvm::Value* R = lastValue;
        llvm::Value* rhsBool = nullptr;
        if (R) {
            if (R->getType()->isFloatTy()) {
                rhsBool = builder.CreateFCmpONE(R, llvm::ConstantFP::get(R->getType(), 0.0), "cmp");
            } else {
                rhsBool = builder.CreateICmpNE(R, llvm::ConstantInt::get(R->getType(), 0), "cmp");
            }
        } else {
            rhsBool = llvm::ConstantInt::getFalse(context);
        }
        builder.CreateBr(endBB);
        
        builder.SetInsertPoint(endBB);
        llvm::PHINode* phi = builder.CreatePHI(llvm::Type::getInt1Ty(context), 2, "phi");
        if (node.op == "&&") {
            phi->addIncoming(llvm::ConstantInt::getFalse(context), lhsBB);
        } else {
            phi->addIncoming(llvm::ConstantInt::getTrue(context), lhsBB);
        }
        phi->addIncoming(rhsBool, rhsBB);
        lastValue = builder.CreateZExt(phi, llvm::Type::getInt32Ty(context), "zext");
        return;
    }

    node.lhs->accept(*this);
    llvm::Value* L = lastValue;
    node.rhs->accept(*this);
    llvm::Value* R = lastValue;

    if (!L || !R) { lastValue = nullptr; return; }
    
    
    if (L->getType()->isFloatTy() && R->getType()->isIntegerTy()) {
        R = builder.CreateSIToFP(R, L->getType(), "sitofp");
    } else if (R->getType()->isFloatTy() && L->getType()->isIntegerTy()) {
        L = builder.CreateSIToFP(L, R->getType(), "sitofp");
    }

    bool isFloat = L->getType()->isFloatTy() || R->getType()->isFloatTy();

    if (node.op == "+") {
        if (isFloat) lastValue = builder.CreateFAdd(L, R, "fadd");
        else lastValue = builder.CreateAdd(L, R, "add");
    } else if (node.op == "-") {
        if (isFloat) lastValue = builder.CreateFSub(L, R, "fsub");
        else lastValue = builder.CreateSub(L, R, "sub");
    } else if (node.op == "*") {
        if (isFloat) lastValue = builder.CreateFMul(L, R, "fmul");
        else lastValue = builder.CreateMul(L, R, "mul");
    } else if (node.op == "/") {
        if (isFloat) lastValue = builder.CreateFDiv(L, R, "fdiv");
        else lastValue = builder.CreateSDiv(L, R, "div");
    } else if (node.op == "%") {
        lastValue = builder.CreateSRem(L, R, "mod");
    } else if (node.op == "<") {
        if (isFloat) lastValue = builder.CreateFCmpOLT(L, R, "lt");
        else lastValue = builder.CreateICmpSLT(L, R, "lt");
        lastValue = builder.CreateZExt(lastValue, llvm::Type::getInt32Ty(context), "zext");
    } else if (node.op == ">") {
        if (isFloat) lastValue = builder.CreateFCmpOGT(L, R, "gt");
        else lastValue = builder.CreateICmpSGT(L, R, "gt");
        lastValue = builder.CreateZExt(lastValue, llvm::Type::getInt32Ty(context), "zext");
    } else if (node.op == "<=") {
        if (isFloat) lastValue = builder.CreateFCmpOLE(L, R, "le");
        else lastValue = builder.CreateICmpSLE(L, R, "le");
        lastValue = builder.CreateZExt(lastValue, llvm::Type::getInt32Ty(context), "zext");
    } else if (node.op == ">=") {
        if (isFloat) lastValue = builder.CreateFCmpOGE(L, R, "ge");
        else lastValue = builder.CreateICmpSGE(L, R, "ge");
        lastValue = builder.CreateZExt(lastValue, llvm::Type::getInt32Ty(context), "zext");
    } else if (node.op == "==") {
        if (isFloat) lastValue = builder.CreateFCmpOEQ(L, R, "eq");
        else lastValue = builder.CreateICmpEQ(L, R, "eq");
        lastValue = builder.CreateZExt(lastValue, llvm::Type::getInt32Ty(context), "zext");
    } else if (node.op == "!=") {
        if (isFloat) lastValue = builder.CreateFCmpONE(L, R, "ne");
        else lastValue = builder.CreateICmpNE(L, R, "ne");
        lastValue = builder.CreateZExt(lastValue, llvm::Type::getInt32Ty(context), "zext");
    } else {
        lastValue = nullptr;
    }
}

/* 一元表达式IR生成：支持负号、逻辑非 */
void IrBuilder::visit(UnaryExp& node) {
    node.operand->accept(*this);
    llvm::Value* V = lastValue;
    if (!V) { lastValue = nullptr; return; }

    if (node.op == "-") {
        if (V->getType()->isFloatTy()) lastValue = builder.CreateFNeg(V, "fneg");
        else lastValue = builder.CreateNeg(V, "neg");
    } else if (node.op == "!") {
        llvm::Value* cmp = nullptr;
        if (V->getType()->isFloatTy()) {
            cmp = builder.CreateFCmpOEQ(V, llvm::ConstantFP::get(V->getType(), 0.0), "not");
        } else {
            llvm::Value* zero = llvm::ConstantInt::get(V->getType(), 0);
            cmp = builder.CreateICmpEQ(V, zero, "not");
        }
        lastValue = builder.CreateZExt(cmp, llvm::Type::getInt32Ty(context), "zext");
    } else {
        lastValue = V;
    }
}

/* 函数调用IR生成：参数类型转换、生成call指令 */
void IrBuilder::visit(CallExp& node) {
    auto* entry = symtab.lookup(node.funcName, FUNC_KIND);
    if (!entry || entry->type->getKind() != Type::FUNCTION) {
        cerr << "[IR Error] undefined function '" << node.funcName << "'" << endl;
        hasError = true; lastValue = nullptr; return;
    }

    auto* ft = static_cast<FunctionType*>(entry->type);
    llvm::Value* funcVal = static_cast<llvm::Value*>(entry->llvmValue);
    if (!funcVal) {
        cerr << "[IR Error] function '" << node.funcName << "' has no LLVM value" << endl;
        hasError = true; lastValue = nullptr; return;
    }

    llvm::Function* callee = llvm::dyn_cast<llvm::Function>(funcVal);
    if (!callee) {
        hasError = true; lastValue = nullptr; return;
    }

    std::vector<llvm::Value*> args;
    for (size_t i = 0; i < node.args.size(); ++i) {
        node.args[i]->accept(*this);
        llvm::Value* argVal = lastValue;
        if (!argVal) { hasError = true; return; }

        
        if (i < ft->paramTypes.size()) {
            llvm::Type* paramLLVM = toLLVMType(ft->paramTypes[i]);
            argVal = createCast(argVal, paramLLVM);
        }
        args.push_back(argVal);
    }

    llvm::Value* call;
    if (callee->getReturnType()->isVoidTy()) {
        call = builder.CreateCall(callee, args);   
    } else {
        call = builder.CreateCall(callee, args, "call");
    }
    lastValue = call;
}

void IrBuilder::visit(ExprInitVal& node) {
    if (node.expr) {
        node.expr->accept(*this);
    } else {
        lastValue = nullptr;
    }
}

void IrBuilder::visit(ArrayInitVal& node) {
    lastValue = nullptr;
}

static std::string unescapeString(const std::string& raw) {
    std::string result;
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\\' && i + 1 < raw.size()) {
            switch (raw[i+1]) {
                case 'n': result += '\n'; i++; break;
                case 't': result += '\t'; i++; break;
                case '\\': result += '\\'; i++; break;
                case '"': result += '"'; i++; break;
                case '0': result += '\0'; i++; break;
                default: result += raw[i]; break;
            }
        } else {
            result += raw[i];
        }
    }
    return result;
}

/* 字符串字面量IR生成：创建全局字符串常量和GEP指针 */
void IrBuilder::visit(StringLiteral& node) {
    std::string raw = node.value;
    std::string content;
    if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
        content = raw.substr(1, raw.size() - 2);
    } else {
        content = raw;
    }
    content = unescapeString(content);   
    llvm::Constant* strConst = llvm::ConstantDataArray::getString(context, content, true);
    llvm::GlobalVariable* gv = new llvm::GlobalVariable(
        module, strConst->getType(), true,
        llvm::GlobalValue::PrivateLinkage, strConst, ".str");
    llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
    lastValue = builder.CreateGEP(strConst->getType(), gv, {zero, zero}, "str.ptr");
}

void IrBuilder::visit(DeclItem& node) {
    if (node.decl) node.decl->accept(*this);
}

void IrBuilder::visit(StmtItem& node) {
    if (node.stmt) node.stmt->accept(*this);
}