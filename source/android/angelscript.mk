LOCAL_PATH := $(QFUSION_PATH)/libsrcs/angelscript/sdk/angelscript
include $(CLEAR_VARS)
LOCAL_MODULE := angelscript

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_C_INCLUDES := $(LOCAL_EXPORT_C_INCLUDES)

LOCAL_SRC_FILES := \
  source/as_atomic.cpp \
  source/as_builder.cpp \
  source/as_bytecode.cpp \
  source/as_callfunc.cpp \
  source/as_callfunc_arm.cpp \
  source/as_callfunc_arm_gcc.S \
  source/as_callfunc_mips.cpp \
  source/as_callfunc_ppc.cpp \
  source/as_callfunc_ppc_64.cpp \
  source/as_callfunc_sh4.cpp \
  source/as_callfunc_x64_gcc.cpp \
  source/as_callfunc_x86.cpp \
  source/as_compiler.cpp \
  source/as_context.cpp \
  source/as_configgroup.cpp \
  source/as_datatype.cpp \
  source/as_generic.cpp \
  source/as_gc.cpp \
  source/as_globalproperty.cpp \
  source/as_memory.cpp \
  source/as_module.cpp \
  source/as_objecttype.cpp \
  source/as_outputbuffer.cpp \
  source/as_parser.cpp \
  source/as_restore.cpp \
  source/as_scriptcode.cpp \
  source/as_scriptengine.cpp \
  source/as_scriptfunction.cpp \
  source/as_scriptnode.cpp \
  source/as_scriptobject.cpp \
  source/as_string.cpp \
  source/as_string_util.cpp \
  source/as_thread.cpp \
  source/as_tokenizer.cpp \
  source/as_typeinfo.cpp \
  source/as_variablescope.cpp

include $(BUILD_STATIC_LIBRARY)
