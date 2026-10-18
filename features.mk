# NO_MAGEL_* = 1 时加入对应宏；省略或 0 保持启用。直接 -D 定义按存在性裁剪。
MAGEL_TRIM_OPTIONS := NO_MAGEL_BIG_INT NO_MAGEL_BIG_FLOAT NO_MAGEL_DEFAULT_POOL NO_MAGEL_STD NO_MAGEL_STD_MATH NO_MAGEL_STD_RANDOM NO_MAGEL_STD_REGEX NO_MAGEL_STD_SIGNAL NO_MAGEL_STD_JSON NO_MAGEL_STD_TOML NO_MAGEL_STD_BINARY NO_MAGEL_STD_LINALG NO_MAGEL_STD_PARSE NO_MAGEL_STD_PROCESS
# 工具链默认编入注解与反射前端及 VM 支持；构建配置指纹负责切换后的重编译。
# 仅在显式验证裁剪配置时将两项设为 0；单次源码编译可用 --no-reflection 关闭。
MAGEL_WITH_REFLECTION ?= 1
STACKVM_WITH_REFLECTION ?= 1
MAGEL_FEATURE_FLAGS = -DMAGEL_WITH_REFLECTION=$(MAGEL_WITH_REFLECTION) -DSTACKVM_WITH_REFLECTION=$(STACKVM_WITH_REFLECTION)
MAGEL_TRIM_FLAGS = $(MAGEL_FEATURE_FLAGS) $(foreach option,$(MAGEL_TRIM_OPTIONS),$(if $(filter 1,$($(option))),-D$(option)))

PYTHON_NULL := $(if $(filter sh bash sh.exe bash.exe,$(notdir $(SHELL))),/dev/null,NUL)
PYTHON ?= $(or $(shell python3 -c "import sys; print('python3' if sys.version_info.major == 3 else '')" 2>$(PYTHON_NULL)), $(shell python -c "import sys; print('python' if sys.version_info.major == 3 else '')" 2>$(PYTHON_NULL)))
PYTHON := $(PYTHON)
