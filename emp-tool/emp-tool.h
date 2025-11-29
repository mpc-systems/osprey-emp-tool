#ifndef EMP_TOOL_H
#define EMP_TOOL_H

#include <thread>

#include "emp-tool/circuits/bit.h"
#include "emp-tool/circuits/integer.h"
#include "emp-tool/circuits/number.h"
#include "emp-tool/circuits/swappable.h"
#include "emp-tool/execution/circuit_execution.h"
#include "emp-tool/execution/protocol_execution.h"
#include "emp-tool/io/file_io_channel.h"
#include "emp-tool/io/highspeed_net_io_channel.h"
#include "emp-tool/io/io_channel.h"
#include "emp-tool/io/mem_io_channel.h"
#include "emp-tool/io/net_io_channel.h"
#include "emp-tool/utils/ThreadPool.h"
#include "emp-tool/utils/aes.h"
#include "emp-tool/utils/aes_opt.h"
#include "emp-tool/utils/block.h"
#include "emp-tool/utils/ccrh.h"
#include "emp-tool/utils/constants.h"
#include "emp-tool/utils/crh.h"
#include "emp-tool/utils/f2k.h"
#include "emp-tool/utils/group.h"
#include "emp-tool/utils/hash.h"
#include "emp-tool/utils/mitccrh.h"
#include "emp-tool/utils/prg.h"
#include "emp-tool/utils/prp.h"
#include "emp-tool/utils/tccrh.h"
#include "emp-tool/utils/utils.h"

#include "util/address.hpp"

namespace emp {

	template <int party, typename IO>
	inline void setup_semi_honest(IO* io, int batch_size = 1024 * 16) {
		OSPREY_TOUCH_RANGE(0, 0, false, true, );
		if constexpr (party == ALICE) {
			HalfGateGen<IO>* t = new HalfGateGen<IO>(io);
			CircuitExecution::circ_exec = t;
			ProtocolExecution::prot_exec = new SemiHonestGen<IO>(io, t);
		} else {
			HalfGateEva<IO>* t = new HalfGateEva<IO>(io);
			CircuitExecution::circ_exec = t;
			ProtocolExecution::prot_exec = new SemiHonestEva<IO>(io, t);
		}
	}
}

#endif // EMP_TOOL_H
