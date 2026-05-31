/* ============================================================================
 * 文件名称: SymbolTable.cpp
 * 功能描述: 符号表管理实现，支持嵌套作用域的符号插入和查找
 *           函数与变量分属不同命名空间，同名不同类不视为重定义
 * 作者信息: SysY2022 Compiler Team
 * 版本信息: 1.1
 * 所属项目: SysY2022 编译器
 * ============================================================================ */

#include "SymbolTable.h"

using namespace std;

/* 构造函数：初始化全局作用域栈 */
SymbolTable::SymbolTable() {
    allScopes.emplace_back(); 
    activeScopes.push_back(0);
}

/* 进入新作用域：创建新的符号映射并压入活跃栈 */
void SymbolTable::enterScope() {
    allScopes.emplace_back();
    activeScopes.push_back(allScopes.size() - 1);
}

/* 退出当前作用域：从活跃栈弹出当前作用域索引 */
void SymbolTable::exitScope() {
    if (activeScopes.size() > 1) {
        activeScopes.pop_back();
    }
}

/* 插入符号到当前作用域，同名且同类别的符号视为重复定义 */
bool SymbolTable::insert(const string& name, SymbolEntry entry) {
    auto& current = allScopes[activeScopes.back()];
    auto range = current.equal_range(name);
    if (entry.scopeLevel == 0) {
    if (range.first != range.second) return false;  // 全局同名即重复
    } else {
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second.kind == entry.kind) return false;
        }
    }
    current.insert({name, entry});
    return true;
}

/* 按名称查找符号：从内层到外层作用域逐层搜索，返回第一个匹配的（不区分类别） */
SymbolEntry* SymbolTable::lookup(const string& name) {
    for (auto it = activeScopes.rbegin(); it != activeScopes.rend(); ++it) {
        auto& scope = allScopes[*it];
        auto range = scope.equal_range(name);
        for (auto fit = range.first; fit != range.second; ++fit) {
            return &(const_cast<SymbolEntry&>(fit->second));
        }
    }
    return nullptr;
}

/* 按名称和类别查找符号：从内层到外层作用域逐层搜索 */
SymbolEntry* SymbolTable::lookup(const string& name, SymbolKind kind) {
    for (auto it = activeScopes.rbegin(); it != activeScopes.rend(); ++it) {
        auto& scope = allScopes[*it];
        auto range = scope.equal_range(name);
        for (auto fit = range.first; fit != range.second; ++fit) {
            if (fit->second.kind == kind) {
                return &(const_cast<SymbolEntry&>(fit->second));
            }
        }
    }
    return nullptr;
}

/* 按名称查找当前作用域符号（不区分类别） */
SymbolEntry* SymbolTable::lookupCurrent(const string& name) {
    auto& current = allScopes[activeScopes.back()];
    auto range = current.equal_range(name);
    for (auto fit = range.first; fit != range.second; ++fit) {
        return &(const_cast<SymbolEntry&>(fit->second));
    }
    return nullptr;
}

/* 按名称和类别查找当前作用域符号 */
SymbolEntry* SymbolTable::lookupCurrent(const string& name, SymbolKind kind) {
    auto& current = allScopes[activeScopes.back()];
    auto range = current.equal_range(name);
    for (auto fit = range.first; fit != range.second; ++fit) {
        if (fit->second.kind == kind) {
            return &(const_cast<SymbolEntry&>(fit->second));
        }
    }
    return nullptr;
}

/* 获取当前作用域层级，0表示全局作用域 */
int SymbolTable::getCurrentScopeLevel() const {
    return static_cast<int>(activeScopes.size()) - 1;
}

/* 打印符号表：遍历所有作用域输出符号名称、类别和类型 */
void SymbolTable::dump(ostream& out) const {
    out << "=== Symbol Table ===" << endl;
    for (size_t i = 0; i < allScopes.size(); ++i) {
        out << "Scope " << i << ":" << endl;
        for (const auto& pair : allScopes[i]) {
            const auto& entry = pair.second;
            string kindStr = (entry.kind == FUNC_KIND) ? "func" : "var";
            out << "  " << entry.name << " : " << entry.type->toString()
                << (entry.isConst ? " (const)" : "")
                << " [" << kindStr << "]"
                << " [level " << entry.scopeLevel << "]" << endl;
        }
    }
}

void SymbolTable::swap(SymbolTable& other) {
    allScopes.swap(other.allScopes);
    activeScopes.swap(other.activeScopes);
}
