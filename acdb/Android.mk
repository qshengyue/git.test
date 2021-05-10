ifeq ($(call is-board-platform-in-list, msmnile kona lahaina),true)

ifneq ($(BUILD_TINY_ANDROID),true)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

#----------------------------------------------------------------------------
#                 Common definitons
#----------------------------------------------------------------------------

acdb-def += -D_ANDROID_

#----------------------------------------------------------------------------
#             Make the Shared library (libar-acdb)
#----------------------------------------------------------------------------

#LOCAL_C_INCLUDES := $(LOCAL_PATH)/inc

LOCAL_CFLAGS := $(acdb-def)

LOCAL_C_INCLUDES := $(LOCAL_PATH)/api\
	$(LOCAL_PATH)/inc\
    $(TOP)/vendor/qcom/proprietary/common/inc

LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/include/mm-audio/ar/ar_osal
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/include/mm-audio/ar/gsl

LOCAL_SRC_FILES := \
	src/acdb.c \
	src/acdb_command.c \
	src/acdb_delta_file_mgr.c \
	src/acdb_delta_parser.c \
	src/acdb_file_mgr.c \
	src/acdb_init.c \
	src/acdb_init_utility.c \
	src/acdb_parser.c \
	src/acdb_utility.c\
	src/acdb_data_proc.c\
	src/acdb_heap.c

LOCAL_MODULE := libar-acdb
LOCAL_MODULE_OWNER := qti
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true

LOCAL_HEADER_LIBRARIES := libcutils_headers\
	libutils_headers

LOCAL_SHARED_LIBRARIES := \
	liblx-osal

LOCAL_COPY_HEADERS_TO := mm-audio/ar/acdb
LOCAL_COPY_HEADERS := api/acdb.h\
	api/acdb_begin_pack.h\
	api/acdb_end_pack.h\
	inc/acdb_utility.h\
	inc/acdb_command.h\
	inc/acdb_common.h\
	inc/acdb_data_proc.h\
	inc/acdb_delta_file_mgr.h\
	inc/acdb_delta_parser.h\
	inc/acdb_file_mgr.h\
	inc/acdb_heap.h\
	inc/acdb_init.h\
	inc/acdb_init_utility.h\
	inc/acdb_parser.h

include $(BUILD_SHARED_LIBRARY)


endif # BUILD_TINY_ANDROID
endif # is-board-platform-in-list
