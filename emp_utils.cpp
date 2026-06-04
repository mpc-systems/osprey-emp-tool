#include <fcntl.h>
#include <unistd.h>

#include <array>
#include <bitset>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "emp-tool/emp-tool.h"

using namespace emp;

double get_cpu_time_ms() {
	pid_t pid = getpid();
	std::ifstream stat_file("/proc/" + std::to_string(pid) + "/stat");
	if (!stat_file.is_open()) {
		throw std::runtime_error("Failed to open /proc/[pid]/stat");
	}

	std::string token;
	long utime_ticks = 0, stime_ticks = 0;
	for (int i = 1; i <= 15; ++i) {
		stat_file >> token;
		if (i == 14)
			utime_ticks = std::stol(token);
		if (i == 15)
			stime_ticks = std::stol(token);
	}

	long ticks_per_sec = sysconf(_SC_CLK_TCK);
	double total_ms = (utime_ticks + stime_ticks) * 1000.0 / ticks_per_sec;
	return total_ms;
}

template <std::size_t width, std::size_t bs>
bool read_from_file(const std::string& file, std::vector<std::bitset<width>>& data) {
	static_assert(width % 8 == 0, "Width must be multiple of 8");

	constexpr std::size_t bytes = width / 8;

	std::ifstream stream(file, std::ios::binary);
	if (!stream.is_open()) {
		std::cout << "Open file failed" << std::endl;
		return false;
	}

	std::size_t read_size = 0;
	std::array<std::byte, bytes * bs> buffer;

	std::vector<std::byte> bdata;
	while ((read_size = stream.readsome(reinterpret_cast<char*>(buffer.data()), sizeof(buffer))) > 0) {
		bdata.insert(bdata.end(), buffer.begin(), buffer.begin() + read_size);
	}

	stream.close();

	std::uintptr_t start = reinterpret_cast<std::uintptr_t>(bdata.data());
	std::size_t size = bdata.size();

	for (std::uintptr_t addr = start; addr < start + size; addr += bytes) {
		std::byte* bitems = reinterpret_cast<std::byte*>(addr);
		std::bitset<width> item(0);
		for (std::size_t i = 0; i < bytes; i++) {
			item |= std::bitset<width>(std::to_integer<int>(bitems[i])) << (i * 8);
		}
		data.push_back(item);
	}

	return true;
}

template <std::size_t width, std::size_t bs>
bool write_to_file(const std::string& file, const std::vector<std::bitset<width>>& data) {
	static_assert(width % 8 == 0, "Width must be multiple of 8");

	constexpr std::size_t bytes = width / 8;

	std::ofstream stream(file, std::ios::binary);
	if (!stream.is_open()) {
		std::cout << "Open file failed" << std::endl;
		return false;
	}

	std::array<std::byte, bytes * bs> buffer;
	stream.rdbuf()->pubsetbuf(reinterpret_cast<char*>(buffer.data()), sizeof(buffer));

	for (std::bitset<width> item : data) {
		for (std::size_t i = 0; i < bytes; i++) {
			std::byte bitem = static_cast<std::byte>((item >> (i * 8)).to_ulong());
			stream.write(reinterpret_cast<const char*>(&bitem), sizeof(bitem));
		}
	}

	stream.flush();
	stream.close();

	return true;
}

std::size_t get_other_input_size(int party, char* problem_name, std::size_t problem_size) {
	if (strcmp(problem_name, "merge_sorted") == 0) {
		return problem_size * 4;
	} else if (strcmp(problem_name, "full_sort") == 0) {
		return problem_size * 4;
	} else if (strcmp(problem_name, "loop_join") == 0) {
		return problem_size * 4;
	} else if (strcmp(problem_name, "matrix_vector_multiply") == 0) {
		if (party == ALICE) {
			return problem_size;
		} else {
			return problem_size * problem_size;
		}
	} else if (strcmp(problem_name, "password") == 0) {
		return problem_size * 9;
	} else if (strcmp(problem_name, "aspirin") == 0) {
		return problem_size * 3;
	} else if (strcmp(problem_name, "comorbidity") == 0) {
		// DIAGNOSIS_MULTIPLIER and WORDS_PER_RECORD must match example_input.cpp and
		// the comorbidity function. Each record is pid_64 (2 words) + diag_10 (1 word).
		constexpr std::size_t DIAGNOSIS_MULTIPLIER = 1000;  // N = 1000 · M (paper ratio)
		constexpr std::size_t WORDS_PER_RECORD = 3;
		if (party == ALICE) {
			// Bob owns the cohort: problem_size records × 4 words.
			return problem_size * WORDS_PER_RECORD;
		} else {
			// Alice owns the diagnosis: N = 1000 · problem_size records × 4 words.
			return problem_size * DIAGNOSIS_MULTIPLIER * WORDS_PER_RECORD;
		}
	} else {
		std::cerr << "Unknown problem name " << problem_name << std::endl;
		std::abort();
	}
}

template <std::size_t width>
void encrypt_file(int party, std::size_t other_input_size, HighSpeedNetIO& io,
				  const std::vector<std::bitset<width>>& input_data, std::vector<Integer<width>>& output_data) {
	std::size_t iters = std::max(input_data.size(), other_input_size);
	output_data.resize(input_data.size() + other_input_size);
	for (std::size_t i = 0; i < iters; i++) {
		output_data[i] =
			Integer<width>((party == ALICE && i < input_data.size()) ? input_data[i] : std::bitset<width>(0), ALICE);
		if (i < std::min(input_data.size(), other_input_size))
			output_data[i + iters] = Integer<width>((party == BOB && i < input_data.size()) ? input_data[i] : std::bitset<width>(0), BOB);
		else
			Integer<width> dummy((party == BOB && i < input_data.size()) ? input_data[i] : std::bitset<width>(0), BOB);
	}
	io.flush();
}

template <std::size_t width>
void decrypt_file(int party, const std::vector<Integer<width>>& input_data,
				  std::vector<std::bitset<width>>& output_data) {
	constexpr std::size_t bs = 4096;
	for (std::size_t i = 0; i < input_data.size(); i += bs) {
		Integer<width * bs> batch;
		for (std::size_t j = 0; j < bs; j++) {
			if (i + j < input_data.size()) {
				std::memcpy(&(batch.bits.data()[j * width]), input_data[i + j].bits.data(), width * sizeof(Bit));
			}
		}

		std::bitset<width* bs> bbatch = batch.reveal();
		std::size_t output_size = std::min(bs, input_data.size() - i);
		for (std::size_t j = 0; j < width * output_size; j += width) {
			std::bitset<width> item(0);
			for (std::size_t k = 0; k < width; k++) {
				item.set(k, bbatch[j + k]);
			}
			output_data.push_back(item);
		}
	}
}

template <std::size_t width>
void merge_sorted(int party, std::size_t problem_size, const std::vector<Integer<width>>& input_data,
				  std::vector<Integer<width>>& output_data) {
	static_assert(width % 8 == 0, "Width must be multiple of 8");

	std::vector<Integer<width>> key;
	std::vector<Integer<width * 3>> value;

	for (std::size_t i = 0; i < input_data.size(); i += 4) {
		key.push_back(input_data[i]);

		Integer<width * 3> vitem;
		std::memcpy(&(vitem.bits.data()[0]), input_data[i + 1].bits.data(), width * sizeof(Bit));
		std::memcpy(&(vitem.bits.data()[width]), input_data[i + 2].bits.data(), width * sizeof(Bit));
		std::memcpy(&(vitem.bits.data()[2 * width]), input_data[i + 3].bits.data(), width * sizeof(Bit));
		value.push_back(vitem);
	}

	bitonic_merge(key.data(), value.data(), 0, key.size(), true);

	for (std::size_t i = 0; i != key.size(); i++) {
		output_data.push_back(key[i]);
		output_data.push_back(Integer<width>(static_cast<Bit*>(&(value[i].bits.data()[0]))));
		output_data.push_back(Integer<width>(static_cast<Bit*>(&(value[i].bits.data()[width]))));
		output_data.push_back(Integer<width>(static_cast<Bit*>(&(value[i].bits.data()[2 * width]))));
	}
}

template <std::size_t width>
void full_sort(int party, std::size_t problem_size, const std::vector<Integer<width>>& input_data,
			   std::vector<Integer<width>>& output_data) {
	static_assert(width % 8 == 0, "Width must be multiple of 8");

	constexpr std::size_t value_width_factor = 15;

	std::vector<Integer<width>> key;
	std::vector<Integer<width * value_width_factor>> value;

	for (std::size_t i = 0; i < input_data.size(); i += 4) {
		key.push_back(input_data[i]);

		Integer<width * value_width_factor> vitem;
		for (std::size_t j = 0; j < value_width_factor; j++) {
			std::memcpy(&(vitem.bits.data()[j * width]), input_data[i + 1 + j].bits.data(), width * sizeof(Bit));
		}
		value.push_back(vitem);
	}

	bitonic_sort(key.data(), value.data(), 0, key.size(), false);

	for (std::size_t i = 0; i != key.size(); i++) {
		output_data.push_back(key[i]);
		for (std::size_t j = 0; j < value_width_factor; j++) {
			output_data.push_back(Integer<width>(static_cast<Bit*>(&(value[i].bits.data()[j * width]))));
		}
	}
}

template <std::size_t width>
void loop_join(int party, std::size_t problem_size, const std::vector<Integer<width>>& input_data,
			   std::vector<Integer<width>>& output_data) {
	static_assert(width % 8 == 0, "Width must be multiple of 8");

	if (input_data.size() % 8 != 0) {
		std::cerr << "Input data size must be multiple of 8" << std::endl;
		return;
	}

	Integer<width> zero(0, PUBLIC);

	typename std::vector<Integer<width>>::const_iterator table1_input_data_begin = input_data.begin();
	typename std::vector<Integer<width>>::const_iterator table1_input_data_end =
		input_data.begin() + input_data.size() / 2;
	typename std::vector<Integer<width>>::const_iterator table2_input_data_begin =
		input_data.begin() + input_data.size() / 2;
	typename std::vector<Integer<width>>::const_iterator table2_input_data_end = input_data.end();

	const std::size_t table_size = input_data.size() / 8;
	output_data.resize(table_size * table_size * 9);

	std::size_t iter = 0;
	for (typename std::vector<Integer<width>>::const_iterator i = table1_input_data_begin; i != table1_input_data_end;
		 i += 4) {
		for (typename std::vector<Integer<width>>::const_iterator j = table2_input_data_begin;
			 j != table2_input_data_end; j += 4) {
			iter += 1;

			Bit valid = i->geq(*j);
			Integer<width> valid_int(0);
			valid_int[0] = !valid;
			output_data[iter] = valid_int;
			output_data[iter + 1] = i->select(valid, zero);
			output_data[iter + 2] = (i + 1)->select(valid, zero);
			output_data[iter + 3] = (i + 2)->select(valid, zero);
			output_data[iter + 4] = (i + 3)->select(valid, zero);
			output_data[iter + 5] = j->select(valid, zero);
			output_data[iter + 6] = (j + 1)->select(valid, zero);
			output_data[iter + 7] = (j + 2)->select(valid, zero);
			output_data[iter + 8] = (j + 3)->select(valid, zero);
		}
	}
}

template <std::size_t width>
void aspirin(int party, std::size_t problem_size, const std::vector<Integer<width>>& input_data,
			 std::vector<Integer<width>>& output_data) {
	static_assert(width % 8 == 0, "Width must be multiple of 8");

	// Record layout matches mage/src/programs/aspirin.cpp:
	//   word 0: timestamp   (low half of patient_id_concat_timestamp)
	//   word 1: patient_id  (high half)
	//   word 2: diagnosis bit (low bit, rest zero)
	constexpr std::size_t record_words = 3;

	std::vector<Integer<2 * width>> key;
	std::vector<Integer<width>> diag;
	key.reserve(input_data.size() / record_words);
	diag.reserve(input_data.size() / record_words);

	for (std::size_t i = 0; i < input_data.size(); i += record_words) {
		Integer<2 * width> k;
		std::memcpy(&(k.bits.data()[0]), input_data[i].bits.data(), width * sizeof(Bit));
		std::memcpy(&(k.bits.data()[width]), input_data[i + 1].bits.data(), width * sizeof(Bit));
		key.push_back(k);
		diag.push_back(input_data[i + 2]);
	}

	// Alice ascending + Bob descending by patient_id ⇒ concatenated 2N sequence is
	// bitonic by (patient_id, timestamp); bitonic_merge sorts it ascending. Mirrors
	// MAGE's "parallel_bitonic_sorter" step in aspirin.cpp:98 (full sort there;
	// merge here exploits the bitonic input, same as our password port).
	bitonic_merge(key.data(), diag.data(), 0, key.size(), true);

	// Count adjacent pairs where first.diag=1, second.diag=0, and patient_ids match
	// (mage/src/programs/aspirin.cpp:101-113). patient_id is the high half of the
	// 2*width-bit key.
	Integer<width> count(0, PUBLIC);
	Integer<width> one(1, PUBLIC);
	for (std::size_t i = 0; i + 1 < key.size(); i++) {
		Integer<width> pid_i(static_cast<Bit*>(&(key[i].bits.data()[width])));
		Integer<width> pid_ip1(static_cast<Bit*>(&(key[i + 1].bits.data()[width])));
		Bit pid_eq = (pid_i == pid_ip1);
		Bit hit = diag[i][0] & !diag[i + 1][0] & pid_eq;
		Integer<width> next = count + one;
		count = count.select(hit, next);
	}

	output_data.push_back(count);
}

template <std::size_t width>
void password(int party, std::size_t problem_size, const std::vector<Integer<width>>& input_data,
			  std::vector<Integer<width>>& output_data) {
	static_assert(width % 8 == 0, "Width must be multiple of 8");

	// Record layout matches mage/src/programs/password.cpp:
	//   words [0..pw_words): pw_hash (256 bits when width == 32, low part of the record)
	//   word  pw_words:       user_id (sort key, high part of the record)
	constexpr std::size_t pw_words = 8;
	constexpr std::size_t record_words = pw_words + 1;

	std::vector<Integer<width>> key;
	std::vector<Integer<width * pw_words>> value;
	key.reserve(input_data.size() / record_words);
	value.reserve(input_data.size() / record_words);

	for (std::size_t i = 0; i < input_data.size(); i += record_words) {
		key.push_back(input_data[i + pw_words]);

		Integer<width * pw_words> vitem;
		for (std::size_t j = 0; j < pw_words; j++) {
			std::memcpy(&(vitem.bits.data()[j * width]), input_data[i + j].bits.data(), width * sizeof(Bit));
		}
		value.push_back(vitem);
	}

	// Alice's records arrive sorted ascending by user_id and Bob's arrive sorted descending
	// (see emp-tool/example_input.cpp), so the concatenated 2N-record sequence is bitonic
	// and a single bitonic_merge yields an ascending sort by user_id — matching MAGE's
	// "Merge the two sorted arrays, sorted by user but not password" step.
	bitonic_merge(key.data(), value.data(), 0, key.size(), true);

	// For each adjacent pair, output user_id if the full (user_id, pw_hash) records match;
	// else 0. Produces 2 * problem_size - 1 outputs, matching MAGE's for_each_pair.
	Integer<width> zero(0, PUBLIC);
	for (std::size_t i = 0; i + 1 < key.size(); i++) {
		Bit match = (key[i] == key[i + 1]) & (value[i] == value[i + 1]);
		output_data.push_back(zero.select(match, key[i]));
	}
}

template <std::size_t width>
void matrix_vector_multiply(int party, std::size_t problem_size, const std::vector<Integer<width>>& input_data,
							std::vector<Integer<width>>& output_data) {
	static_assert(width % 8 == 0, "Width must be multiple of 8");

	output_data.resize(problem_size);
	for (std::size_t i = 0; i < problem_size; i++) {
		Integer<width> result(0);
		for (std::size_t j = 0; j < problem_size; j++) {
			result = result + (input_data[i * problem_size + j] * input_data[problem_size * problem_size + j]);
		}
		output_data[i] = result;
	}
}

// ---------------------------------------------------------------------------
// Custom 0/1-key bitonic sort for the comorbidity compaction step.
//
// For a 0/1 key, "swap on b" (the value at the higher-index position) realises
// a correct DESC max/min comparator without any AND for control derivation:
//
//   a | b | ctrl=b → swap? | out_i | out_j  ↔ max(a,b), min(a,b)
//   0 | 0 |       no       |   a=0 |   b=0   ✓
//   0 | 1 |       yes      |   b=1 |   a=0   ✓
//   1 | 0 |       no       |   a=1 |   b=0   ✓
//   1 | 1 |       yes      |   b=1 |   a=1   ✓
//
// Symmetrically, "swap on a" gives ASC. Per cmp_swap on (1-bit key + W-bit data):
// 1 + W ANDs total (no AND for the control, just XORs to apply it).
// ---------------------------------------------------------------------------
inline int comorbidity_gpwr2_less(int n) {
	int k = 1;
	while (k < n) k <<= 1;
	return k >> 1;
}

template <std::size_t diag_bits>
inline void comorbidity_cmp_swap_valid(Bit* valids, Integer<diag_bits>* diags, int i, int j, bool desc) {
	Bit ctrl = desc ? valids[j] : valids[i];
	// Conditional swap of the 1-bit key.
	Bit delta_v = valids[i] ^ valids[j];
	Bit and_v = ctrl & delta_v;  // 1 AND
	valids[i] = valids[i] ^ and_v;
	valids[j] = valids[j] ^ and_v;
	// Conditional swap of the diag data (one AND per bit, reusing ctrl).
	for (std::size_t b = 0; b < diag_bits; b++) {
		Bit delta_d = diags[i][b] ^ diags[j][b];
		Bit and_d = ctrl & delta_d;
		diags[i][b] = diags[i][b] ^ and_d;
		diags[j][b] = diags[j][b] ^ and_d;
	}
}

template <std::size_t diag_bits>
void comorbidity_bitonic_merge_valid(Bit* valids, Integer<diag_bits>* diags, int lo, int n, bool desc) {
	if (n > 1) {
		int m = comorbidity_gpwr2_less(n);
		for (int i = lo; i < lo + n - m; i++) {
			comorbidity_cmp_swap_valid(valids, diags, i, i + m, desc);
		}
		comorbidity_bitonic_merge_valid(valids, diags, lo, m, desc);
		comorbidity_bitonic_merge_valid(valids, diags, lo + m, n - m, desc);
	}
}

template <std::size_t diag_bits>
void comorbidity_bitonic_sort_valid(Bit* valids, Integer<diag_bits>* diags, int lo, int n, bool desc) {
	if (n > 1) {
		int m = n / 2;
		comorbidity_bitonic_sort_valid(valids, diags, lo, m, !desc);
		comorbidity_bitonic_sort_valid(valids, diags, lo + m, n - m, desc);
		comorbidity_bitonic_merge_valid(valids, diags, lo, n, desc);
	}
}

template <std::size_t width>
void comorbidity(int party, std::size_t problem_size, const std::vector<Integer<width>>& input_data,
				 std::vector<Integer<width>>& output_data) {
	static_assert(width % 8 == 0, "Width must be multiple of 8");

	// Path D' — mirrors orq's comorbidity:
	//   SELECT diag, COUNT(*) FROM diagnosis WHERE pid IN cohort
	//   GROUP BY diag ORDER BY cnt DESC LIMIT 10
	//
	// Pipeline:
	//   1. bitonic_merge by pid, carry diag (input is bitonic by construction).
	//   2. valid[i] = (pid[i] == pid[i+1]) — adjacency check; works because
	//      cmp_swap is stable, so in matched pid groups Alice's diag row (lower
	//      input index) precedes Bob's cohort row.
	//   3. Drop pid; sort by valid DESC carrying diag (big sort = compaction).
	//   4. Truncate to first M rows (no reveal; relies on R ≤ M test-data invariant).
	//   5. Sort by diag carrying valid (small sort on M rows).
	//   6. Group-by sweep: count[i] at first-of-group = Σ valid in group.
	//   7. Top-K selection over (valid, count) composite, carrying diag.
	//   8. Reveal (diag, count) only — valid bit stays hidden.
	//
	// Field widths:
	//   pid: 64 bits (paper's int64_t model)
	//   diag: 10 bits (K_DIAG ≤ 1024 → fits in 1 word; not padded to 64)
	//   count: 64 bits
	//   composite: 65 bits (valid in the top bit + 64-bit count)
	// Each record on disk is 3 × uint32 words: pid (low, high) then diag (1 word).
	constexpr std::size_t DIAGNOSIS_MULTIPLIER = 1000;     // N = 1000 · M (paper ratio)
	constexpr std::size_t K_TOP = 10;
	constexpr std::size_t pid_bits = 64;
	constexpr std::size_t diag_bits = 10;
	constexpr std::size_t count_bits = 64;
	constexpr std::size_t composite_bits = count_bits + 1; // valid in the top bit
	constexpr std::size_t pid_words = pid_bits / width;            // = 64/32 = 2
	constexpr std::size_t diag_words = 1;                          // 10 bits fits in 1 word
	constexpr std::size_t words_per_record = pid_words + diag_words; // 3

	static_assert(pid_bits % width == 0, "pid_bits must be a multiple of input word width");
	static_assert(diag_bits <= width, "diag_bits must fit in a single input word");

	const std::size_t M = problem_size;
	const std::size_t N = problem_size * DIAGNOSIS_MULTIPLIER;
	const std::size_t total_rows = N + M;

	// input_data layout (from encrypt_file):
	//   [0, words_per_record · N):           Alice's diagnosis rows (pid_64, diag_10)
	//   [words_per_record · N, ...):         Bob's cohort rows (pid_64, placeholder)

	// Helper: read a 64-bit pid from 2 consecutive input words.
	auto read_pid_64 = [&](std::size_t w) -> Integer<pid_bits> {
		Integer<pid_bits> result(0, PUBLIC);
		for (std::size_t k = 0; k < pid_words; k++) {
			std::memcpy(&(result.bits.data()[k * width]), input_data[w + k].bits.data(),
						width * sizeof(Bit));
		}
		return result;
	};

	// Helper: read a 10-bit diag from the low bits of a single input word.
	auto read_diag_10 = [&](std::size_t w) -> Integer<diag_bits> {
		Integer<diag_bits> result(0, PUBLIC);
		for (std::size_t b = 0; b < diag_bits; b++) {
			result[b] = input_data[w][b];
		}
		return result;
	};

	// --- Step 1: unpack inputs into parallel arrays (pid_64, diag_10). ---
	std::vector<Integer<pid_bits>> pids;
	std::vector<Integer<diag_bits>> diags;
	std::vector<Bit> valids(total_rows);
	std::vector<Integer<count_bits>> counts(M, Integer<count_bits>(0, PUBLIC));
	std::vector<Integer<composite_bits>> composite(M, Integer<composite_bits>(0, PUBLIC));
	pids.reserve(total_rows);
	diags.reserve(total_rows);

	for (std::size_t i = 0; i < N; i++) {
		std::size_t base = i * words_per_record;
		pids.push_back(read_pid_64(base));
		diags.push_back(read_diag_10(base + pid_words));
	}
	const std::size_t bob_start = N * words_per_record;
	for (std::size_t i = 0; i < M; i++) {
		std::size_t base = bob_start + i * words_per_record;
		pids.push_back(read_pid_64(base));
		diags.push_back(read_diag_10(base + pid_words));
	}

	// --- Step 2: bitonic_merge by pid, carrying diag (10 bits). ---
	// Input is bitonic (Alice ASC, Bob DESC); cmp_swap is stable, so within
	// any same-pid group Alice's row stays at the lower output index.
	bitonic_merge(pids.data(), diags.data(), 0, static_cast<int>(total_rows), Bit(true, PUBLIC));

	// --- Step 3: adjacency sweep → set valid. ---
	// In matched groups (size 2): Alice diag at i, Bob cohort at i+1; pid[i]==pid[i+1].
	// In singletons: pid[i]!=pid[i+1].
	for (std::size_t i = 0; i + 1 < total_rows; i++) {
		valids[i] = (pids[i] == pids[i + 1]);
	}
	valids[total_rows - 1] = Bit(false, PUBLIC);

	// pids are no longer needed.
	pids.clear();
	pids.shrink_to_fit();

	// --- Step 4: sort by valid DESC, carrying diag — custom b-as-control sort. ---
	// 0 ANDs to derive the swap control (just reuse valids[j] as a wire); only
	// the diag swap (diag_bits ANDs per cmp_swap) and the valid swap (1 AND)
	// cost anything. Net per cmp_swap: 1 + diag_bits = 11 ANDs.
	comorbidity_bitonic_sort_valid<diag_bits>(valids.data(), diags.data(), 0,
											  static_cast<int>(total_rows), /*desc=*/true);

	// --- Step 5: truncate to first M rows (no reveal of R). ---
	// Test-data invariant: R = M, so all first-M rows are valid.
	diags.resize(M);
	valids.resize(M);

	// --- Step 6: sort by diag ASC, carrying valid. ---
	bitonic_sort(diags.data(), valids.data(), 0, static_cast<int>(M), Bit(true, PUBLIC));

	// --- Step 7: group-by sweep → count = Σ valid per diag group. ---
	{
		Integer<diag_bits> last_diag(-1, PUBLIC);  // all-ones; doesn't match any real diag (≤ K_DIAG-1)
		Integer<count_bits> running_count(0, PUBLIC);
		Integer<count_bits> zero_count(0, PUBLIC);
		for (std::ptrdiff_t i = static_cast<std::ptrdiff_t>(M) - 1; i >= 0; i--) {
			Bit same_diag = (diags[i] == last_diag);
			Integer<count_bits> v(0, PUBLIC);
			v[0] = valids[i];
			// running = (same_diag ? running : 0) + v
			Integer<count_bits> base = zero_count.select(same_diag, running_count);
			running_count = base + v;
			last_diag = diags[i];
			counts[i] = running_count;
		}
	}

	// Mask: keep count only at first-of-group rows (i.e., where diag changes from i-1).
	{
		Integer<count_bits> zero_count(0, PUBLIC);
		for (std::size_t i = 1; i < M; i++) {
			Bit is_first = (diags[i] != diags[i - 1]);
			counts[i] = zero_count.select(is_first, counts[i]);
		}
		// counts[0] is always first-of-group (no predecessor).
	}

	// --- Step 8: build composite (valid << count_bits) | count for top-K. ---
	// Composite is treated as a single (count_bits + 1)-bit integer; valid is the
	// high bit so valid=1 rows always outrank valid=0 rows.
	for (std::size_t i = 0; i < M; i++) {
		std::memcpy(&(composite[i].bits.data()[0]), counts[i].bits.data(), count_bits * sizeof(Bit));
		composite[i][count_bits] = valids[i];
	}

	// --- Step 9: top-K_TOP selection by composite DESC, carrying diag. ---
	for (std::size_t r = 0; r < K_TOP && r < M; r++) {
		for (std::size_t j = M - 1; j > r; j--) {
			Bit swap_it = composite[j] > composite[j - 1];
			Integer<composite_bits> ca = composite[j - 1];
			Integer<composite_bits> cb = composite[j];
			composite[j - 1] = ca.select(swap_it, cb);
			composite[j] = cb.select(swap_it, ca);
			Integer<diag_bits> da = diags[j - 1];
			Integer<diag_bits> db = diags[j];
			diags[j - 1] = da.select(swap_it, db);
			diags[j] = db.select(swap_it, da);
		}
	}

	// --- Step 10: emit top-K (diag_10 as 1 word, count_64 as 2 words: low, high).
	// Valid bit (composite[r][count_bits]) is deliberately NOT included in output. ---
	for (std::size_t r = 0; r < K_TOP; r++) {
		// diag: 1 width-32 word; low diag_bits hold the value, upper bits are 0.
		Integer<width> diag_out(0, PUBLIC);
		for (std::size_t b = 0; b < diag_bits; b++) {
			diag_out[b] = diags[r][b];
		}
		output_data.push_back(diag_out);

		// count: 2 width-32 words (low then high) carved from composite's low 64 bits.
		for (std::size_t k = 0; k < pid_words; k++) {
			Integer<width> w(0, PUBLIC);
			std::memcpy(w.bits.data(), &(composite[r].bits.data()[k * width]), width * sizeof(Bit));
			output_data.push_back(w);
		}
	}
}

int main(int argc, char** argv) {
#ifdef PARTY
	constexpr int party = PARTY;
#else
#error "Party must be defined at compile time"
#endif

	if (argc != 7) {
		std::cout << "Usage: " << argv[0]
				  << " [problem_name] [problem_size] [port] [other_ip] [input_file] [output_file]" << std::endl;
		return 1;
	}

	char* problem_name = argv[1];
	std::size_t problem_size = std::stoull(argv[2]);
	int port = atoi(argv[3]);
	char* other_ip = argv[4];
	char* input_file = argv[5];
	char* output_file = argv[6];

	constexpr std::size_t width = 32;
	constexpr std::size_t bs = 4096;

	std::vector<std::bitset<width>> input_data;
	read_from_file<width, bs>(input_file, input_data);

	HighSpeedNetIO io(party == ALICE ? nullptr : other_ip, port, port + 1, true);
	setup_semi_honest<party, HighSpeedNetIO>(&io, party);

	double start_cpu_time = get_cpu_time_ms();

	std::chrono::high_resolution_clock::time_point encrypt_start = std::chrono::high_resolution_clock::now();
	std::vector<Integer<width>> input_data_encrypt;
	encrypt_file(party, get_other_input_size(party, problem_name, problem_size), io, input_data, input_data_encrypt);
	std::chrono::high_resolution_clock::time_point encrypt_end = std::chrono::high_resolution_clock::now();
	std::cout << "Encrypt time: "
			  << std::chrono::duration_cast<std::chrono::milliseconds>(encrypt_end - encrypt_start).count() << " ms"
			  << std::endl;

	std::vector<Integer<width>> output_data_encrypt;
	if (strcmp(problem_name, "merge_sorted") == 0) {
		merge_sorted<width>(party, problem_size, input_data_encrypt, output_data_encrypt);
	} else if (strcmp(problem_name, "full_sort") == 0) {
		full_sort<width>(party, problem_size, input_data_encrypt, output_data_encrypt);
	} else if (strcmp(problem_name, "loop_join") == 0) {
		loop_join<width>(party, problem_size, input_data_encrypt, output_data_encrypt);
	} else if (strcmp(problem_name, "matrix_vector_multiply") == 0) {
		matrix_vector_multiply<width>(party, problem_size, input_data_encrypt, output_data_encrypt);
	} else if (strcmp(problem_name, "password") == 0) {
		password<width>(party, problem_size, input_data_encrypt, output_data_encrypt);
	} else if (strcmp(problem_name, "aspirin") == 0) {
		aspirin<width>(party, problem_size, input_data_encrypt, output_data_encrypt);
	} else if (strcmp(problem_name, "comorbidity") == 0) {
		comorbidity<width>(party, problem_size, input_data_encrypt, output_data_encrypt);
	} else {
		std::cerr << "Unknown problem name" << std::endl;
		return 1;
	}

	std::chrono::high_resolution_clock::time_point calc_end = std::chrono::high_resolution_clock::now();
	std::cout << "Calc time: " << std::chrono::duration_cast<std::chrono::milliseconds>(calc_end - encrypt_end).count()
			  << " ms" << std::endl;

	std::vector<std::bitset<width>> output_data;
	decrypt_file(party, output_data_encrypt, output_data);
	std::chrono::high_resolution_clock::time_point decrypt_end = std::chrono::high_resolution_clock::now();
	std::cout << "Decrypt time: "
			  << std::chrono::duration_cast<std::chrono::milliseconds>(decrypt_end - calc_end).count() << " ms"
			  << std::endl;

	std::cout << "Total time: "
			  << std::chrono::duration_cast<std::chrono::milliseconds>(decrypt_end - encrypt_start).count() << " ms"
			  << std::endl;

	double end_cpu_time = get_cpu_time_ms();
	std::cout << "Total cpu time: " << end_cpu_time - start_cpu_time << " ms" << std::endl;

	write_to_file<width, bs>(output_file, output_data);
}
