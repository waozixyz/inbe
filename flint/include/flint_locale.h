#ifndef FLINT_LOCALE_H
#define FLINT_LOCALE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FlintLocaleEntry {
    char *key;
    char *value;
} FlintLocaleEntry;

typedef struct FlintLocaleLanguage {
    char *code;
    char *label;
} FlintLocaleLanguage;

void locale_init(void);
int locale_set(const char *code);
const char *locale_get(const char *key);
void locale_format(char *dst, size_t dst_size, const char *key, ...);

int locale_count(void);
const char *locale_code_at(int index);
const char *locale_label_at(int index);
int locale_index_of(const char *code);
const char *locale_current_code(void);
int locale_current_index(void);

#ifdef __cplusplus
}
#endif

#endif // FLINT_LOCALE_H
