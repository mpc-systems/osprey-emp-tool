#ifndef EMP_BIT_H__
#define EMP_BIT_H__
#include "emp-tool/circuits/swappable.h"
#include "emp-tool/execution/circuit_execution.h"
#include "emp-tool/execution/protocol_execution.h"
#include "emp-tool/utils/block.h"
#include "emp-tool/utils/utils.h"

namespace emp {
	class Bit {
	public:
		block bit;

		Bit(bool _b = false, int party = PUBLIC) {
			if (party == PUBLIC)
				bit = CircuitExecution::circ_exec->public_label(_b);
			else
				ProtocolExecution::prot_exec->feed(&bit, party, &_b, 1);
		}

		Bit(const block& a) {
			memcpy(&bit, &a, sizeof(block));
		}

		Bit operator!=(const Bit& rhs) const {
			return (*this) ^ rhs;
		}

		Bit operator==(const Bit& rhs) const {
			return !(*this ^ rhs);
		}

		Bit operator&(const Bit& rhs) const {
			Bit res;
			res.bit = CircuitExecution::circ_exec->and_gate(bit, rhs.bit);
			return res;
		}

		Bit operator|(const Bit& rhs) const {
			return (*this ^ rhs) ^ (*this & rhs);
		}

		Bit operator!() const {
			return CircuitExecution::circ_exec->not_gate(bit);
		}

		// swappable
		Bit select(const Bit& select, const Bit& new_v) const {
			Bit tmp = *this;
			tmp = tmp ^ new_v;
			tmp = tmp & select;
			return *this ^ tmp;
		}

		Bit If(const Bit& sel, const Bit& rhs) const {
			return static_cast<const Bit*>(this)->select(sel, rhs);
		}

		Bit operator^(const Bit& rhs) const {
			Bit res;
			res.bit = CircuitExecution::circ_exec->xor_gate(bit, rhs.bit);
			return res;
		}

		Bit operator^=(const Bit& rhs) {
			this->bit = CircuitExecution::circ_exec->xor_gate(bit, rhs.bit);
			return (*this);
		}

		// batcher
		template <typename... Args>
		static size_t bool_size(Args&&... args) {
			return 1;
		}

		static void bool_data(bool* b, bool data) {
			b[0] = data;
		}
	};
}
#endif
