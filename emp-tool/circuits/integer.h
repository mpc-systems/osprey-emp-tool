#ifndef EMP_INTEGER_H__
#define EMP_INTEGER_H__

#include <math.h>

#include <algorithm>
#include <vector>
#include <bitset>

#include "emp-tool/circuits/bit.h"
#include "emp-tool/circuits/number.h"

#include "lib/allocator.hpp"
#include "util/address.hpp"

using std::vector;
using std::min;
namespace emp {

	// https://github.com/samee/obliv-c/blob/obliv-c/src/ext/oblivc/obliv_bits.c#L1487
	inline void add_full(Bit* dest, Bit* carryOut, const Bit* op1, const Bit* op2, const Bit* carryIn, int size) {
		Bit carry, bxc, axc, t;
		int skipLast;
		int i = 0;
		if (size == 0) {
			if (carryIn && carryOut)
				*carryOut = *carryIn;
			return;
		}
		if (carryIn)
			carry = *carryIn;
		else
			carry = false;
		// skip AND on last bit if carryOut==NULL
		skipLast = (carryOut == nullptr);
		while (size-- > skipLast) {
			axc = op1[i] ^ carry;
			bxc = op2[i] ^ carry;
			dest[i] = op1[i] ^ bxc;
			t = axc & bxc;
			carry = carry ^ t;
			++i;
		}
		if (carryOut != nullptr)
			*carryOut = carry;
		else
			dest[i] = carry ^ op2[i] ^ op1[i];
	}

	inline void sub_full(Bit* dest, Bit* borrowOut, const Bit* op1, const Bit* op2, const Bit* borrowIn, int size) {
		Bit borrow, bxc, bxa, t;
		int skipLast;
		int i = 0;
		if (size == 0) {
			if (borrowIn && borrowOut)
				*borrowOut = *borrowIn;
			return;
		}
		if (borrowIn)
			borrow = *borrowIn;
		else
			borrow = false;
		// skip AND on last bit if borrowOut==NULL
		skipLast = (borrowOut == nullptr);
		while (size-- > skipLast) {
			bxa = op1[i] ^ op2[i];
			bxc = borrow ^ op2[i];
			dest[i] = bxa ^ borrow;
			t = bxa & bxc;
			borrow = borrow ^ t;
			++i;
		}
		if (borrowOut != nullptr) {
			*borrowOut = borrow;
		} else
			dest[i] = op1[i] ^ op2[i] ^ borrow;
	}

	inline void mul_full(Bit* dest, const Bit* op1, const Bit* op2, int size) {
		Bit* sum = new Bit[size];
		Bit* temp = new Bit[size];
		for (int i = 0; i < size; ++i)
			sum[i] = false;
		for (int i = 0; i < size; ++i) {
			for (int k = 0; k < size - i; ++k)
				temp[k] = op1[k] & op2[i];
			add_full(sum + i, nullptr, sum + i, temp, nullptr, size - i);
		}
		memcpy(dest, sum, sizeof(Bit) * size);
		delete[] sum;
		delete[] temp;
	}

	inline void ifThenElse(Bit* dest, const Bit* tsrc, const Bit* fsrc, int size, Bit cond) {
		Bit x, a;
		int i = 0;
		while (size-- > 0) {
			x = tsrc[i] ^ fsrc[i];
			a = cond & x;
			dest[i] = a ^ fsrc[i];
			++i;
		}
	}

	inline void condNeg(Bit cond, Bit* dest, const Bit* src, int size) {
		int i;
		Bit c = cond;
		for (i = 0; i < size - 1; ++i) {
			dest[i] = src[i] ^ cond;
			Bit t = dest[i] ^ c;
			c = c & dest[i];
			dest[i] = t;
		}
		dest[i] = cond ^ c ^ src[i];
	}

	inline void div_full(Bit* vquot, Bit* vrem, const Bit* op1, const Bit* op2, int size) {
		Bit* overflow = new Bit[size];
		Bit* temp = new Bit[size];
		Bit* rem = new Bit[size];
		Bit* quot = new Bit[size];
		Bit b;
		memcpy(rem, op1, size * sizeof(Bit));
		overflow[0] = false;
		for (int i = 1; i < size; ++i)
			overflow[i] = overflow[i - 1] | op2[size - i];
		// skip AND on last bit if borrowOut==NULL
		for (int i = size - 1; i >= 0; --i) {
			sub_full(temp, &b, rem + i, op2, nullptr, size - i);
			b = b | overflow[i];
			ifThenElse(rem + i, rem + i, temp, size - i, b);
			quot[i] = !b;
		}
		if (vrem != nullptr)
			memcpy(vrem, rem, size * sizeof(Bit));
		if (vquot != nullptr)
			memcpy(vquot, quot, size * sizeof(Bit));
		delete[] overflow;
		delete[] temp;
		delete[] rem;
		delete[] quot;
	}

	template <std::size_t nbits>
	class Integer {
	public:
		vector<Bit, osprey::lib::ContentObliviousAllocator<Bit>> bits;

		Integer() : bits(nbits) {
		}

		Integer(const vector<Bit, osprey::lib::ContentObliviousAllocator<Bit>>& bits) : bits(bits) {
		}

		Integer(int64_t input, int party = PUBLIC) : bits(nbits) {
			bool b[nbits] = {false};
			int_to_bool<int64_t>(b, input, nbits);
			init(b, party);
		}

		template <typename T>
		Integer(T* input, int party = PUBLIC) requires(!std::is_same_v<T, Bit>) : bits(nbits) {
			bool b[nbits] = {false};
			to_bool<T>(b, input, nbits);
			init(b, party);
		}

		template <typename T = Bit>
		Integer(Bit* input, int party = PUBLIC) requires(std::is_same_v<T, Bit>) : bits(nbits) {
			std::memcpy(bits.data(), input, nbits * sizeof(Bit));
		}

		Integer(const std::bitset<nbits>& input, int party = PUBLIC) : bits(nbits) {
			bool b[nbits] = {false};
			for (size_t i = 0; i < nbits; ++i)
				b[i] = input[i];
			init(b, party);
		}

		// Comparable
		Bit geq(const Integer<nbits>& rhs) const {
			assert(size() == rhs.size());
			Integer<nbits + 1> thisExtend = this->resize<nbits + 1>(true);
			Integer<nbits + 1> rhsExtend = rhs.resize<nbits + 1>(true);
			Integer<nbits + 1> tmp = thisExtend - rhsExtend;
			return !tmp[tmp.size() - 1];
		}

		Bit equal(const Integer<nbits>& rhs) const {
			assert(size() == rhs.size());
			Bit res(true);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(this->bits.data()), size() * sizeof(Bit), false, false, res);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(rhs.bits.data()), rhs.size() * sizeof(Bit), false, true, res);
			for (size_t i = 0; i < size(); ++i)
				res = res & (bits[i] == rhs[i]);
			return res;
		}

		Bit operator>=(const Integer<nbits>& rhs) const {
			return static_cast<const Integer<nbits>*>(this)->geq(rhs);
		}

		Bit operator<(const Integer<nbits>& rhs) const {
			return !((*static_cast<const Integer<nbits>*>(this)) >= rhs);
		}

		Bit operator<=(const Integer<nbits>& rhs) const {
			return rhs >= *static_cast<const Integer<nbits>*>(this);
		}

		Bit operator>(const Integer<nbits>& rhs) const {
			return !(rhs >= *static_cast<const Integer<nbits>*>(this));
		}

		Bit operator==(const Integer<nbits>& rhs) const {
			return static_cast<const Integer<nbits>*>(this)->equal(rhs);
		}

		Bit operator!=(const Integer<nbits>& rhs) const {
			return !(*static_cast<const Integer<nbits>*>(this) == rhs);
		}

		// Swappable
		Integer<nbits> select(const Bit& sel, const Integer<nbits>& rhs) const {
			Integer<nbits> res;
			OSPREY_TOUCH_RANGE(
				reinterpret_cast<std::uintptr_t>(this->bits.data()), this->size() * sizeof(Bit), false, false, res
			);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(rhs.bits.data()), rhs.size() * sizeof(Bit), false, false, res);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(res.bits.data()), res.size() * sizeof(Bit), true, true, res);

			for (size_t i = 0; i < size(); ++i)
				bits[i].select(sel, rhs[i], res[i]);
			return res;
		}

		Integer<nbits> If(const Bit& sel, const Integer<nbits>& rhs) const {
			return this->select(sel, rhs);
		}

		constexpr size_t size() const {
			return nbits;
		}

		std::bitset<nbits> reveal(int party = PUBLIC) const {
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(this->bits.data()), size() * sizeof(Bit), false, true, 0);
			std::bitset<nbits> bs;
			bool b[size()];
			revealBools(b, party);
			for (size_t i = 0; i < nbits; ++i)
				bs.set(i, b[i]);
			return bs;
		}

		Integer<nbits> abs() const {
			Integer<nbits> res(*this);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(this->bits.data()), size() * sizeof(Bit), false, false, res);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(res.bits.data()), res.size() * sizeof(Bit), true, true, res);
			for (size_t i = 0; i < size(); ++i)
				res[i] = bits[size() - 1];
			return ((*this) + res) ^ res;
		}

		template <std::size_t newnbits>
		Integer<newnbits> resize(bool signed_extend = true) const {
			static_assert(newnbits >= nbits, "newnbits should be greater than nbits");
			Bit MSB(false, PUBLIC);
			if (signed_extend)
				MSB = bits[bits.size() - 1];
			Integer<newnbits> res;
			std::copy(bits.begin(), bits.end(), res.bits.begin());
			for (size_t i = size(); i < newnbits; ++i)
				res[i] = MSB;
			return res;
		}

		Integer<nbits> modExp(Integer<nbits> p, Integer<nbits> q) {
			// the value of q should be less than half of the MAX_INT
			Integer<nbits> base = *this;
			Integer<nbits> res(size(), 1);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(this->bits.data()), size() * sizeof(Bit), false, false, res);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(p.bits.data()), p.size() * sizeof(Bit), false, false, res);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(q.bits.data()), q.size() * sizeof(Bit), false, false, res);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(res.bits.data()), res.size() * sizeof(Bit), true, true, res);

			for (size_t i = 0; i < p.size(); ++i) {
				Integer<nbits> tmp = (res * base) % q;
				res = res.select(p[i], tmp);
				base = (base * base) % q;
			}
			return res;
		}

		// Logical operations
		inline Integer<nbits> operator^(const Integer<nbits>& rhs) const {
			Integer<nbits> res(*this);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(this->bits.data()), size() * sizeof(Bit), false, false, res);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(rhs.bits.data()), rhs.size() * sizeof(Bit), false, false, res);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(res.bits.data()), res.size() * sizeof(Bit), true, true, res);
			for (size_t i = 0; i < size(); ++i)
				res.bits[i] = res.bits[i] ^ rhs.bits[i];
			return res;
		}

		inline Integer<nbits> operator^=(const Integer<nbits>& rhs) {
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(rhs.bits.data()), rhs.size() * sizeof(Bit), false, false, rhs);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(this->bits.data()), size() * sizeof(Bit), true, true, *(this));
			for (size_t i = 0; i < size(); ++i)
				this->bits[i] ^= rhs.bits[i];
			return (*this);
		}

		inline Integer<nbits> operator|(const Integer<nbits>& rhs) const {
			Integer<nbits> res(*this);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(this->bits.data()), size() * sizeof(Bit), false, false, res);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(rhs.bits.data()), rhs.size() * sizeof(Bit), false, false, res);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(res.bits.data()), res.size() * sizeof(Bit), true, true, res);

			for (size_t i = 0; i < size(); ++i)
				res.bits[i] = res.bits[i] | rhs.bits[i];
			return res;
		}

		inline Integer<nbits> operator&(const Integer<nbits>& rhs) const {
			Integer<nbits> res(*this);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(this->bits.data()), size() * sizeof(Bit), false, false, res);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(rhs.bits.data()), rhs.size() * sizeof(Bit), false, false, res);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(res.bits.data()), res.size() * sizeof(Bit), true, true, res);

			for (size_t i = 0; i < size(); ++i)
				res.bits[i] = res.bits[i] & rhs.bits[i];
			return res;
		}

		inline Integer<nbits> operator<<(size_t shamt) const {
			Integer<nbits> res(*this);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(this->bits.data()), size() * sizeof(Bit), false, false, res);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(res.bits.data()), res.size() * sizeof(Bit), true, true, res);
			if (shamt > size()) {
				for (size_t i = 0; i < size(); ++i)
					res.bits[i] = false;
			} else {
				for (size_t i = size() - 1; i >= shamt; --i)
					res.bits[i] = bits[i - shamt];
				for (size_t i = shamt - 1; i >= 0; --i)
					res.bits[i] = false;
			}
			return res;
		}

		inline Integer<nbits> operator>>(size_t shamt) const {
			Integer<nbits> res(*this);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(this->bits.data()), size() * sizeof(Bit), false, false, res);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(res.bits.data()), res.size() * sizeof(Bit), true, true, res);

			if (shamt > size()) {
				for (size_t i = 0; i < size(); ++i)
					res.bits[i] = false;
			} else {
				for (size_t i = shamt; i < size(); ++i)
					res.bits[i - shamt] = bits[i];
				for (size_t i = size() - shamt; i < size(); ++i)
					res.bits[i] = false;
			}
			return res;
		}

		inline Integer<nbits> operator<<(const Integer<nbits>& shamt) const {
			Integer<nbits> res(*this);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(this->bits.data()), size() * sizeof(Bit), false, false, res);
			OSPREY_TOUCH_RANGE(
				reinterpret_cast<std::uintptr_t>(shamt.bits.data()), shamt.size() * sizeof(Bit), false, false, res
			);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(res.bits.data()), res.size() * sizeof(Bit), true, true, res);

			for (size_t i = 0; i < min(size_t(ceil(log2(size()))), shamt.size() - 1); ++i)
				res = res.select(shamt[i], res << (1 << i));
			return res;
		}

		inline Integer<nbits> operator>>(const Integer<nbits>& shamt) const {
			Integer<nbits> res(*this);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(this->bits.data()), size() * sizeof(Bit), false, false, res);
			OSPREY_TOUCH_RANGE(
				reinterpret_cast<std::uintptr_t>(shamt.bits.data()), shamt.size() * sizeof(Bit), false, false, res
			);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(res.bits.data()), res.size() * sizeof(Bit), true, true, res);

			for (size_t i = 0; i < min(size_t(ceil(log2(size()))), shamt.size() - 1); ++i)
				res = res.select(shamt[i], res >> (1 << i));
			return res;
		}

		inline Integer<nbits> operator+(const Integer<nbits>& rhs) const {
			assert(size() == rhs.size());
			Integer<nbits> res(*this);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(this->bits.data()), size() * sizeof(Bit), false, false, res);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(rhs.bits.data()), rhs.size() * sizeof(Bit), false, false, res);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(res.bits.data()), res.size() * sizeof(Bit), true, true, res);

			add_full(res.bits.data(), nullptr, bits.data(), rhs.bits.data(), nullptr, size());
			return res;
		}

		inline Integer<nbits> operator-(const Integer<nbits>& rhs) const {
			assert(size() == rhs.size());
			Integer<nbits> res(*this);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(this->bits.data()), size() * sizeof(Bit), false, false, res);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(rhs.bits.data()), rhs.size() * sizeof(Bit), false, false, res);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(res.bits.data()), res.size() * sizeof(Bit), true, true, res);

			sub_full(res.bits.data(), nullptr, bits.data(), rhs.bits.data(), nullptr, size());
			return res;
		}

		inline Integer<nbits> operator*(const Integer<nbits>& rhs) const {
			assert(size() == rhs.size());
			Integer<nbits> res(*this);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(this->bits.data()), size() * sizeof(Bit), false, false, res);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(rhs.bits.data()), rhs.size() * sizeof(Bit), false, false, res);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(res.bits.data()), res.size() * sizeof(Bit), true, true, res);

			mul_full(res.bits.data(), bits.data(), rhs.bits.data(), size());
			return res;
		}

		inline Integer<nbits> operator/(const Integer<nbits>& rhs) const {
			assert(size() == rhs.size());
			Integer<nbits> res(*this);
			Integer<nbits> i1 = abs();
			Integer<nbits> i2 = rhs.abs();
			Bit sign = bits[size() - 1] ^ rhs[size() - 1];

			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(i1.bits.data()), i1.size() * sizeof(Bit), false, false, res);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(i2.bits.data()), i2.size() * sizeof(Bit), false, false, res);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(res.bits.data()), res.size() * sizeof(Bit), true, true, res);

			div_full(res.bits.data(), nullptr, i1.bits.data(), i2.bits.data(), size());
			condNeg(sign, res.bits.data(), res.bits.data(), size());
			return res;
		}

		inline Integer<nbits> operator%(const Integer<nbits>& rhs) const {
			assert(size() == rhs.size());
			Integer<nbits> res(*this);
			Integer<nbits> i1 = abs();
			Integer<nbits> i2 = rhs.abs();
			Bit sign = bits[size() - 1];

			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(i1.bits.data()), i1.size() * sizeof(Bit), false, false, res);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(i2.bits.data()), i2.size() * sizeof(Bit), false, false, res);
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(res.bits.data()), res.size() * sizeof(Bit), true, true, res);

			div_full(nullptr, res.bits.data(), i1.bits.data(), i2.bits.data(), size());
			condNeg(sign, res.bits.data(), res.bits.data(), size());
			return res;
		}

		inline Integer<nbits> operator-() const {
			return Integer<nbits>(size(), 0, PUBLIC) - (*this);
		}

		inline Bit& operator[](size_t index) {
			return bits[min(index, size() - 1)];
		}

		const Bit& operator[](size_t index) const {
			return bits[min(index, size() - 1)];
		}

		void init(bool* b, int party) {
			OSPREY_TOUCH_RANGE(reinterpret_cast<std::uintptr_t>(this->bits.data()), size() * sizeof(Bit), false, true, );
			if (party == PUBLIC) {
				block one = CircuitExecution::circ_exec->public_label(true);
				block zero = CircuitExecution::circ_exec->public_label(false);
				for (std::size_t i = 0; i < nbits; ++i)
					bits[i] = b[i] ? one : zero;
			} else {
				ProtocolExecution::prot_exec->feed((block*) bits.data(), party, b, nbits);
			}
		}

		void revealBools(bool* bools, int party = PUBLIC) const {
			ProtocolExecution::prot_exec->reveal(bools, party, (block*) bits.data(), size());
		}
	};
}
#endif // INTEGER_H__
