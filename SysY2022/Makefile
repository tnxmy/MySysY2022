# ==============================================================================
# SysY2022 Compiler - Makefile (Stage 6)
# ==============================================================================

CXX = g++
CC  = cc

LLVM_CONFIG := $(shell which llvm-config-21 2>/dev/null || which llvm-config 2>/dev/null)

ifeq ($(LLVM_CONFIG),)
  $(error "llvm-config not found. Please install llvm-dev.")
endif

LLVM_CXXFLAGS := $(shell $(LLVM_CONFIG) --cxxflags)
LLVM_LDFLAGS  := $(shell $(LLVM_CONFIG) --ldflags)
LLVM_LIBS     := $(shell $(LLVM_CONFIG) --libs)
LLVM_SYS_LIBS := $(shell $(LLVM_CONFIG) --system-libs)
LLVM_INC      := $(shell $(LLVM_CONFIG) --includedir)

SRC_DIR   = src
INC_DIR   = include
BUILD_DIR = build
OBJ_DIR   = $(BUILD_DIR)/obj

INCLUDES = -I$(INC_DIR) -I$(BUILD_DIR) -I$(LLVM_INC)

CXXFLAGS = $(LLVM_CXXFLAGS) $(INCLUDES)
CFLAGS   = $(INCLUDES)

LDFLAGS = $(LLVM_LDFLAGS) $(LLVM_LIBS) $(LLVM_SYS_LIBS)

CPP_SRC = $(SRC_DIR)/main.cpp \
          $(SRC_DIR)/Ast.cpp \
          $(SRC_DIR)/SymbolTable.cpp \
          $(SRC_DIR)/Type.cpp \
          $(SRC_DIR)/SemanticAnalyzer.cpp \
          $(SRC_DIR)/IrBuilder.cpp \
          $(BUILD_DIR)/sysy.tab.cpp

C_SRC = $(BUILD_DIR)/lex.yy.c

OBJ_MAIN       = $(OBJ_DIR)/main.o
OBJ_AST        = $(OBJ_DIR)/Ast.o
OBJ_SYMTAB     = $(OBJ_DIR)/SymbolTable.o
OBJ_TYPE       = $(OBJ_DIR)/Type.o
OBJ_SEMANTIC   = $(OBJ_DIR)/SemanticAnalyzer.o
OBJ_IRBUILDER  = $(OBJ_DIR)/IrBuilder.o
OBJ_PARSER     = $(OBJ_DIR)/sysy.tab.o
OBJ_LEXER      = $(OBJ_DIR)/lex.yy.o

OBJECTS = $(OBJ_MAIN) $(OBJ_AST) $(OBJ_SYMTAB) $(OBJ_TYPE) $(OBJ_SEMANTIC) $(OBJ_IRBUILDER) $(OBJ_PARSER) $(OBJ_LEXER)

TARGET = $(BUILD_DIR)/compiler

test-semantics: all
	@echo "=== Test: Semantic analysis ==="
	@echo 'int main() { int a; a = 1; return a; }' > /tmp/test.sy
	$(BUILD_DIR)/compiler --check-semantics /tmp/test.sy
.PHONY: all clean dirs test-llvm

all: dirs $(TARGET)

dirs:
	@mkdir -p $(BUILD_DIR) $(OBJ_DIR)

$(BUILD_DIR)/sysy.tab.cpp $(BUILD_DIR)/sysy.tab.hpp: $(SRC_DIR)/sysy.y
	bison -d -v -o $(BUILD_DIR)/sysy.tab.cpp $<

$(BUILD_DIR)/lex.yy.c: $(SRC_DIR)/sysy.l $(BUILD_DIR)/sysy.tab.hpp
	flex -o $@ $<

$(OBJ_DIR)/main.o: $(SRC_DIR)/main.cpp $(BUILD_DIR)/sysy.tab.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/Ast.o: $(SRC_DIR)/Ast.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/SymbolTable.o: $(SRC_DIR)/SymbolTable.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/Type.o: $(SRC_DIR)/Type.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/SemanticAnalyzer.o: $(SRC_DIR)/SemanticAnalyzer.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/IrBuilder.o: $(SRC_DIR)/IrBuilder.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/sysy.tab.o: $(BUILD_DIR)/sysy.tab.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/lex.yy.o: $(BUILD_DIR)/lex.yy.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

clean:
	rm -rf $(BUILD_DIR)

test-llvm: all
	@echo "=== Test 1: Simple main ==="
	@echo 'int main() { return 42; }' > /tmp/test1.sy
	$(BUILD_DIR)/compiler --emit-llvm /tmp/test1.sy
	@echo ""
	@echo "=== Test 2: Variables + arithmetic ==="
	@echo 'int main() { int a = 10; int b = 20; int c; c = a + b; return c; }' > /tmp/test2.sy
	$(BUILD_DIR)/compiler --emit-llvm /tmp/test2.sy
	@echo ""
	@echo "=== Test 3: Global + local ==="
	@echo 'int global = 5; int main() { int local; local = global + 3; return local; }' > /tmp/test3.sy
	$(BUILD_DIR)/compiler --emit-llvm /tmp/test3.sy
	@echo ""
	@echo "=== Test 4: Unary minus ==="
	@echo 'int main() { int a = 5; return -a; }' > /tmp/test4.sy
	$(BUILD_DIR)/compiler --emit-llvm /tmp/test4.sy

test-stage8: all
	@echo "=== Test 1: Function call ==="
	@echo 'int add(int a, int b) { return a + b; } int main() { return add(1, 2); }' > /tmp/t1.sy
	$(BUILD_DIR)/compiler --emit-llvm /tmp/t1.sy
	@echo ""
	@echo "=== Test 2: Array access ==="
	@echo 'int main() { int a[3]; a[1] = 5; return a[1]; }' > /tmp/t2.sy
	$(BUILD_DIR)/compiler --emit-llvm /tmp/t2.sy
	@echo ""
	@echo "=== Test 3: Implicit cast int->float ==="
	@echo 'int main() { float a = 3; float b = a + 1.5; return 0; }' > /tmp/t3.sy
	$(BUILD_DIR)/compiler --emit-llvm /tmp/t3.sy
	@echo ""
	@echo "=== Test 4: Array as parameter ==="
	@echo 'int sum(int a[], int n) { int s = 0; int i = 0; while(i < n) { s = s + a[i]; i = i + 1; } return s; } int main() { int a[3]; a[0]=1; a[1]=2; a[2]=3; return sum(a, 3); }' > /tmp/t4.sy
	$(BUILD_DIR)/compiler --emit-llvm /tmp/t4.sy
  
test-stage9: all
	@echo "=== Test 1: getint/putint ==="
	@echo 'int main() { int a = getint(); putint(a); return 0; }' > /tmp/t9_1.sy
	$(BUILD_DIR)/compiler --emit-llvm /tmp/t9_1.sy
	@echo ""
	@echo "=== Test 2: putf ==="
	@echo 'int main() { putf("%d", 42); return 0; }' > /tmp/t9_2.sy
	$(BUILD_DIR)/compiler --emit-llvm /tmp/t9_2.sy
	@echo ""
	@echo "=== Test 3: starttime/stoptime ==="
	@echo 'int main() { starttime(); int a = 1; stoptime(); return a; }' > /tmp/t9_3.sy
	$(BUILD_DIR)/compiler --emit-llvm /tmp/t9_3.sy
  
test-stage10: all
	@echo "=== Test 1: Simple main -> RISC-V asm ==="
	@echo 'int main() { return 42; }' > /tmp/t10_1.sy
	$(BUILD_DIR)/compiler /tmp/t10_1.sy -o /tmp/t10_1.s
	@cat /tmp/t10_1.s | head -20
	@echo ""
	@echo "=== Test 2: Variables + arithmetic -> asm ==="
	@echo 'int main() { int a = 10; int b = 20; return a + b; }' > /tmp/t10_2.sy
	$(BUILD_DIR)/compiler /tmp/t10_2.sy -o /tmp/t10_2.s
	@echo ""
	@echo "=== Test 3: Control flow -> asm ==="
	@echo 'int main() { int a = 0; if (a < 1) a = 10; while (a > 0) a = a - 1; return a; }' > /tmp/t10_3.sy
	$(BUILD_DIR)/compiler /tmp/t10_3.sy -o /tmp/t10_3.s