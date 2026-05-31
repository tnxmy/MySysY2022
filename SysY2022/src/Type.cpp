/* ============================================================================
 * 文件名称: Type.cpp
 * 功能描述: 类型系统实现，定义SysY类型到LLVM IR类型的映射
 * 作者信息: SysY2022 Compiler Team
 * 版本信息: 1.0
 * 所属项目: SysY2022 编译器
 * ============================================================================ */

#include "Type.h"
#include <llvm/IR/DerivedTypes.h>

using namespace std;

/* 将SysY int类型映射为LLVM i32类型 */
llvm::Type* IntType::toLLVM(llvm::LLVMContext& ctx) const {
    return llvm::Type::getInt32Ty(ctx);
}
/* 判断另一类型是否为INT */
bool IntType::equals(const Type* other) const {
    return other && other->getKind() == INT;
}
/* 将SysY float类型映射为LLVM float类型 */
llvm::Type* FloatType::toLLVM(llvm::LLVMContext& ctx) const {
    return llvm::Type::getFloatTy(ctx);
}
/* 判断另一类型是否为FLOAT */
bool FloatType::equals(const Type* other) const {
    return other && other->getKind() == FLOAT;
}
/* 将SysY void类型映射为LLVM void类型 */
llvm::Type* VoidType::toLLVM(llvm::LLVMContext& ctx) const {
    return llvm::Type::getVoidTy(ctx);
}
/* 判断另一类型是否为VOID */
bool VoidType::equals(const Type* other) const {
    return other && other->getKind() == VOID;
}
/* 返回数组类型的字符串表示，如 int[3][4] */
string ArrayType::toString() const {
    string s = baseType->toString();
    for (int d : dims) {
        if (d >= 0) s += "[" + to_string(d) + "]";
        else s += "[]";
    }
    return s;
}
/* 将SysY数组类型映射为LLVM嵌套数组类型 [d0 x [d1 x ... x base]] */
llvm::Type* ArrayType::toLLVM(llvm::LLVMContext& ctx) const {
    
    return baseType->toLLVM(ctx);
}
/* 判断数组类型是否维度相同且基类型等价 */
bool ArrayType::equals(const Type* other) const {
    if (!other || other->getKind() != ARRAY) return false;
    auto* o = static_cast<const ArrayType*>(other);
    if (!baseType->equals(o->baseType)) return false;
    if (dims.size() != o->dims.size()) return false;
    for (size_t i = 0; i < dims.size(); ++i) {
        if (dims[i] != o->dims[i]) return false;
    }
    return true;
}
/* 返回函数类型的字符串表示 */
string FunctionType::toString() const {
    string s = retType->toString() + "(";
    for (size_t i = 0; i < paramTypes.size(); ++i) {
        if (i > 0) s += ", ";
        s += paramTypes[i]->toString();
    }
    s += ")";
    return s;
}
/* 判断函数类型是否返回类型和参数列表均相同 */
bool FunctionType::equals(const Type* other) const {
    if (!other || other->getKind() != FUNCTION) return false;
    auto* o = static_cast<const FunctionType*>(other);
    if (!retType->equals(o->retType)) return false;
    if (paramTypes.size() != o->paramTypes.size()) return false;
    for (size_t i = 0; i < paramTypes.size(); ++i) {
        if (!paramTypes[i]->equals(o->paramTypes[i])) return false;
    }
    return true;
}

static IntType intTypeInstance;
static FloatType floatTypeInstance;
static VoidType voidTypeInstance;

Type* getIntType()   { return &intTypeInstance; }
Type* getFloatType() { return &floatTypeInstance; }
Type* getVoidType()  { return &voidTypeInstance; }
