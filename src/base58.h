#include <stdbool.h>
#include <stddef.h>

/* Return true IFF successful, false otherwise. */
bool b58enc(/* out */ char *b58, /* in/out */ size_t *b58sz, const void *bin, size_t binsz);
