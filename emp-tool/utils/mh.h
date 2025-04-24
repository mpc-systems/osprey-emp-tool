#ifndef EMP_MH_H__
#define EMP_MH_H__
#include <stdio.h>

#include "emp-tool/utils/prp.h"

namespace emp {

	/*
	 * By default, MH use zero_block as the AES key.
	 * Here we model f(x) = AES_{00..0}(x) as a random permutation (and thus in the RPM model)
	 */
	class MH : public PRP {
	public:
		MH(const block& key = zero_block) : PRP(key) {
		}

#ifdef __GNUC__
#ifndef __clang__
#pragma GCC push_options
#pragma GCC optimize("unroll-loops")
#endif
#endif
		template <std::size_t K, std::size_t H>
		void hash(block out[K * H], block in[K * H]) {
			gid++;

			block tmp[K * H];
			for (std::size_t i = 0; i < K; i++) {
				block tweak = makeBlock(gid * K + i, 0);
				for (std::size_t j = 0; j < H; j++) {
					out[i * H + j] = in[i * H + j] ^ tweak;
					tmp[i * H + j] = out[i * H + j];
				}
			}

			AES_ecb_encrypt_blks(out, K * H, &(this->aes));

			for (std::size_t i = 0; i < K * H; i++) {
				out[i] = out[i] ^ tmp[i];
			}
		}

	private:
		std::uint64_t gid = 0;
	};
}
#endif // MH_H__
