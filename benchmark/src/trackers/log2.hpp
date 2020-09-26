#pragma once

static inline size_t calc_next_log2(unsigned long num)
{
	size_t result = (size_t) (sizeof(long) * 8 - __builtin_clzl(num));
	if (!(num & (num - 1)))
		result--;
	return result;
}
