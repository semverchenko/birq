#ifndef CPUMASK_H
#define CPUMASK_H

#define NR_CPUS 4096

#include <stdlib.h>
#include "bit_array.h"
#include "hexio.h"

typedef struct { 
	BIT_ARRAY* bits;
} cpumask_t;
extern cpumask_t _unused_cpumask_arg_;

#define cpus_init(dst) ((dst).bits = bit_array_create(NR_CPUS))
#define cpus_free(dst) bit_array_free((dst).bits)
#define cpus_copy(dst, src) bit_array_copy_all((dst).bits, (src).bits)

#define cpu_set(cpu, dst) bit_array_set_bit((dst).bits, cpu)
#define cpu_clear(cpu, dst) bit_array_clear_bit((dst).bits, cpu)

#define cpus_setall(dst) bit_array_set_all((dst).bits)
#define cpus_clear(dst) bit_array_clear_all((dst).bits)

#define cpu_isset(cpu, cpumask) bit_array_get_bit((cpumask).bits, cpu)

#define cpus_and(dst, src1, src2) bit_array_and((dst).bits, (src1).bits, (src2).bits)
#define cpus_or(dst, src1, src2) bit_array_or((dst).bits, (src1).bits, (src2).bits)
#define cpus_xor(dst, src1, src2) bit_array_xor((dst).bits, (src1).bits, (src2).bits)
#define cpus_complement(dst, src) bit_array_not((dst).bits, (src).bits)

#define cpus_equal(src1, src2) (bit_array_cmp((src1).bits, (src2).bits) == 0)
#define cpus_empty(src) (bit_array_num_bits_set((src).bits) == 0)
#define cpus_full(src) (bit_array_num_bits_cleared((src).bits) == 0)
#define cpus_weight(cpumask) bit_array_num_bits_set((cpumask).bits)\

#define cpus_shift_right(dst, n) bit_array_shift_right((dst).bits, n, 0)
#define cpus_shift_left(dst, n) bit_array_shift_left((dst).bits, n, 0)

static inline int __first_cpu(const cpumask_t srcp)
{
	bit_index_t res = bit_array_length(srcp.bits);
	bit_array_find_first_set_bit(srcp.bits, &res);
        return res;
}

#define first_cpu(src) __first_cpu((src))
int __next_cpu(int n, const cpumask_t *srcp);
#define next_cpu(n, src) __next_cpu((n), (src))

#define cpumask_scnprintf(buf, len, src) bitmask_scnprintf((buf), (len), (src).bits)
#define cpumask_parse_user(ubuf, ulen, dst) bitmask_parse_user((ubuf), (ulen), (dst).bits)
#endif /* CPUMASK_H */
