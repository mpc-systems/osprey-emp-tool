#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <utility>
#include <vector>

template <typename T, std::size_t width>
void write(std::ofstream& stream, const T value) {
	static_assert(sizeof(T) >= width, "Size of T must be greater than or equal to width");
	stream.write(reinterpret_cast<const char*>(&value), width);
}

void write_record(std::ofstream& stream, const std::uint64_t value, std::size_t value_width = 4) {
	write<std::uint64_t, 4>(stream, value);

	if (value_width < 1) {
		return;
	}

	for (std::size_t i = 0; i < value_width; i++) {
		write<std::uint64_t, 4>(stream, 0);
	}
}

int main(int argc, char** argv) {
	if (argc != 3 && argc != 4) {
		std::cout << "Usage: " << argv[0] << " problem_name problem_size [option]" << std::endl;
		return 0;
	}
	std::string problem_name(argv[1]);
	std::size_t input_size = std::stoull(std::string(argv[2]));

	std::string option;
	if (argc == 4) {
		option = argv[3];
	}

	std::ofstream garbler_file(problem_name + "_" + std::to_string(input_size) + "_garbler.input");
	std::ofstream evaluator_file(problem_name + "_" + std::to_string(input_size) + "_evaluator.input");
	std::ofstream expected_file(problem_name + "_" + std::to_string(input_size) + ".expected");

	if (problem_name == "merge_sorted") {
		for (std::uint64_t i = 0; i != input_size * 2; i++) {
			if (i < input_size) {
				write_record(garbler_file, 2 * i);
			} else {
				write_record(evaluator_file, 2 * (2 * input_size - i - 1) + 1);
			}
			write_record(expected_file, i);
		}
	} else if (problem_name == "full_sort") {
		if (option == "") {
			for (std::uint64_t i = 0; i != input_size * 2; i++) {
				if (i < input_size) {
					write_record(garbler_file, 2 * i, 15);
				} else {
					write_record(evaluator_file, 2 * (2 * input_size - i - 1) + 1, 15);
				}
				write_record(expected_file, i, 15);
			}
		} else if (option == "random") {
			std::vector<std::uint32_t> sorted(2 * input_size);
			for (std::uint64_t i = 0; i != sorted.size(); i++) {
				sorted[i] = static_cast<std::uint32_t>(i);
			}
			std::vector<std::uint32_t> array(sorted);
			std::random_shuffle(array.begin(), array.end());
			for (std::uint64_t i = 0; i != input_size * 2; i++) {
				if (i < input_size) {
					write_record(garbler_file, array[i], 15);
				} else {
					write_record(evaluator_file, array[i], 15);
				}
				write_record(expected_file, sorted[i], 15);
			}
		} else {
			std::cerr << "Unknown option " << option << std::endl;
		}
	} else if (problem_name == "loop_join") {
		std::vector<std::uint32_t> table1_keys(input_size);
		std::iota(table1_keys.begin(), table1_keys.end(), 0);
		std::vector<std::uint32_t> table2_keys(input_size);
		std::iota(table2_keys.begin(), table2_keys.end(), 0);

		for (std::uint64_t i = 0; i != table1_keys.size(); i++) {
			write_record(garbler_file, table1_keys[i]);
		}

		for (std::uint64_t i = 0; i != table2_keys.size(); i++) {
			write_record(evaluator_file, table2_keys[i]);
		}

		for (std::uint64_t i = 0, k = 0; i != table1_keys.size(); i++) {
			for (std::uint64_t j = 0; j != table2_keys.size(); j++, k++) {
				bool valid = (table1_keys[i] < table2_keys[j]);
				if (valid) {
					write<std::uint32_t, 4>(expected_file, 1);
					write_record(expected_file, table1_keys[i]);
					write_record(expected_file, table2_keys[j]);
				} else {
					write<std::uint32_t, 4>(expected_file, 0);
					write_record(expected_file, 0);
					write_record(expected_file, 0);
				}
			}
		}
	} else if (problem_name == "matrix_multiply") {
		if (option == "") {
			for (std::uint64_t i = 0; i != input_size; i++) {
				for (std::uint64_t j = 0; j != input_size; j++) {
					std::uint8_t elem = (i == j) ? 1 : 0;
					write<std::uint8_t, 1>(garbler_file, elem);
					write<std::uint8_t, 1>(evaluator_file, elem);
					write<std::uint16_t, 2>(expected_file, elem);
				}
			}
		} else if (option == "random") {
			std::default_random_engine generator;
			std::uniform_int_distribution<std::uint8_t> distribution(0, UINT8_MAX);
			std::vector<std::uint8_t> a(input_size * input_size);
			for (std::size_t i = 0; i != a.size(); i++) {
				a[i] = distribution(generator);
				write<std::uint8_t, 1>(garbler_file, a[i]);
			}
			std::vector<std::uint8_t> b(input_size * input_size);
			for (std::size_t i = 0; i != b.size(); i++) {
				b[i] = distribution(generator);
				write<std::uint8_t, 1>(evaluator_file, b[i]);
			}
			for (std::size_t i = 0; i != input_size; i++) {
				for (std::size_t j = 0; j != input_size; j++) {
					std::uint16_t elem = 0;
					for (std::size_t k = 0; k != input_size; k++) {
						elem += static_cast<std::uint16_t>(a[i * input_size + k]) *
								static_cast<std::uint16_t>(b[j * input_size + k]);
					}
					write<std::uint16_t, 2>(expected_file, elem);
				}
			}
		} else {
			std::cerr << "Unknown option " << option << std::endl;
		}
	} else if (problem_name == "matrix_vector_multiply") {
		if (option == "") {
			for (std::uint64_t i = 0; i != input_size; i++) {
				std::uint8_t elem = static_cast<std::uint8_t>(i);
				write<std::uint32_t, 4>(evaluator_file, elem);
				write<std::uint32_t, 4>(expected_file, elem);
			}
			for (std::uint64_t i = 0; i != input_size; i++) {
				for (std::uint64_t j = 0; j != input_size; j++) {
					std::uint8_t elem = (i == j) ? 1 : 0;
					write<std::uint32_t, 4>(garbler_file, elem);
				}
			}
		} else if (option == "random") {
			std::default_random_engine generator;
			std::uniform_int_distribution<std::uint8_t> distribution(0, UINT8_MAX);
			std::vector<std::uint8_t> vector(input_size);
			for (std::size_t i = 0; i != input_size; i++) {
				vector[i] = distribution(generator);
				write<std::uint32_t, 4>(evaluator_file, vector[i]);
			}
			for (std::size_t i = 0; i != input_size; i++) {
				std::uint16_t expected_elem = 0;
				for (std::size_t j = 0; j != input_size; j++) {
					std::uint8_t matrix_elem = distribution(generator);
					write<std::uint32_t, 4>(garbler_file, matrix_elem);
					expected_elem += static_cast<std::uint16_t>(matrix_elem) * static_cast<std::uint16_t>(vector[j]);
				}
				write<std::uint32_t, 4>(expected_file, expected_elem);
			}
		} else {
			std::cerr << "Unknown option " << option << std::endl;
		}
	} else if (problem_name == "password") {
		// Layout matches mage/src/programs/password.cpp: each record is
		// pw_hash (256 bits, low) followed by user_id (32 bits, high).
		// Garbler has users 1..N; user 1 has hash 0, the rest have hash 1.
		// Evaluator has users N..1 (reverse); all have hash 0.
		// The only (user, pw_hash) pair that matches across parties is (1, 0).
		bool random = (option == "random");
		std::default_random_engine generator;
		std::uniform_int_distribution<std::uint32_t> distribution(0, 1);
		for (std::uint64_t i = 0; i != input_size * 2; i++) {
			if (i < input_size) {
				std::uint32_t user = static_cast<std::uint32_t>(i + 1);
				std::uint32_t hash = random ? distribution(generator) : ((user == 1) ? 0u : 1u);
				write<std::uint32_t, 4>(garbler_file, hash);
				for (std::size_t k = 0; k < 7; k++) {
					write<std::uint32_t, 4>(garbler_file, 0);
				}
				write<std::uint32_t, 4>(garbler_file, user);

				// After sorting by user_id, adjacent pairs are:
				//   pos 2*i:   garbler user (i+1) vs evaluator user (i+1) — match iff hash == 0
				//   pos 2*i+1: evaluator user (i+1) vs garbler user (i+2) — different users, always 0
				write<std::uint32_t, 4>(expected_file, (hash == 0) ? user : 0u);
				if (i + 1 != input_size) {
					write<std::uint32_t, 4>(expected_file, 0);
				}
			} else {
				std::uint32_t user = static_cast<std::uint32_t>(2 * input_size - i);
				for (std::size_t k = 0; k < 8; k++) {
					write<std::uint32_t, 4>(evaluator_file, 0);
				}
				write<std::uint32_t, 4>(evaluator_file, user);
			}
		}
	} else if (problem_name == "aspirin") {
		// Layout matches mage/src/programs/aspirin.cpp: each record is
		// word 0 = timestamp   (low 32 bits of patient_id_concat_timestamp)
		// word 1 = patient_id  (high 32 bits of patient_id_concat_timestamp)
		// word 2 = diagnosis   (low bit; rest zero)
		// Garbler: patients 0..N-1 at ts=1, diag=1 for all except patient 0.
		// Evaluator: patients N-1..0 at ts=2, all diag=0.
		// After bitonic_merge (ascending by patient_id_concat_timestamp), each
		// patient's two events sit adjacent in time order (garbler then evaluator),
		// so the count of pairs with first.diag=1 & !second.diag & same patient
		// is exactly input_size - 1.
		for (std::uint64_t i = 0; i != input_size * 2; i++) {
			if (i < input_size) {
				std::uint32_t pid = static_cast<std::uint32_t>(i);
				std::uint32_t diag = (i == 0) ? 0u : 1u;
				write<std::uint32_t, 4>(garbler_file, 1);
				write<std::uint32_t, 4>(garbler_file, pid);
				write<std::uint32_t, 4>(garbler_file, diag);
			} else {
				std::uint32_t pid = static_cast<std::uint32_t>(2 * input_size - i - 1);
				write<std::uint32_t, 4>(evaluator_file, 2);
				write<std::uint32_t, 4>(evaluator_file, pid);
				write<std::uint32_t, 4>(evaluator_file, 0);
			}
		}
		// MAGE also outputs an order-validity bit before the count; we skip it
		// here because the input format guarantees the bitonic precondition.
		write<std::uint32_t, 4>(expected_file, static_cast<std::uint32_t>(input_size - 1));
	} else {
		std::cerr << "Unknown problem " << problem_name << std::endl;
	}

	return 0;
}
