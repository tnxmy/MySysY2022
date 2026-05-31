#ifndef IR_BUILDER_H
#define IR_BUILDER_H
/* ============================================================================
 * 文件名称: IrBuilder.h
 * 功能描述: LLVM IR生成器类声明，负责将AST转换为LLVM中间表示
 * 作者信息: SysY2022 Compiler Team
 * 版本信息: 1.0
 * 所属项目: SysY2022 编译器
 * ============================================================================ */


#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/DerivedTypes.h>
#include "Ast.h"
#include "Type.h"
#include "SymbolTable.h"
#include "SemanticAnalyzer.h"

/* IR生成器：遍历AST生成LLVM IR，支持优化和RISC-V汇编输出 */
class IrBuilder : public Visitor {
public:
    llvm::LLVMContext context;
    llvm::Module module;
    llvm::IRBuilder<> builder;
    SymbolTable symtab;
    bool hasError = false;
    std::map<std::string, ConstValue> constValues;

    llvm::Function* currentFunction = nullptr;
    Type* currentFuncRetType = nullptr;
    llvm::Value* lastValue = nullptr;

    IrBuilder() : module("sysy_module", context), builder(context) {}

    bool generateIR(AstNode* root);
    void printIR(std::ostream& out);
    bool optimize();
    bool emitAssembly(const std::string& filename);

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

    void declareSysYLib();

private:
    llvm::Value* getLLVMValue(const std::string& name);
    void setLLVMValue(const std::string& name, llvm::Value* val);
    llvm::Type* toLLVMType(Type* ty);
    llvm::Value* createLoad(llvm::Value* ptr, Type* ty);

    
    std::vector<std::pair<llvm::BasicBlock*, llvm::BasicBlock*>> loopStack;

    
    void createCondBr(Expr* cond, llvm::BasicBlock* trueBB, llvm::BasicBlock* falseBB);
    
    
    llvm::Value* createCast(llvm::Value* val, llvm::Type* targetLLVM);
    llvm::Type* getNestedArrayLLVMType(ArrayType* arrTy);
    llvm::Value* getArrayElementPtr(LVal& node, SymbolEntry* entry);
};

#endif 
