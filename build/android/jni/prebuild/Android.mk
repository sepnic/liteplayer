LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := msgutils

LOCAL_SRC_FILES := $(TARGET_ARCH_ABI)/libmsgutils.so

include $(PREBUILT_SHARED_LIBRARY)
