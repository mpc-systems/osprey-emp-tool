namespace senate_pwd {

	std::size_t get_other_input_size(int party, std::size_t problem_size) {
		return problem_size;
	}

	template <std::size_t width>
	void join_and_aggregate(
		int party, std::size_t problem_size, const std::vector<Integer<width>>& input_data, std::vector<Integer<width>>& output_data
	) {
		// Verify the inputs.
		{
			Bit verifyOrder(true);
			for (int i = 0; i < input_data.size() / 2 - 1; ++i) {
				Bit lessThanNext = input_data[i].geq(input_data[i+1]);
				verifyOrder = verifyOrder & !lessThanNext;
			}
		}
		{
			Bit verifyOrder(true);
			for (int i = input_data.size() / 2;
					i < input_data.size() - 1; ++i) {
				Bit greaterThanNext = input_data[i].geq(input_data[i+1]);
				verifyOrder = verifyOrder & greaterThanNext;
			}
		}

		output_data.clear();
		output_data.insert(output_data.end(), input_data.begin(), input_data.end());
		bitonic_merge(output_data.data(), (Bit *) nullptr, 0, output_data.size(), false);

		// Everything is sorted now, do a PSU.
		for (int i = 0; i < output_data.size() - 1; ++i) {
			Bit equals = output_data[i].equal(output_data[i+1]);
			output_data[i] = output_data[i].select(equals, Integer<width>(0, PUBLIC));
		}
	}

}
