/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <reent.h>
#include <pthread.h>
#include <setjmp.h>
#include <getopt.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <ctype.h>

#include "rom/ets_sys.h"

#include "private/elf_symbol.h"

extern int __ltdf2(double a, double b);
extern unsigned int __fixunsdfsi(double a);
extern int __gtdf2(double a, double b);
extern double __floatunsidf(unsigned int i);
extern double __divdf3(double a, double b);

/** @brief Libc public functions symbols look-up table */

static const struct esp_elfsym g_esp_libc_elfsyms[] = {

    /* string.h */

    ESP_ELFSYM_EXPORT(strerror),
    ESP_ELFSYM_EXPORT(memset),
    ESP_ELFSYM_EXPORT(memcpy),
    ESP_ELFSYM_EXPORT(memmove),
    ESP_ELFSYM_EXPORT(strlen),
    ESP_ELFSYM_EXPORT(strtod),
    ESP_ELFSYM_EXPORT(strrchr),
    ESP_ELFSYM_EXPORT(strchr),
    ESP_ELFSYM_EXPORT(strcmp),
    ESP_ELFSYM_EXPORT(strtol),
    ESP_ELFSYM_EXPORT(strcspn),
    ESP_ELFSYM_EXPORT(strncat),
    ESP_ELFSYM_EXPORT(strcasecmp),
    ESP_ELFSYM_EXPORT(strcpy),
    ESP_ELFSYM_EXPORT(strncpy),
    ESP_ELFSYM_EXPORT(strtok),
    ESP_ELFSYM_EXPORT(sscanf),
    //ESP_ELFSYM_EXPORT(strdupa),
    //ESP_ELFSYM_EXPORT(strndupa),

    /* unistd.h */

    ESP_ELFSYM_EXPORT(usleep),
    ESP_ELFSYM_EXPORT(sleep),
    ESP_ELFSYM_EXPORT(exit),
    ESP_ELFSYM_EXPORT(close),

    /* stdlib.h */

    //ESP_ELFSYM_EXPORT_WHY(malloc, my_malloc),
    //ESP_ELFSYM_EXPORT(calloc),
    //ESP_ELFSYM_EXPORT(realloc),
    //ESP_ELFSYM_EXPORT(free),

    /* stdio.h */

    ESP_ELFSYM_EXPORT(snprintf),
    ESP_ELFSYM_EXPORT(dprintf),
    ESP_ELFSYM_EXPORT(sprintf),
    ESP_ELFSYM_EXPORT(snprintf),
    ESP_ELFSYM_EXPORT(vprintf),
    ESP_ELFSYM_EXPORT(vdprintf),
    ESP_ELFSYM_EXPORT(vsprintf),
    ESP_ELFSYM_EXPORT(vsnprintf),

    /* time.h */

    ESP_ELFSYM_EXPORT(clock_gettime),
    ESP_ELFSYM_EXPORT(strftime),
    ESP_ELFSYM_EXPORT(gettimeofday),

    /* pthread.h */

    ESP_ELFSYM_EXPORT(pthread_create),
    ESP_ELFSYM_EXPORT(pthread_attr_init),
    ESP_ELFSYM_EXPORT(pthread_attr_setstacksize),
    ESP_ELFSYM_EXPORT(pthread_detach),
    ESP_ELFSYM_EXPORT(pthread_join),
    ESP_ELFSYM_EXPORT(pthread_exit),

    /* newlib */

    ESP_ELFSYM_EXPORT(__errno),
    ESP_ELFSYM_EXPORT(__getreent),
#ifdef __HAVE_LOCALE_INFO__
    ESP_ELFSYM_EXPORT(__locale_ctype_ptr),
#else
    ESP_ELFSYM_EXPORT(_ctype_),
#endif

    /* math */

    ESP_ELFSYM_EXPORT(__ltdf2),
    ESP_ELFSYM_EXPORT(__fixunsdfsi),
    ESP_ELFSYM_EXPORT(__gtdf2),
    ESP_ELFSYM_EXPORT(__floatunsidf),
    ESP_ELFSYM_EXPORT(__divdf3),

    /* getopt.h */

    ESP_ELFSYM_EXPORT(getopt_long),
    ESP_ELFSYM_EXPORT(optind),
    ESP_ELFSYM_EXPORT(opterr),
    ESP_ELFSYM_EXPORT(optarg),
    ESP_ELFSYM_EXPORT(optopt),

    /* setjmp.h */

    ESP_ELFSYM_EXPORT(longjmp),
    ESP_ELFSYM_EXPORT(setjmp),

    ESP_ELFSYM_END
};

/** @brief ESP-IDF public functions symbols look-up table */

static const struct esp_elfsym g_esp_espidf_elfsyms[] = {

    /* sys/socket.h */

    ESP_ELFSYM_EXPORT(lwip_bind),
    ESP_ELFSYM_EXPORT(lwip_setsockopt),
    ESP_ELFSYM_EXPORT(lwip_socket),
    ESP_ELFSYM_EXPORT(lwip_listen),
    ESP_ELFSYM_EXPORT(lwip_accept),
    ESP_ELFSYM_EXPORT(lwip_recv),
    ESP_ELFSYM_EXPORT(lwip_recvfrom),
    ESP_ELFSYM_EXPORT(lwip_send),
    ESP_ELFSYM_EXPORT(lwip_sendto),
    ESP_ELFSYM_EXPORT(lwip_connect),

    /* arpa/inet.h */

    ESP_ELFSYM_EXPORT(ipaddr_addr),
    ESP_ELFSYM_EXPORT(lwip_htons),
    ESP_ELFSYM_EXPORT(lwip_htonl),
    ESP_ELFSYM_EXPORT(ip4addr_ntoa),

    /* ROM functions */

    ESP_ELFSYM_EXPORT(ets_printf),

    ESP_ELFSYM_END
};

extern void *why_malloc(size_t size);
extern void why_free(void *ptr);
extern void *why_calloc(size_t nmemb, size_t size);
extern void *why_realloc(void *ptr, size_t size);
extern void *why_reallocarray(void *ptr, size_t nmemb, size_t size);

extern int why_open(const char *pathname, int flags, mode_t mode);
extern int why_close(int fd);

extern ssize_t why_write(int fd, const void *buf, size_t count);
extern ssize_t why_read(int fd, void *buf, size_t count);
extern off_t why_lseek(int fd, off_t offset, int whence);

extern int why_printf(const char *fmt, ...);
extern int why_puts(const char *str);

extern FILE *why_fopen(const char *path, const char *mode);
extern int why_fclose(FILE *__stream);
extern int why_fseek(FILE *stream, long offset, int whence);
extern int why_fseeko(FILE *stream, __off_t offset, int whence);
extern int why_fflush(FILE *stream);

extern FILE *why_fdopen(int, const char *);
extern int why_fprintf(FILE *__stream, const char *__fmt, ...);

extern int why_fgetc(FILE *stream);
extern char *why_fgets(char *str, int size, FILE *stream);

extern char *why_strdup(const char *s);
extern char *why_strndup(const char *s, size_t n);

extern int why_isatty(int fd);
extern char *why_getenv(const char *name);

extern int why_atexit(void (*function)(void));

extern int why_tcgetattr(int fd, void *termios_p);
extern int why_tcsetattr(int fd, int optional_actions, const void *termios_p);

extern int why_fputs(const char *str, FILE *stream);
extern int why_fputc(int c, FILE *stream);
extern int why_fileno(FILE *stream);

static const struct esp_elfsym g_why2025_libc_elfsyms[] = {
    ESP_ELFSYM_EXPORT_WHY(malloc),
    ESP_ELFSYM_EXPORT_WHY(free),
    ESP_ELFSYM_EXPORT_WHY(calloc),
    ESP_ELFSYM_EXPORT_WHY(realloc),
    ESP_ELFSYM_EXPORT_WHY(reallocarray),
    ESP_ELFSYM_EXPORT_WHY(open),
    ESP_ELFSYM_EXPORT_WHY(close),
    ESP_ELFSYM_EXPORT_WHY(fseek),
    ESP_ELFSYM_EXPORT_WHY(fseeko),
    ESP_ELFSYM_EXPORT_WHY(fflush),
    ESP_ELFSYM_EXPORT_WHY(write),
    ESP_ELFSYM_EXPORT_WHY(read),
    ESP_ELFSYM_EXPORT_WHY(lseek),
    ESP_ELFSYM_EXPORT_WHY(printf),
    ESP_ELFSYM_EXPORT_WHY(puts),
    ESP_ELFSYM_EXPORT_WHY(fopen),
    ESP_ELFSYM_EXPORT_WHY(fclose),
    ESP_ELFSYM_EXPORT_WHY(fdopen),
    ESP_ELFSYM_EXPORT_WHY(fprintf),
    ESP_ELFSYM_EXPORT_WHY(fgetc),
    ESP_ELFSYM_EXPORT_WHY(fgets),
    
    ESP_ELFSYM_EXPORT_WHY(strdup),
    ESP_ELFSYM_EXPORT_WHY(strndup),

    ESP_ELFSYM_EXPORT_WHY(isatty),
    ESP_ELFSYM_EXPORT_WHY(getenv),
    ESP_ELFSYM_EXPORT_WHY(atexit),

    ESP_ELFSYM_EXPORT_WHY(tcgetattr),
    ESP_ELFSYM_EXPORT_WHY(tcsetattr),

    ESP_ELFSYM_EXPORT_WHY(fputs),
    ESP_ELFSYM_EXPORT_WHY(fputc),
    ESP_ELFSYM_EXPORT_WHY(fileno),

    ESP_ELFSYM_END
};

/**
 * @brief Find symbol address by name.
 *
 * @param sym_name - Symbol name
 *
 * @return Symbol address if success or 0 if failed.
 */
uintptr_t elf_find_sym(const char *sym_name)
{
    const struct esp_elfsym *syms;

    syms = g_why2025_libc_elfsyms;
    while (syms->name) {
        if (!strcmp(syms->name, sym_name)) {
            return (uintptr_t)syms->sym;
        }

        syms++;
    }
#ifdef CONFIG_ELF_LOADER_LIBC_SYMBOLS
    syms = g_esp_libc_elfsyms;
    while (syms->name) {
        if (!strcmp(syms->name, sym_name)) {
            return (uintptr_t)syms->sym;
        }

        syms++;
    }
#else
    syms = g_esp_libc_elfsyms;
    (void)syms;
#endif

#ifdef CONFIG_ELF_LOADER_ESPIDF_SYMBOLS
    syms = g_esp_espidf_elfsyms;
    while (syms->name) {
        if (!strcmp(syms->name, sym_name)) {
            return (uintptr_t)syms->sym;
        }

        syms++;
    }
#else
    syms = g_esp_espidf_elfsyms;
    (void)syms;
#endif

#ifdef CONFIG_ELF_LOADER_CUSTOMER_SYMBOLS
    extern const struct esp_elfsym g_customer_elfsyms[];

    syms = g_customer_elfsyms;
    while (syms->name) {
        if (!strcmp(syms->name, sym_name)) {
            return (uintptr_t)syms->sym;
        }

        syms++;
    }
#endif

    return 0;
}
