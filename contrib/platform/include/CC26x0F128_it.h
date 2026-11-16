/**
 * @file CC26x0F128_it.h
 */

#ifndef _CC26x0F128_IT_H_
#define _CC26x0F128_IT_H_

#include <sys/cdefs.h>

typedef void (*vector_t)(void);

__BEGIN_DECLS

extern vector_t vector[];

#define IRQN_HANDLER(idx, name) void name##_IRQHandler(void);
#define SKIP_HANDLER(idx)
#include "CC26x0F128_handlers.h"
#undef IRQN_HANDLER
#undef SKIP_HANDLER

__END_DECLS

#endif // defined(_CC26x0F128_IT_H_)