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

ifeq ($(strip $(BOARD_USE_CELLULAR_COEX)),true)
    LOCAL_REQUIRED_MODULES += libbtvendorcellcoex-client
       LOCAL_SRC_FILES += hci_service.c
       LOCAL_CFLAGS += -DUSE_CELLULAR_COEX
    LOCAL_SHARED_LIBRARIES += \
               libstlport \
               libbinder \
               libutils \
               libandroid_runtime \
               liblog \
               libbtvendorcellcoex-client
endif

LOCAL_MODULE := libbt-vendor
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

endif # BOARD_HAVE_BLUETOOTH_LINUX
