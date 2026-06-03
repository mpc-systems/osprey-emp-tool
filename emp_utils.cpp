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
		// K_DIAG must match example_input.cpp and the comorbidity function.
		constexpr std::size_t K_DIAG = 10;
		constexpr std::size_t DIAGNOSIS_MULTIPLIER = K_DIAG * (K_DIAG + 1) / 2;  // 55
		if (party == ALICE) {
			// Bob owns the cohort: problem_size records × 2 words (pid, placeholder).
			return problem_size * 2;
		} else {
			// Alice owns the diagnosis: problem_size · 55 records × 2 words (pid, diag).
			return problem_size * DIAGNOSIS_MULTIPLIER * 2;
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

template <std::size_t width>
void comorbidity(int party, std::size_t problem_size, const std::vector<Integer<width>>& input_data,
				 std::vector<Integer<width>>& output_data) {
	static_assert(width % 8 == 0, "Width must be multiple of 8");

	// Mirrors orq's comorbidity benchmark:
	//   SELECT diag, COUNT(*) FROM diagnosis WHERE pid IN cohort
	//   GROUP BY diag ORDER BY cnt DESC LIMIT 10
	//
	// K_DIAG must match example_input.cpp and get_other_input_size above.
	constexpr std::size_t K_DIAG = 10;
	constexpr std::size_t DIAGNOSIS_MULTIPLIER = K_DIAG * (K_DIAG + 1) / 2;  // 55
	constexpr std::size_t K_TOP = 10;

	const std::size_t M = problem_size;                          // cohort size
	const std::size_t N = problem_size * DIAGNOSIS_MULTIPLIER;   // diagnosis size
	const std::size_t total_rows = N + M;

	// input_data layout (produced by encrypt_file):
	//   [0,         2*N):     Alice's diagnosis rows (pid, diag) × 2 words each
	//   [2*N, 2*N + 2*M):     Bob's cohort rows (pid, 0_placeholder) × 2 words each

	// Step 1: Unpack into parallel arrays (pid, packed=(diag,tid)).
	// packed = diag in bits [0, width), tid in bit width. Width+1 bits total.
	std::vector<Integer<width>> pids;
	std::vector<Integer<width + 1>> packed;
	pids.reserve(total_rows);
	packed.reserve(total_rows);

	for (std::size_t i = 0; i < N; i++) {
		pids.push_back(input_data[i * 2]);
		Integer<width + 1> p(0, PUBLIC);
		for (std::size_t b = 0; b < width; b++) {
			p[b] = input_data[i * 2 + 1][b];
		}
		p[width] = Bit(false, PUBLIC);  // tid = 0 for diagnosis
		packed.push_back(p);
	}
	for (std::size_t i = 0; i < M; i++) {
		pids.push_back(input_data[2 * N + i * 2]);
		Integer<width + 1> p(0, PUBLIC);
		// diag bits already 0 from the constructor; nothing to set.
		p[width] = Bit(true, PUBLIC);  // tid = 1 for cohort
		packed.push_back(p);
	}

	// Step 2: bitonic_merge by pid ASC, carrying packed=(diag,tid).
	// Concatenated input is bitonic by pid (Alice ASC, Bob DESC — see example_input.cpp).
	bitonic_merge(pids.data(), packed.data(), 0, static_cast<int>(total_rows), Bit(true, PUBLIC));

	// Step 3: Semi-join sweep. For each row, set valid[i] iff this row is
	// a diagnosis row (tid=0) AND its pid group contains a cohort row (tid=1).
	// "pid group contains cohort" requires propagating tid bits across all rows
	// of the same pid; we do this with a forward + backward scan over the sorted array.
	std::vector<Bit> tids(total_rows);
	for (std::size_t i = 0; i < total_rows; i++) {
		tids[i] = packed[i][width];
	}

	std::vector<Bit> forward_has(total_rows);
	{
		Integer<width> fwd_pid(-1, PUBLIC);  // sentinel: all-ones; won't match any real pid
		Bit fwd_has(false, PUBLIC);
		Bit zero_bit(false, PUBLIC);
		for (std::size_t i = 0; i < total_rows; i++) {
			Bit new_group = (pids[i] != fwd_pid);
			fwd_has = fwd_has.select(new_group, zero_bit);  // reset on new group
			fwd_has = fwd_has | tids[i];
			fwd_pid = pids[i];
			forward_has[i] = fwd_has;
		}
	}

	std::vector<Bit> backward_has(total_rows);
	{
		Integer<width> bwd_pid(-1, PUBLIC);
		Bit bwd_has(false, PUBLIC);
		Bit zero_bit(false, PUBLIC);
		for (std::ptrdiff_t i = static_cast<std::ptrdiff_t>(total_rows) - 1; i >= 0; i--) {
			Bit new_group = (pids[i] != bwd_pid);
			bwd_has = bwd_has.select(new_group, zero_bit);
			bwd_has = bwd_has | tids[i];
			bwd_pid = pids[i];
			backward_has[i] = bwd_has;
		}
	}

	std::vector<Bit> valids(total_rows);
	std::vector<Integer<width>> diags(total_rows);
	for (std::size_t i = 0; i < total_rows; i++) {
		valids[i] = (!tids[i]) & (forward_has[i] | backward_has[i]);
		// Project out pid and tid; only diag (and valid) are needed downstream.
		for (std::size_t b = 0; b < width; b++) {
			diags[i][b] = packed[i][b];
		}
	}

	// Step 4: bitonic_sort by diag ASC, carrying valid. Input is NOT bitonic
	// (it was sorted by pid, not diag), so we need a full sort.
	bitonic_sort(diags.data(), valids.data(), 0, static_cast<int>(total_rows), Bit(true, PUBLIC));

	// Step 5: Group-by sweep. After sort, rows with the same diag are contiguous.
	// Backward pass: running_count[i] = sum of valid bits from i to end-of-group.
	// At the first row of each group, running_count = total group count.
	std::vector<Integer<width>> counts(total_rows);
	{
		Integer<width> last_diag(-1, PUBLIC);
		Integer<width> running_count(0, PUBLIC);
		for (std::ptrdiff_t i = static_cast<std::ptrdiff_t>(total_rows) - 1; i >= 0; i--) {
			Bit same_diag = (diags[i] == last_diag);
			Integer<width> valid_int(0, PUBLIC);
			valid_int[0] = valids[i];
			Integer<width> accumulated = running_count + valid_int;
			// running_count = same_diag ? accumulated : valid_int (reset on new group)
			running_count = valid_int.select(same_diag, accumulated);
			last_diag = diags[i];
			counts[i] = running_count;
		}
	}

	// Mask counts: keep them only at the first row of each group; zero elsewhere.
	// This ensures top-K selection picks one representative per group.
	{
		Integer<width> zero_int(0, PUBLIC);
		for (std::size_t i = 1; i < total_rows; i++) {
			Bit is_first = (diags[i] != diags[i - 1]);
			counts[i] = zero_int.select(is_first, counts[i]);
		}
		// counts[0] is always at a group boundary (no predecessor) — keep as is.
	}

	// Step 6: Top-K selection via bubble-style network. K = K_TOP = 10.
	// After K passes, positions [0, K) hold the K largest counts (DESC),
	// with their associated diag values. No full sort needed.
	for (std::size_t r = 0; r < K_TOP && r < total_rows; r++) {
		for (std::size_t j = total_rows - 1; j > r; j--) {
			Bit swap_it = counts[j] > counts[j - 1];
			Integer<width> ca = counts[j - 1];
			Integer<width> cb = counts[j];
			counts[j - 1] = ca.select(swap_it, cb);
			counts[j] = cb.select(swap_it, ca);
			Integer<width> da = diags[j - 1];
			Integer<width> db = diags[j];
			diags[j - 1] = da.select(swap_it, db);
			diags[j] = db.select(swap_it, da);
		}
	}

	// Output top-K_TOP (diag, count) pairs.
	for (std::size_t r = 0; r < K_TOP; r++) {
		output_data.push_back(diags[r]);
		output_data.push_back(counts[r]);
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
