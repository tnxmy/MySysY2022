/* ============================================================================
 * 文件名称: main.cpp
 * 功能描述: 编译器主入口，负责命令行解析、词法分析、语法分析、语义分析和代码生成
 * 作者信息: SysY2022 Compiler Team
 * 版本信息: 1.0
 * 所属项目: SysY2022 编译器
 * ============================================================================ */

#include <iostream>
#include <string>
#include <fstream>
#include <cstdio>
#include <vector>
#include <map>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/TargetSelect.h>

#include "sysy.tab.hpp"
#include "Ast.h"
#include "Type.h"
#include "SymbolTable.h"
#include "SemanticAnalyzer.h"
#include "IrBuilder.h"

extern "C" {
    extern int yylex(void);
    extern FILE* yyin;
}

extern AstNode* root;
extern YYSTYPE yylval;

/* 词法记号名称映射表，用于调试输出 */
static const char* tokenName(int tok) {
    switch (tok) {
        case INT: return "INT"; case FLOAT: return "FLOAT"; case VOID: return "VOID";
        case CONST: return "CONST"; case IF: return "IF"; case ELSE: return "ELSE";
        case WHILE: return "WHILE"; case BREAK: return "BREAK"; case CONTINUE: return "CONTINUE";
        case RETURN: return "RETURN"; case IDENT: return "IDENT"; case INT_CONST: return "INT_CONST";
        case FLOAT_CONST: return "FLOAT_CONST"; case PLUS: return "PLUS"; case MINUS: return "MINUS";
        case MUL: return "MUL"; case DIV: return "DIV"; case MOD: return "MOD";
        case ASSIGN: return "ASSIGN"; case EQ: return "EQ"; case NEQ: return "NEQ";
        case LT: return "LT"; case GT: return "GT"; case LE: return "LE"; case GE: return "GE";
        case AND: return "AND"; case OR: return "OR"; case NOT: return "NOT";
        case SEMICOLON: return "SEMICOLON"; case COMMA: return "COMMA";
        case LPAREN: return "LPAREN"; case RPAREN: return "RPAREN";
        case LBRACKET: return "LBRACKET"; case RBRACKET: return "RBRACKET";
        case LBRACE: return "LBRACE"; case RBRACE: return "RBRACE";
        case ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

/* 词法分析调试：逐个读取并打印所有token */
static void dumpTokens(FILE* f) {
    yyin = f;
    int tok;
    while ((tok = yylex()) != 0) {
        printf("%s", tokenName(tok));
        if (tok == IDENT) { printf("(%s)", yylval.str); free(yylval.str); }
        else if (tok == INT_CONST) printf("(%d)", yylval.num);
        else if (tok == FLOAT_CONST) printf("%f", yylval.fnum);
        printf("\n");
    }
}

/* 打印编译器版本和构建信息 */
static void printVersion() {
    std::cout << "Target:    RISC-V 64 (generic-rv64)" << std::endl;
    std::cout << "Backend:   LLVM" << std::endl;
    std::cout << "Frontend:  Flex + Bison" << std::endl;
    std::cout << "SysY2022 Compiler (Stage 10 / 14)" << std::endl;
    std::cout << "Stage:     RISC-V Assembly Generation" << std::endl;
}

/* 打印命令行使用说明 */
static void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " [options] input.sy" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -o <file>          Output assembly to <file>" << std::endl;
    std::cout << "  --version          Print version info" << std::endl;
    std::cout << "  --dump-tokens      Dump token stream" << std::endl;
    std::cout << "  --dump-ast         Dump AST" << std::endl;
    std::cout << "  --dump-symbols     Dump symbol table" << std::endl;
    std::cout << "  --check-semantics  Run semantic analysis" << std::endl;
    std::cout << "  --emit-llvm        Generate LLVM IR (.ll)" << std::endl;
    std::cout << "  -h, --help         Show help" << std::endl;
}

/* 主函数：解析参数、驱动编译流程、输出目标代码 */
int main(int argc, char** argv) {
    if (argc < 2) { printUsage(argv[0]); return 1; }

    
        /* 初始化LLVM所有目标平台信息（阶段10） */
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmPrinters();

    std::string inputFile, outputFile = "output.s";
    bool dumpTokensFlag = false, dumpAst = false, dumpSymbols = false;
    bool checkSemantics = false, emitLLVM = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--version") { printVersion(); return 0; }
        else if (arg == "-h" || arg == "--help") { printUsage(argv[0]); return 0; }
        else if (arg == "-o" && i + 1 < argc) outputFile = argv[++i];
        else if (arg == "--dump-tokens") dumpTokensFlag = true;
        else if (arg == "--dump-ast") dumpAst = true;
        else if (arg == "--dump-symbols") dumpSymbols = true;
        else if (arg == "--check-semantics") checkSemantics = true;
        else if (arg == "--emit-llvm") emitLLVM = true;
        else if (arg[0] != '-') inputFile = arg;
    }

    if (inputFile.empty()) { std::cerr << "[ERROR] No input file." << std::endl; return 1; }

    
    if (dumpTokensFlag) {
        FILE* f = std::fopen(inputFile.c_str(), "r");
        if (!f) { std::cerr << "[ERROR] Cannot open " << inputFile << std::endl; return 1; }
        dumpTokens(f); std::fclose(f); return 0;
    }

    
    FILE* f = std::fopen(inputFile.c_str(), "r");
    if (!f) { std::cerr << "[ERROR] Cannot open " << inputFile << std::endl; return 1; }
    yyin = f;
    int parseResult = yyparse();
    std::fclose(f);
    if (parseResult != 0 || !root) { std::cerr << "[ERROR] Parsing failed." << std::endl; return 1; }

    if (dumpAst) {
        std::cout << "=== AST Dump ===" << std::endl;
        DumpVisitor dumper; root->accept(dumper); return 0;
    }

    
        /* 阶段5：语义分析，构建符号表并检查类型 */
    SemanticAnalyzer analyzer;
    root->accept(analyzer);

    if (checkSemantics || dumpSymbols) {
        if (dumpSymbols) {
            std::cout << "=== Symbol Table ===" << std::endl;
            analyzer.symtab.dump();
        }
        if (analyzer.hasError) {
            std::cerr << "[FAILED] Semantic errors found." << std::endl;
            return 1;
        }
        if (checkSemantics) {
            std::cout << "[OK] Semantic analysis passed." << std::endl;
        }
        return 0;
    }

    
    if (analyzer.hasError) {
        std::cerr << "[ERROR] Semantic analysis failed." << std::endl;
        return 1;
    }

    
        /* 阶段6-10：IR生成、优化和RISC-V汇编输出 */
    IrBuilder irb;
    irb.constValues = analyzer.constValues;
    if (!irb.generateIR(root)) {
        std::cerr << "[ERROR] IR generation failed." << std::endl;
        return 1;
    }

    
    llvm::Function* userMain = irb.module.getFunction("main");
    if (userMain) {
        userMain->setName("__sysy_main");
        llvm::FunctionType* mainTy = llvm::FunctionType::get(
            llvm::Type::getInt32Ty(irb.context), false);
        llvm::Function* wrapper = llvm::Function::Create(
            mainTy, llvm::Function::ExternalLinkage, "main", irb.module);
        llvm::BasicBlock* bb = llvm::BasicBlock::Create(irb.context, "entry", wrapper);
        llvm::IRBuilder<> wrapBuilder(bb);
        llvm::Value* ret = wrapBuilder.CreateCall(userMain, {}, "ret");
        
        if (llvm::Function* putintFunc = irb.module.getFunction("putint")) {
            wrapBuilder.CreateCall(putintFunc, {ret});
        }
        if (llvm::Function* putchFunc = irb.module.getFunction("putch")) {
            wrapBuilder.CreateCall(putchFunc, {
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(irb.context), 10)});
        }
        wrapBuilder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(irb.context), 0));
    }

    if (emitLLVM) {
        irb.printIR(std::cout);
        return 0;
    }

    
    irb.optimize();
    if (!irb.emitAssembly(outputFile)) {
        std::cerr << "[ERROR] Assembly generation failed." << std::endl;
        return 1;
    }
    std::cout << "[OK] Assembly written to " << outputFile << std::endl;
    return 0;
}