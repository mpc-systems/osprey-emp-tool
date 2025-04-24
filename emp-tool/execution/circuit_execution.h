#ifndef EMP_CIRCUIT_EXECUTION_H__
#define EMP_CIRCUIT_EXECUTION_H__
#include "emp-tool/io/highspeed_net_io_channel.h"
#include "emp-tool/io/net_io_channel.h"
#include "emp-tool/utils/block.h"
#include "emp-tool/utils/constants.h"
#include "emp-tool/utils/mitccrh.h"
#include "emp-tool/utils/prg.h"

namespace emp {

	/*
	 * The half-gate garbling scheme, with improved hashing
	 * [REF] Implementation of "Two Halves Make a Whole"
	 * https://eprint.iacr.org/2014/756.pdf
	 */
	inline block halfgates_garble(block LA0, block A1, block LB0, block B1, block delta, block* table,
								  MITCCRH<8>* mitccrh) {
		bool pa = getLSB(LA0);
		bool pb = getLSB(LB0);
		block HLA0, HA1, HLB0, HB1;
		block tmp, W0;

		block H[4];
		H[0] = LA0;
		H[1] = A1;
		H[2] = LB0;
		H[3] = B1;
		mitccrh->hash<2, 2>(H);
		HLA0 = H[0];
		HA1 = H[1];
		HLB0 = H[2];
		HB1 = H[3];

		table[0] = HLA0 ^ HA1;
		table[0] = table[0] ^ (select_mask[pb] & delta);
		W0 = HLA0;
		W0 = W0 ^ (select_mask[pa] & table[0]);
		tmp = HLB0 ^ HB1;
		table[1] = tmp ^ LA0;
		W0 = W0 ^ HLB0;
		W0 = W0 ^ (select_mask[pb] & tmp);

		return W0;
	}

	inline block halfgates_eval(block A, block B, const block* table, MITCCRH<8>* mitccrh) {
		block HA, HB, W;
		int sa, sb;

		sa = getLSB(A);
		sb = getLSB(B);

		block H[2];
		H[0] = A;
		H[1] = B;
		mitccrh->hash<2, 1>(H);
		HA = H[0];
		HB = H[1];

		W = HA ^ HB;
		W = W ^ (select_mask[sa] & table[0]);
		W = W ^ (select_mask[sb] & table[1]);
		W = W ^ (select_mask[sb] & A);
		return W;
	}

	/* Circuit Pipelining
	 * [REF] Implementation of "Faster Secure Two-Party Computation Using Garbled Circuit"
	 * https://www.usenix.org/legacy/event/sec11/tech/full_papers/Huang.pdf
	 */
	template <class T>
	class HalfGateParty {
	public:
		block and_gate(const block& in1, const block& in2) {
			return static_cast<T*>(this)->and_gate(in1, in2);
		}
		block xor_gate(const block& in1, const block& in2) {
			return static_cast<T*>(this)->xor_gate(in1, in2);
		}
		block not_gate(const block& in1) {
			return static_cast<T*>(this)->not_gate(in1);
		}
		block public_label(bool b) {
			return static_cast<T*>(this)->public_label(b);
		}
		uint64_t num_and() {
			return static_cast<T*>(this)->num_and();
		}
	};

	template <typename IO>
	class HalfGateGen : public HalfGateParty<HalfGateGen<IO>> {
	public:
		block delta;
		IO* io;
		block constant[2];
		MITCCRH<8> mitccrh;
		HalfGateGen(IO* io) : io(io) {
			block tmp[2];
			PRG().random_block(tmp, 2);
			set_delta(tmp[0]);
			io->send_block(tmp + 1, 1);
			mitccrh.setS(tmp[1]);
		}
		void set_delta(const block& _delta) {
			delta = set_bit(_delta, 0);
			PRG().random_block(constant, 2);
			io->send_block(constant, 2);
			constant[1] = constant[1] ^ delta;
		}
		block public_label(bool b) {
			return constant[b];
		}
		block and_gate(const block& a, const block& b) {
			block table[2];
			block res = halfgates_garble(a, a ^ delta, b, b ^ delta, delta, table, &mitccrh);
			io->send_block(table, 2);
			return res;
		}
		block xor_gate(const block& a, const block& b) {
			return a ^ b;
		}
		block not_gate(const block& a) {
			return xor_gate(a, public_label(true));
		}
		uint64_t num_and() {
			return mitccrh.gid / 2;
		}
	};

	template <typename IO>
	class HalfGateEva : public HalfGateParty<HalfGateEva<IO>> {
	public:
		IO* io;
		block constant[2];
		MITCCRH<8> mitccrh;
		HalfGateEva(IO* io) : io(io) {
			set_delta();
			block tmp;
			io->recv_block(&tmp, 1);
			mitccrh.setS(tmp);
		}
		void set_delta() {
			io->recv_block(constant, 2);
		}
		block public_label(bool b) {
			return constant[b];
		}
		block and_gate(const block& a, const block& b) {
			block table[2];
			io->recv_block(table, 2);
			return halfgates_eval(a, b, table, &mitccrh);
		}
		block xor_gate(const block& a, const block& b) {
			return a ^ b;
		}
		block not_gate(const block& a) {
			return xor_gate(a, public_label(true));
		}
		uint64_t num_and() {
			return mitccrh.gid / 2;
		}
	};

	template <int party, typename IO>
	struct HalfGateInternal {
		inline static std::conditional<party == ALICE, HalfGateGen<IO>, HalfGateEva<IO>>::type* circ_exec = nullptr;
	};

#ifdef PARTY
	using CircuitExecution = HalfGateInternal<PARTY, HighSpeedNetIO>;
#else
#error "Party must be defined at compile time"
#endif

	enum RTCktOpt {
		on,
		off
	};
}
#endif
