#ifndef EMP_PROTOCOL_EXECUTION_H__
#define EMP_PROTOCOL_EXECUTION_H__
#include <pthread.h>

#include "emp-tool/execution/circuit_execution.h"
#include "emp-tool/io/highspeed_net_io_channel.h"
#include "emp-tool/ot/iknp.h"
#include "emp-tool/utils/block.h"
#include "emp-tool/utils/constants.h"

namespace emp {
	template <class T, typename IO>
	class SemiHonestParty {
	public:
		IO* io = nullptr;
		IKNP<IO>* ot = nullptr;
		PRG shared_prg;

		block* buf = nullptr;
		bool* buff = nullptr;
		int top = 0;
		int batch_size = 1024 * 16;

		SemiHonestParty(IO* io, int party) {
			this->io = io;
			ot = new IKNP<IO>(io);
			buf = new block[batch_size];
			buff = new bool[batch_size];
		}

		void set_batch_size(int size) {
			delete[] buf;
			delete[] buff;
			batch_size = size;
			buf = new block[batch_size];
			buff = new bool[batch_size];
		}

		~SemiHonestParty() {
			delete[] buf;
			delete[] buff;
			delete ot;
		}

		void feed(block* lbls, int party, const bool* b, int nel) {
			static_cast<T*>(this)->feed(lbls, party, b, nel);
		}
		void reveal(bool* out, int party, const block* lbls, int nel) {
			static_cast<T*>(this)->reveal(out, party, lbls, nel);
		}
		void finalize() {
			static_cast<T*>(this)->finalize();
		}
	};

	template <typename IO>
	class SemiHonestGen : public SemiHonestParty<SemiHonestGen<IO>, IO> {
	public:
		HalfGateGen<IO>* gc;
		SemiHonestGen(IO* io, HalfGateGen<IO>* gc) : SemiHonestParty<SemiHonestGen<IO>, IO>(io, ALICE) {
			this->gc = gc;
			bool delta_bool[128];
			block_to_bool(delta_bool, gc->delta);
			this->ot->setup_send(delta_bool);
			block seed;
			PRG prg;
			prg.random_block(&seed, 1);
			this->io->send_block(&seed, 1);
			this->shared_prg.reseed(&seed);
			refill();
		}

		void refill() {
			this->ot->send_cot(this->buf, this->batch_size);
			this->top = 0;
		}

		void feed(block* label, int party, const bool* b, int length) {
			if (party == ALICE) {
				this->shared_prg.random_block(label, length);
				for (int i = 0; i < length; ++i) {
					if (b[i])
						label[i] = label[i] ^ gc->delta;
				}
			} else {
				if (length > this->batch_size) {
					this->ot->send_cot(label, length);
				} else {
					bool* tmp = new bool[length];
					if (length > this->batch_size - this->top) {
						memcpy(label, this->buf + this->top, (this->batch_size - this->top) * sizeof(block));
						int filled = this->batch_size - this->top;
						refill();
						memcpy(label + filled, this->buf, (length - filled) * sizeof(block));
						this->top = (length - filled);
					} else {
						memcpy(label, this->buf + this->top, length * sizeof(block));
						this->top += length;
					}

					this->io->recv_data(tmp, length);
					for (int i = 0; i < length; ++i)
						if (tmp[i])
							label[i] = label[i] ^ gc->delta;
					delete[] tmp;
				}
			}
		}

		void reveal(bool* b, int party, const block* label, int length) {
			if (party == XOR) {
				for (int i = 0; i < length; ++i)
					b[i] = getLSB(label[i]);
				return;
			}
			std::vector<std::uint8_t> lsbs(length);
			for (int i = 0; i < length; ++i) {
				lsbs[i] = getLSB(label[i]);
			}
			if (party == BOB or party == PUBLIC) {
				this->io->send_data(lsbs.data(), length);
				for (int i = 0; i < length; ++i)
					b[i] = false;
			} else if (party == ALICE) {
				std::vector<std::uint8_t> tmps(length);
				this->io->recv_data(tmps.data(), length);
				for (int i = 0; i < length; ++i)
					b[i] = (tmps[i] != lsbs[i]);
			}
			if (party == PUBLIC)
				this->io->recv_data(b, length);
		}
	};

	template <typename IO>
	class SemiHonestEva : public SemiHonestParty<SemiHonestEva<IO>, IO> {
	public:
		HalfGateEva<IO>* gc;
		PRG prg;
		SemiHonestEva(IO* io, HalfGateEva<IO>* gc) : SemiHonestParty<SemiHonestEva<IO>, IO>(io, BOB) {
			this->gc = gc;
			this->ot->setup_recv();
			block seed;
			this->io->recv_block(&seed, 1);
			this->shared_prg.reseed(&seed);
			refill();
		}

		void refill() {
			prg.random_bool(this->buff, this->batch_size);
			this->ot->recv_cot(this->buf, this->buff, this->batch_size);
			this->top = 0;
		}

		void feed(block* label, int party, const bool* b, int length) {
			if (party == ALICE) {
				this->shared_prg.random_block(label, length);
			} else {
				if (length > this->batch_size) {
					this->ot->recv_cot(label, b, length);
				} else {
					bool* tmp = new bool[length];
					if (length > this->batch_size - this->top) {
						memcpy(label, this->buf + this->top, (this->batch_size - this->top) * sizeof(block));
						memcpy(tmp, this->buff + this->top, (this->batch_size - this->top));
						int filled = this->batch_size - this->top;
						refill();
						memcpy(label + filled, this->buf, (length - filled) * sizeof(block));
						memcpy(tmp + filled, this->buff, length - filled);
						this->top = length - filled;
					} else {
						memcpy(label, this->buf + this->top, length * sizeof(block));
						memcpy(tmp, this->buff + this->top, length);
						this->top += length;
					}

					for (int i = 0; i < length; ++i)
						tmp[i] = (tmp[i] != b[i]);
					this->io->send_data(tmp, length);

					delete[] tmp;
				}
			}
		}

		void reveal(bool* b, int party, const block* label, int length) {
			if (party == XOR) {
				for (int i = 0; i < length; ++i)
					b[i] = getLSB(label[i]);
				return;
			}
			std::vector<std::uint8_t> lsbs(length);
			for (int i = 0; i < length; ++i) {
				lsbs[i] = getLSB(label[i]);
			}
			if (party == BOB or party == PUBLIC) {
				std::vector<std::uint8_t> tmps(length);
				this->io->recv_data(tmps.data(), length);
				for (int i = 0; i < length; ++i)
					b[i] = (tmps[i] != lsbs[i]);
			} else if (party == ALICE) {
				this->io->send_data(lsbs.data(), length);
				for (int i = 0; i < length; ++i)
					b[i] = false;
			}
			if (party == PUBLIC)
				this->io->send_data(b, length);
		}
	};

	template <int party, typename IO>
	struct SemiHonestInternal {
		inline static std::conditional<party == ALICE, SemiHonestGen<IO>, SemiHonestEva<IO>>::type* prot_exec = nullptr;
	};

#ifdef PARTY
	using ProtocolExecution = SemiHonestInternal<PARTY, HighSpeedNetIO>;
#else
#error "Party must be defined at compile time"
#endif
}
#endif
