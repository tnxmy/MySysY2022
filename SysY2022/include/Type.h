#ifndef TYPE_H
#define TYPE_H
/* ============================================================================
 * 文件名称: Type.h
 * 功能描述: 类型系统定义，包含SysY基础类型和LLVM IR类型的映射关系
 * 作者信息: SysY2022 Compiler Team
 * 版本信息: 1.0
 * 所属项目: SysY2022 编译器
 * ============================================================================ */


#include <string>
#include <vector>
#include <llvm/IR/Type.h>
#include <llvm/IR/LLVMContext.h>

/* 类型系统抽象基类，定义所有类型的通用接口 */
class Type {
public:
        /* 类型枚举：INT(32位整数), FLOAT(32位浮点), VOID(无返回), ARRAY(数组), FUNCTION(函数) */
    enum Kind { INT, FLOAT, VOID, ARRAY, FUNCTION };
    virtual ~Type() = default;
    virtual Kind getKind() const = 0;
    virtual std::string toString() const = 0;
    virtual llvm::Type* toLLVM(llvm::LLVMContext& ctx) const = 0;
    virtual bool equals(const Type* other) const = 0;
};

/* 32位有符号整数类型，对应LLVM i32 */
class IntType : public Type {
public:
    Kind getKind() const override { return INT; }
    std::string toString() const override { return "int"; }
    llvm::Type* toLLVM(llvm::LLVMContext& ctx) const override;
    bool equals(const Type* other) const override;
};

/* 32位单精度浮点类型，对应LLVM float */
class FloatType : public Type {
public:
    Kind getKind() const override { return FLOAT; }
    std::string toString() const override { return "float"; }
    llvm::Type* toLLVM(llvm::LLVMContext& ctx) const override;
    bool equals(const Type* other) const override;
};

/* 无返回类型，对应LLVM void，用于无返回值函数 */
class VoidType : public Type {
public:
    Kind getKind() const override { return VOID; }
    std::string toString() const override { return "void"; }
    llvm::Type* toLLVM(llvm::LLVMContext& ctx) const override;
    bool equals(const Type* other) const override;
};

/* 数组类型，包含基类型和维度列表，支持多维数组 */
class ArrayType : public Type {
public:
    Type* baseType;
    std::vector<int> dims; 

    ArrayType(Type* base, const std::vector<int>& d) : baseType(base), dims(d) {}
    Kind getKind() const override { return ARRAY; }
    std::string toString() const override;
    llvm::Type* toLLVM(llvm::LLVMContext& ctx) const override;
    bool equals(const Type* other) const override;
};

/* 函数类型，包含返回类型和参数类型列表 */
class FunctionType : public Type {
public:
    Type* retType;
    std::vector<Type*> paramTypes;

    FunctionType(Type* ret, const std::vector<Type*>& params) : retType(ret), paramTypes(params) {}
    Kind getKind() const override { return FUNCTION; }
    std::string toString() const override;
    llvm::Type* toLLVM(llvm::LLVMContext& ctx) const override { return nullptr; }
    bool equals(const Type* other) const override;
};

Type* getIntType();
Type* getFloatType();
Type* getVoidType();

#endif 
