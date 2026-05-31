#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H
/* ============================================================================
 * 文件名称: SymbolTable.h
 * 功能描述: 符号表定义，管理变量/常量/函数的作用域和属性信息
 * 作者信息: SysY2022 Compiler Team
 * 版本信息: 1.1  (支持函数与变量命名空间分离)
 * 所属项目: SysY2022 编译器
 * ============================================================================ */


#include <string>
#include <vector>
#include <map>
#include <iostream>
#include "Type.h"

/* 符号类别：变量/常量 或 函数，用于支持同名不同类的符号共存 */
enum SymbolKind { VAR_KIND, FUNC_KIND };

/* 符号表条目，记录标识符的名称、类型、常量属性、作用域层级、LLVM值和符号类别 */
struct SymbolEntry {
    std::string name;
    Type* type;
    bool isConst;
    int scopeLevel;
    void* llvmValue;
    SymbolKind kind;

    SymbolEntry() : type(nullptr), isConst(false), scopeLevel(0), llvmValue(nullptr), kind(VAR_KIND) {}
    SymbolEntry(const std::string& n, Type* t, bool c, int level, SymbolKind k = VAR_KIND)
        : name(n), type(t), isConst(c), scopeLevel(level), llvmValue(nullptr), kind(k) {}
};

/* 符号表管理类，支持嵌套作用域的进入/退出和符号查找，函数与变量分属不同命名空间 */
class SymbolTable {
public:
        /* 构造函数，初始化全局作用域 */
    SymbolTable();
        /* 进入新的作用域（如函数体、语句块） */
    void enterScope();
        /* 退出当前作用域，清理局部符号 */
    void exitScope();
        /* 向当前作用域插入符号，若同名同类别则返回false */
    bool insert(const std::string& name, SymbolEntry entry);
        /* 按名称查找符号（不区分类别，向后兼容，返回第一个匹配的） */
    SymbolEntry* lookup(const std::string& name);
        /* 按名称和类别查找符号，优先搜索内层作用域 */
    SymbolEntry* lookup(const std::string& name, SymbolKind kind);
        /* 按名称查找当前作用域符号（不区分类别） */
    SymbolEntry* lookupCurrent(const std::string& name);
        /* 按名称和类别查找当前作用域符号 */
    SymbolEntry* lookupCurrent(const std::string& name, SymbolKind kind);
        /* 获取当前作用域层级，0为全局作用域 */
    int getCurrentScopeLevel() const;
        /* 打印所有作用域中的符号信息，用于调试 */
    void dump(std::ostream& out = std::cout) const;
    void swap(SymbolTable& other);

private:
    std::vector<std::multimap<std::string, SymbolEntry>> allScopes;
    std::vector<size_t> activeScopes;
};

#endif 
