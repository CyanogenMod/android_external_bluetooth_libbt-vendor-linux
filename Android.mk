LOCAL_PATH := $(call my-dir)

ifeq ($(BOARD_HAVE_BLUETOOTH_LINUX), true)

include $(CLEAR_VARS)

BDROID_DIR := $(TOP_DIR)external/bluetooth/bluedroid

LOCAL_SRC_FILES := \
        bt_vendor_linux.c

LOCAL_C_INCLUDES += \
        $(BDROID_DIR)/hci/include

LOCAL_SHARED_LIBRARIES := \
        libcutils

LOCAL_MODULE := libbt-vendor
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_SHARED_LIBRARIES)

include $(BUILD_SHARED_LIBRARY)

endif # BOARD_HAVE_BLUETOOTH_LINUX