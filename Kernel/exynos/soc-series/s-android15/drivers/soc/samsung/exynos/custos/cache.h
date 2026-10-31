#ifndef __CUSTOS_CACHE_H__
#define __CUSTOS_CACHE_H__

#ifndef __ASSEMBLY__
#include <linux/types.h>
#include <linux/cache.h>

#define CUSTOS_CACHE_ALIGN	SMP_CACHE_BYTES

void custos_cache_free(void *);
void *custos_cache_alloc(ssize_t size);

int custos_cache_prepare(void);

#endif /* !__ASSEMBLY__ */
#endif /* !__CUSTOS_CACHE_H__ */
