namespace senate_pwd {

	std::size_t get_other_input_size(int party, std::size_t problem_size) {
		return problem_size * 9;
	}

	void instrument_print() {
		printf("Senate PWD Join and Aggregate Operator\n");
	}

	template <std::size_t width>
	void join_and_aggregate(
		int party, std::size_t problem_size, const std::vector<Integer<width>>& input_data, std::vector<Integer<width>>& output_data
	) {
		std::vector<Integer<width>> key;
		std::vector<Integer<width * 8>> value;

		for (std::size_t i = 0; i < input_data.size(); i += 9) {
			key.push_back(input_data[i]);

			Integer<width * 8> vitem;
			for (std::size_t j = 0; j < 8; j++) {
				std::memcpy(&(vitem.bits.data()[j * width]), input_data[i + 1 + j].bits.data(), width * sizeof(Bit));
			}
			value.push_back(vitem);
		}

		bitonic_merge(key.data(), value.data(), 0, key.size(), true);

		output_data.resize(key.size());

		// Everything is sorted now, do a PSU.
		instrument_print();
		for (int i = 0; i < key.size() - 1; ++i) {
			Bit equals = key[i].equal(key[i+1]);
			output_data[i] = key[i].select(equals, Integer<width>(0, PUBLIC));
			printf("Finished key %d\n", i);
		}
		output_data[key.size() - 1] = key[key.size() - 1];
		printf("Finished last key\n");
	}

}
