/* config.h - hand-written for the Qt port (replaces autoconf output) */
#ifndef CONFIG_H
#define CONFIG_H
#define PACKAGE "xfig"
#define PACKAGE_NAME "xfig"
#define PACKAGE_VERSION "3.2.9a"
#define PACKAGE_STRING "xfig 3.2.9a"
#define PACKAGE_TARNAME "xfig"
#define PACKAGE_BUGREPORT "thomas.loimer@tuwien.ac.at"
#define PACKAGE_URL "https://sourceforge.net/projects/mcj"
#define VERSION "3.2.9a"
#define PROTOCOL_VERSION "3.2"
#define RELEASE_YEAR "2024"
#define ARROWS4TO14 1
#define HAVE_DECL_S_IFDIR 1
#define HAVE_DECL_S_IWRITE 1
#define HAVE_DIRENT_H 1
#define HAVE_GETCWD 1
#define HAVE_INTPTR_T 1
#define HAVE_INTTYPES_H 1
#define HAVE_ISASCII 1
#define HAVE_NL_LANGINFO 1
#define HAVE_POSIX_SPAWNP 1
#define HAVE_SETLOCALE 1
#define HAVE_STDINT_H 1
#define HAVE_STDIO_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRCASECMP 1
#define HAVE_STRCHR 1
#define HAVE_STRDUP 1
#define HAVE_STRERROR 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_STRNCASECMP 1
#define HAVE_STRNDUP 1
#define HAVE_STRRCHR 1
#define HAVE_STRSTR 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UINTPTR_T 1
#define HAVE_UNISTD_H 1
#define STDC_HEADERS 1
#define GSEXE "gs"
#define HAVE_PNG 1
#define HAVE_JPEG 1
#define CACHE_SIZE_LIMIT 20000
/* xfig's data (CompKeyDB, doc/, Libraries/) is found at run time, next to
   the program wherever it is installed (xq_data_path() in ui/main.cpp) */
#ifdef __cplusplus
extern "C"
#endif
const char *xq_data_path(const char *sub);
#define XFIGLIBDIR xq_data_path("")
#define XFIGDOCDIR xq_data_path("doc")
#define FIG2DEV_DIR ""
#define PRINTCAP "/etc/printcap"
#define REG_NOERROR 0
/* set at start-up as the library_dir resource, also xq_data_path() */
#define OBJLIBDIR ""
#endif
