#include "processor.h"
#include "trap.h"
#include "mulhi.h"
#include "softfloat.h"
#include "platform.h" // softfloat isNaNF32UI, etc.
#include "internals.h" // ditto
#include <assert.h>
#include "payload.h"

// Redefine the macros
#undef MMU
#undef PIPE
#undef STATE
#undef RS1
#undef RS2
#undef WRITE_RD
#undef FRS1
#undef FRS2
#undef FRS3
#undef WRITE_FRD
#undef set_fp_exceptions

#define STATE (state)
#define RS1 ((const reg_t)pay_buf.A_value.dw)
#define RS2 ((const reg_t)pay_buf.B_value.dw)
#define WRITE_RD(value) (pay_buf.C_value.dw = (value))

#define FRS1 ((const freg_t)pay_buf.A_value.dw)
#define FRS2 ((const freg_t)pay_buf.B_value.dw)
#define FRS3 ((const freg_t)pay_buf.D_value.dw)
#define WRITE_FRD(value) (pay_buf.C_value.dw = (value))


#define set_fp_exceptions ({ pay_buf.fflags = softfloat_exceptionFlags; \
                             softfloat_exceptionFlags = 0; })
