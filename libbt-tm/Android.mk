LOCAL_PATH := $(call my-dir)

ifeq ($(BOARD_HAVE_BLUETOOTH_LINUX), true)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
        bt_tm.c

LOCAL_C_INCLUDES += \
        vendor/intel/telemetry-client/client

LOCAL_SHARED_LIBRARIES := \
        libcutils \
        libtmclient

LOCAL_MODULE := libbttm
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_SHARED_LIBRARIES)

include $(BUILD_SHARED_LIBRARY)

endif # BOARD_HAVE_BLUETOOTH_LINUX