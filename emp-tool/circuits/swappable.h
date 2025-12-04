#ifndef EMP_SWAPPABLE_H__
#define EMP_SWAPPABLE_H__
#include "emp-tool/circuits/bit.h"
namespace emp {
	class Bit;

	template <typename T>
	inline T If(const Bit& select, const T& o1, const T& o2) {
		T res = o2;
		return res.If(select, o1);
	}
	template <typename T>
	inline void swap(const Bit& swap, T& o1, T& o2) {
		T o = If(swap, o1, o2);
		if constexpr (std::is_same_v<T, Integer<o1.size()>>) {
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(o1.bits.data()), o1.size() * sizeof(Bit), false, false, );
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(o2.bits.data()), o2.size() * sizeof(Bit), false, false, );
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(o.bits.data()), o.size() * sizeof(Bit), true, true, );
		}
		o ^= o2;
		o1 ^= o;
		o2 ^= o;
	}
}
#endif
