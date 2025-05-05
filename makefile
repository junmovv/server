# ====================== Makefile ======================
# 定义C和C++编译器
CC = gcc
CXX = g++

# 定义最终生成的可执行文件名称
TARGET := EchoServer

# 定义中间文件构建目录
BUILD_DIR := _build

# 默认构建目标
all: $(TARGET)


# ====================== makefile.app ======================
# 应用配置相关参数

# 头文件搜索路径（-I开头）
INC_PATH := \
    -I./include \
    -I./log/include

# 源文件搜索目录
SRC_PATH := \
    . \
    ./src \
    ./log/src

# 编译参数定义
CFLAGS :=   # C语言特有编译参数
CXXFLAGS := # C++特有编译参数
GFLAGS := -lpthread  # 全局公共参数
G_MACROS :=   # 预定义宏

# 合并编译参数：C参数 + C++参数 + 头文件路径 + 宏定义
GFLAGS += $(CFLAGS) $(CXXFLAGS) $(INC_PATH) $(G_MACROS)


# ====================== makefile.rules ======================
# 构建规则相关

#根据源文件路径 提取.o 
SOURCES := $(foreach dir, $(SRC_PATH), $(wildcard $(dir)/*))
C_SOURCES := $(filter %.c, $(SOURCES))
C_OBJS := $(C_SOURCES:%.c=%.o)
CPP_SOURCES := $(filter %.cpp, $(SOURCES))
CPP_OBJS := $(CPP_SOURCES:%.cpp=%.o)
OBJS := $(C_OBJS) $(CPP_OBJS)

#增加编译目标文件路径
OBJS := $(addprefix $(BUILD_DIR)/,$(OBJS))
#生成对应.d
DEPS := $(OBJS:%.o=%.d)

# 生成依赖关系
$(BUILD_DIR)/%.d : %.c
	$(MKDIR)
	@echo "\033[32m	CREATE	$(notdir $@)\033[0m"
	@set -e; rm -f $@ $@.*; \
	$(CC) -MM $(GFLAGS) $< > $@.$$$$; \
	sed 's,$$(notdir $*)$\.o[ :]*,$(BUILD_DIR)/$*\.o $(BUILD_DIR)/$*\.d : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$ $@.*

$(BUILD_DIR)/%.d : %.cpp
	$(MKDIR)
	@echo "\033[32m	CREATE	$(notdir $@)\033[0m"
	@set -e; rm -f $@ $@.*; \
	$(CXX) -MM $(GFLAGS) $< >  $@.$$$$; \
	sed 's,$$(notdir $*)$\.o[ :]*,$(BUILD_DIR)/$*\.o $(BUILD_DIR)/$*\.d : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$ $@.*

# 目标文件依赖于源文件和头文件
$(BUILD_DIR)/%.o : %.c
	$(MKDIR)
	@echo "\033[35m	CC	$(notdir $@)\033[0m"
	@$(CC) $(GFLAGS) -o $@ -c $<

$(BUILD_DIR)/%.o : %.cpp
	$(MKDIR)
	@echo "\033[35m	CXX	$(notdir $@)\033[0m"
	@$(CXX) $(GFLAGS) -o $@ -c $<


#自定义命令
MKDIR = @mkdir -p $(dir $@)

#COPY 函数   如果入参2  不存在 则创建此目录  \
然后判断 入参1(目录)和入参3(文件) 组成的 带路径的文件是否存在  如果存在 比较 1/3 和 2/3 是不是同一个文件，不同则执行cp \
如果1/3文件不存在  则 打印 不存在
define COPY
    test -d $(2) || mkdir -p $(2); \
    test -e  $(1)/$(3) && (cmp -s $(1)/$(3) $(2)/$(3) || (echo "\033[32m	CP	$(1)/$(3)	TO	$(2)$(4)\033[0m"; cp -f $(1)/$(3) $(2)$(4))) \
    || (echo  -e "\033[32m        warning: $(1)/$(3) is not exist\033[0m")
endef


#例如  $1  divMulti_update $2  _update  $3  divMulti:./thirdLibSrc/divMulti
# 得到结果是 ./thirdLibSrc/divMulti   内部的patsubst 得到的是divMulti   然后过滤掉 $3 的 divMulti: 得到 ./thirdLibSrc/divMulti 由于路径后面没有: 最外层的:不起作用
GET_LIB_SRC_PATH = $(patsubst $(patsubst %$(2),%,$(1)):%,%,$(filter $(patsubst %$(2),%,$(1)):%,$(3)))


# 包含自动生成的依赖文件(.d文件)
sinclude $(DEPS)

# 链接所有目标文件生成可执行文件
$(TARGET) : $(OBJS)
	@echo "\033[34m	Linking: $(notdir $^) with libs $(LDLIBS) -> $(notdir $@)\033[0m"
	@$(CXX) $^ -o $@ $(GFLAGS) $(LDLIBS)

# 清理构建产物
clean:
	rm -rf $(BUILD_DIR) $(TARGET)

# 清理库文件（依赖makefile.lib中定义的目标）
libclean: $(LIBS_CLEAN) $(DYN_LIBS_CLEAN)