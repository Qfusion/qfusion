LOCAL_PATH := $(call my-dir)/libsrcs/libcares
include $(CLEAR_VARS)
LOCAL_MODULE := cares

LOCAL_CFLAGS := -DHAVE_CONFIG_H
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)

LOCAL_SRC_FILES := \
  ares__close_sockets.c \
  ares__get_hostent.c \
  ares__read_line.c \
  ares__timeval.c \
  ares_cancel.c \
  ares_data.c \
  ares_destroy.c \
  ares_expand_name.c \
  ares_expand_string.c \
  ares_fds.c \
  ares_free_hostent.c \
  ares_free_string.c \
  ares_getenv.c \
  ares_gethostbyaddr.c \
  ares_gethostbyname.c \
  ares_getnameinfo.c \
  ares_getsock.c \
  ares_init.c \
  ares_library_init.c \
  ares_llist.c \
  ares_mkquery.c \
  ares_create_query.c \
  ares_nowarn.c \
  ares_options.c \
  ares_parse_a_reply.c \
  ares_parse_aaaa_reply.c \
  ares_parse_mx_reply.c \
  ares_parse_naptr_reply.c \
  ares_parse_ns_reply.c \
  ares_parse_ptr_reply.c \
  ares_parse_soa_reply.c \
  ares_parse_srv_reply.c \
  ares_parse_txt_reply.c \
  ares_platform.c \
  ares_process.c \
  ares_query.c \
  ares_search.c \
  ares_send.c \
  ares_strcasecmp.c \
  ares_strdup.c \
  ares_strerror.c \
  ares_timeout.c \
  ares_version.c \
  ares_writev.c \
  bitncmp.c \
  inet_net_pton.c \
  inet_ntop.c \
  windows_port.c

include $(BUILD_STATIC_LIBRARY)
