pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = acdb.pc
EXTRA_DIST = $(pkgconfig_DATA)

AM_CFLAGS = -Wall

if BUILDSYSTEM_OPENWRT
AM_CFLAGS += -I ./inc
AM_CFLAGS += -I ./api

AM_CPPFLAGS = -I ./inc
AM_CPPFLAGS += -I ./api

acdb_sources = ./inc/acdb_command.h \
               ./inc/acdb_common.h \
               ./inc/acdb_delta_file_mgr.h \
               ./inc/acdb_delta_parser.h \
               ./inc/acdb_file_mgr.h \
               ./inc/acdb_init.h \
               ./inc/acdb_init_utility.h \
               ./inc/acdb_parser.h \
               ./inc/acdb_utility.h \
               ./inc/acdb_data_proc.h \
               ./inc/acdb_heap.h\
               ./api/acdb.h \
               ./api/acdb_begin_pack.h \
               ./api/acdb_end_pack.h

acdb_c_sources = ./src/acdb.c \
                 ./src/acdb_command.c \
                 ./src/acdb_delta_file_mgr.c \
                 ./src/acdb_delta_parser.c \
                 ./src/acdb_file_mgr.c \
                 ./src/acdb_init.c \
                 ./src/acdb_init_utility.c \
                 ./src/acdb_parser.c \
                 ./src/acdb_utility.c \
                 ./src/acdb_data_proc.c \
                 ./src/acdb_heap.c

else
AM_CFLAGS += -I ${top_srcdir}/inc
AM_CFLAGS += -I ${top_srcdir}/api

AM_CPPFLAGS = -I ${top_srcdir}/inc
AM_CPPFLAGS += -I ${top_srcdir}/api

acdb_sources = ${top_srcdir}/inc/acdb_command.h \
               ${top_srcdir}/inc/acdb_common.h \
               ${top_srcdir}/inc/acdb_delta_file_mgr.h \
               ${top_srcdir}/inc/acdb_delta_parser.h \
               ${top_srcdir}/inc/acdb_file_mgr.h \
               ${top_srcdir}/inc/acdb_init.h \
               ${top_srcdir}/inc/acdb_init_utility.h \
               ${top_srcdir}/inc/acdb_parser.h \
               ${top_srcdir}/inc/acdb_utility.h \
               ${top_srcdir}/inc/acdb_data_proc.h \
               ${top_srcdir}/inc/acdb_heap.h \
               ${top_srcdir}/api/acdb.h \
               ${top_srcdir}/api/acdb_begin_pack.h \
               ${top_srcdir}/api/acdb_end_pack.h

acdb_c_sources = ${top_srcdir}/src/acdb.c \
                 ${top_srcdir}/src/acdb_command.c \
                 ${top_srcdir}/src/acdb_delta_file_mgr.c \
                 ${top_srcdir}/src/acdb_delta_parser.c \
                 ${top_srcdir}/src/acdb_file_mgr.c \
                 ${top_srcdir}/src/acdb_init.c \
                 ${top_srcdir}/src/acdb_init_utility.c \
                 ${top_srcdir}/src/acdb_parser.c \
                 ${top_srcdir}/src/acdb_utility.c \
                 ${top_srcdir}/src/acdb_data_proc.c \
                 ${top_srcdir}/src/acdb_heap.c
endif

lib_includedir = $(includedir)
lib_include_HEADERS = $(acdb_sources)


#LIBRARY for LE apps
lib_LTLIBRARIES = libacdb.la
libacdb_la_CC = @CC@
libacdb_la_LIBADD = -lpthread -lar_osal
libacdb_la_SOURCES = $(acdb_c_sources)
libacdb_la_CFLAGS = $(AM_CFLAGS)
libacdb_la_CPPFLAGS = $(AM_CPPFLAGS)
libacdb_la_LDFLAGS = -shared -avoid-version

