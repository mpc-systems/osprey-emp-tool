namespace senate_tpc_h_q4 {

	template <std::size_t width>
	struct KeyVal {
		Integer<width> key;
		Integer<width> count;
	};

	template <std::size_t width>
	struct row_12 {
		Integer<width> orderKey;
		Integer<width> orderPriority;
		Integer<width> count;
	};

	template <std::size_t width>
	struct row_l {
		Integer<width> orderKey;
		Integer<width> commitDate;
		Integer<width> receiptDate;
	};

	template <std::size_t width>
	struct row_o {
		Integer<width> orderKey;
		Integer<width> orderDate;
		Integer<width> orderPriority;
	};

	template <std::size_t width>
	struct row_123_o {
		Integer<width> orderPriority;
		Integer<width> count;
	};

	template <std::size_t width>
	void cmp_swap(std::vector<KeyVal<width>>& group, int i, int j) {
		Bit to_swap = (group[i].key > group[j].key);
		swap(to_swap, group[i].key, group[j].key);
		swap(to_swap, group[i].count, group[j].count);
	}

	template <std::size_t width>
	void cmp_swap(std::vector<row_12<width>>& group, int i, int j) {
		Bit to_swap = (group[i].orderPriority > group[j].orderPriority);
		swap(to_swap, group[i].orderKey, group[j].orderKey);
		swap(to_swap, group[i].orderPriority, group[j].orderPriority);
		swap(to_swap, group[i].count, group[j].count);
	}

	template <std::size_t width>
	void groupBy(std::vector<KeyVal<width>>& group, int lo, int n) {
		if (n > 1) {
			int m = greatestPowerOfTwoLessThan(n);
			for (int i = lo; i < lo + n - m; i++)
				cmp_swap(group, i, i + m);
			groupBy(group, lo, n - m);
			groupBy(group, lo + n - m, m);
		}
	}

	template <std::size_t width>
	void groupBy(std::vector<row_12<width>>& group, int lo, int n) {
		if (n > 1) {
			int m = greatestPowerOfTwoLessThan(n);
			for (int i = lo; i < lo + n - m; i++)
				cmp_swap(group, i, i + m);
			groupBy(group, lo, n - m);
			groupBy(group, lo + n - m, m);
		}
	}

	template <std::size_t width>
	void cmp_swap_counts(std::vector<KeyVal<width>>& group, int i, int j, Bit acc) {
		Bit to_swap = ((group[i].count > group[j].count) == acc);
		swap(to_swap, group[i].key, group[j].key);
		swap(to_swap, group[i].count, group[j].count);
	}

	template <std::size_t width>
	void cmp_swap_key(std::vector<row_12<width>>& group, int i, int j, Bit acc) {
		Bit to_swap = ((group[i].orderKey > group[j].orderKey) == acc);
		swap(to_swap, group[i].orderKey, group[j].orderKey);
		swap(to_swap, group[i].orderPriority, group[j].orderPriority);
		swap(to_swap, group[i].count, group[j].count);
	}

	template <std::size_t width>
	void cmp_swap_key(std::vector<row_o<width>>& group, int i, int j, Bit acc) {
		Bit to_swap = ((group[i].orderKey > group[j].orderKey) == acc);
		swap(to_swap, group[i].orderKey, group[j].orderKey);
		swap(to_swap, group[i].orderDate, group[j].orderDate);
		swap(to_swap, group[i].orderPriority, group[j].orderPriority);
	}

	template <std::size_t width>
	void cmp_swap_key(std::vector<row_l<width>>& group, int i, int j, Bit acc) {
		Bit to_swap = ((group[i].orderKey > group[j].orderKey) == acc);
		swap(to_swap, group[i].orderKey, group[j].orderKey);
		swap(to_swap, group[i].commitDate, group[j].commitDate);
		swap(to_swap, group[i].receiptDate, group[j].receiptDate);
	}

	template <std::size_t width>
	void cmp_swap_key(std::vector<row_123_o<width>>& group, int i, int j, Bit acc) {
		Bit to_swap = ((group[i].orderPriority > group[j].orderPriority) == acc);
		swap(to_swap, group[i].orderPriority, group[j].orderPriority);
		swap(to_swap, group[i].count, group[j].count);
	}

	template <std::size_t width>
	void merge(std::vector<row_12<width>>& group, int lo, int n, bool acc) {
		if (n > 1) {
			int m = greatestPowerOfTwoLessThan(n);
			for (int i = lo; i < lo + n - m; i++) {
				cmp_swap_key(group, i, i + m, acc);
			}
			if (acc) {
				merge(group, lo, n - m, acc);
				merge(group, lo + n - m, m, acc);
			} else {
				merge(group, lo, m, acc);
				merge(group, lo + m, n - m, acc);
			}
		}
	}

	template <std::size_t width>
	void merge(std::vector<row_o<width>>& group, int lo, int n, bool acc) {
		if (n > 1) {
			int m = greatestPowerOfTwoLessThan(n);
			for (int i = lo; i < lo + n - m; i++) {
				cmp_swap_key(group, i, i + m, acc);
			}
			if (acc) {
				merge(group, lo, n - m, acc);
				merge(group, lo + n - m, m, acc);
			} else {
				merge(group, lo, m, acc);
				merge(group, lo + m, n - m, acc);
			}
		}
	}

	template <std::size_t width>
	void merge(std::vector<row_l<width>>& group, int lo, int n, bool acc) {
		if (n > 1) {
			int m = greatestPowerOfTwoLessThan(n);
			for (int i = lo; i < lo + n - m; i++) {
				cmp_swap_key(group, i, i + m, acc);
			}
			if (acc) {
				merge(group, lo, n - m, acc);
				merge(group, lo + n - m, m, acc);
			} else {
				merge(group, lo, m, acc);
				merge(group, lo + m, n - m, acc);
			}
		}
	}

	template <std::size_t width>
	void merge(std::vector<row_123_o<width>>& group, int lo, int n, bool acc) {
		if (n > 1) {
			int m = greatestPowerOfTwoLessThan(n);
			for (int i = lo; i < lo + n - m; i++) {
				cmp_swap_key(group, i, i + m, acc);
			}
			if (acc) {
				merge(group, lo, n - m, acc);
				merge(group, lo + n - m, m, acc);
			} else {
				merge(group, lo, m, acc);
				merge(group, lo + m, n - m, acc);
			}
		}
	}

	template <std::size_t width>
	void orderBy(std::vector<row_123_o<width>>& group, int lo, int n, bool acc = true) {
		if (n > 1) {
			int m = n / 2;
			orderBy(group, lo, m, true);
			orderBy(group, lo + m, n - m, false);
			merge(group, lo, n, acc);
		}
	}

	template <std::size_t width>
	void orderBy(std::vector<row_12<width>>& group, int lo, int n, bool acc = true) {
		if (n > 1) {
			int m = n / 2;
			orderBy(group, lo, m, true);
			orderBy(group, lo + m, n - m, false);
			merge(group, lo, n, acc);
		}
	}

	template <std::size_t width>
	void orderBy(std::vector<row_l<width>>& group, int lo, int n, bool acc = true) {
		if (n > 1) {
			int m = n / 2;
			orderBy(group, lo, m, true);
			orderBy(group, lo + m, n - m, false);
			merge(group, lo, n, acc);
		}
	}

	template <std::size_t width>
	void orderBy(std::vector<row_o<width>>& group, int lo, int n, bool acc = true) {
		if (n > 1) {
			int m = n / 2;
			orderBy(group, lo, m, true);
			orderBy(group, lo + m, n - m, false);
			merge(group, lo, n, acc);
		}
	}

	template <std::size_t width>
	inline Integer<width> select(Integer<width> A, Integer<width> B) {
		Bit eq = A.equal(Integer<width>(0, PUBLIC));
		Integer<width> result = A.select(eq, B);
		return result;
	}

	template <std::size_t width>
	inline Integer<width> dup_select_3(Integer<width> A, Integer<width> B, Integer<width> C) {
		Bit eq1 = A.equal(B);
		Bit eq2 = B.equal(C);
		Bit eq = eq1 | eq2;
		Integer<width> result = B;
		result[0] = result[0] & eq;
		return result;
	}

	template <std::size_t width>
	inline Integer<width> dup_select_2(Integer<width> A, Integer<width> B) {
		Bit eq = A.equal(B);
		Integer<width> result = A;
		result[0] = result[0] & eq;
		return result;
	}

	template <std::size_t width>
	inline row_12<width> dup_select_2(row_12<width> A, row_12<width> B) {
		Bit eq = A.orderKey.equal(B.orderKey);
		Integer<width> resultKey = A.orderKey;
		Integer<width> resultPriority = A.orderPriority | B.orderPriority;
		resultKey[0] = resultKey[0] & eq;
		resultPriority[0] = (A.orderPriority[0] & eq) | (B.orderPriority[0] & eq);
		row_12<width> res;
		res.orderKey = resultKey;
		res.orderPriority = resultPriority;
		res.count = A.count;
		return res;
	}

	template <std::size_t width>
	inline row_12<width> dup_select_3(row_12<width> A, row_12<width> B, row_12<width> C) {
		Bit eq1 = A.orderKey.equal(B.orderKey);
		Bit eq2 = B.orderKey.equal(C.orderKey);
		Bit eq = eq1 | eq2;
		Integer<width> resultKey = B.orderKey;
		Integer<width> potentialPriority = C.orderPriority.select(eq1, A.orderPriority);
		Integer<width> resultPriority = B.orderPriority | potentialPriority;
		resultKey[0] = resultKey[0] & eq;
		resultPriority[0] = resultPriority[0] & eq;

		row_12<width> res;
		res.orderKey = resultKey;
		res.orderPriority = resultPriority;
		res.count = A.count;
		return res;
	}

	std::size_t get_other_input_size(int party, std::size_t problem_size) {
		if (party == 1) {
			return problem_size;
		} else {
			return 2 * problem_size;
		}
	}

	// join o_order_key on l_order_key
	template <std::size_t width>
	void join_and_aggregate(
		int party, std::size_t problem_size, const std::vector<Integer<width>>& input_data, std::vector<Integer<width>>& output_data
	) {
		// Party 1 has (orderKey, orderPriority)
		// Party 2 has l_orderKey
		int s_problem_size = problem_size;
		int input_size = 2 * problem_size;
		std::vector<Integer<width>> input1(std::begin(input_data), std::begin(input_data) + input_size);
		std::vector<Integer<width>> input2(std::begin(input_data) + input_size, std::end(input_data));

		std::vector<row_12<width>> groups_o(s_problem_size);
		for (int i = 0; i < 2 * s_problem_size; i += 2) {
			groups_o[i / 2].orderKey = input1[i];
			groups_o[i / 2].orderPriority = input1[i + 1];
			groups_o[i / 2].count = Integer<width>(1, PUBLIC);
		}

		// orderBy(groups_o, 0, s_problem_size, true);

		std::vector<row_12<width>> groups_l(s_problem_size);
		for (int i = 0; i < s_problem_size; i++) {
			groups_l[i].orderKey = input2[i];
			groups_l[i].orderPriority = Integer<width>(0, PUBLIC);
			groups_l[i].count = Integer<width>(1, PUBLIC);
		}

		// orderBy(groups_l, 0, s_problem_size, false);

		std::vector<row_12<width>> groups_join(2 * s_problem_size);
		for (int i = 0; i < s_problem_size; i++) {
			groups_join[i].orderKey = groups_o[i].orderKey;
			groups_join[i].orderPriority = groups_o[i].orderPriority;
			groups_join[i].count = groups_o[i].count;
		}

		for (int i = 0; i < s_problem_size; i++) {
			groups_join[s_problem_size + i].orderKey = groups_o[i].orderKey;
			groups_join[s_problem_size + i].orderPriority = groups_o[i].orderPriority;
			groups_join[s_problem_size + i].count = groups_o[i].count;
		}

		std::cout << "Verify inputs are sorted" << std::endl;

		Bit verifyOrder(true);
		for (int i = 0; i < s_problem_size - 1; i++) {
			Bit lessThanNext = groups_join[i].orderKey.geq(groups_join[i + 1].orderKey);
			verifyOrder = verifyOrder & !lessThanNext;
		}

		for (int i = s_problem_size; i < 2 * s_problem_size - 1; i++) {
			Bit greaterThanNext = groups_join[i].orderKey.geq(groups_join[i + 1].orderKey);
			verifyOrder = verifyOrder & greaterThanNext;
		}

		merge(groups_join, 0, 2 * s_problem_size, true);

		std::vector<row_12<width>> output_groups_join(s_problem_size);

		// Find the output.
		for (int i = 0; i < 2 * s_problem_size - 2; i += 2) {
			output_groups_join[i / 2] = dup_select_3(groups_join[i], groups_join[i + 1], groups_join[i + 2]);
		}
		output_groups_join[s_problem_size - 1] =
			dup_select_2(groups_join[2 * s_problem_size - 2], groups_join[2 * s_problem_size - 1]);

		std::cout << "Do a filter, exists operator" << std::endl;

		for (int i = 0; i < s_problem_size; i++) {
			Bit keep_row = output_groups_join[i].orderKey != Integer<width>(0, PUBLIC);
			output_groups_join[i].orderKey = Integer<width>(0, PUBLIC).select(keep_row, output_groups_join[i].orderKey);
			output_groups_join[i].orderPriority =
				Integer<width>(0, PUBLIC).select(keep_row, output_groups_join[i].orderPriority);
			output_groups_join[i].orderKey = Integer<width>(0, PUBLIC).select(keep_row, output_groups_join[i].orderKey);
		}

		std::cout << "Do groupby on the resulting group" << std::endl;
		groupBy(output_groups_join, 0, s_problem_size);
		// This is the final round, do the sum and then reveal the results.
		for (int i = 0; i < s_problem_size - 1; i++) {
			Integer<width> sum = output_groups_join[i].count + output_groups_join[i + 1].count;
			Bit equals = output_groups_join[i].orderPriority.equal(output_groups_join[i + 1].orderPriority);
			output_groups_join[i].orderKey = output_groups_join[i].orderKey.select(equals, Integer<width>(0, PUBLIC));
			output_groups_join[i].orderPriority =
				output_groups_join[i].orderPriority.select(equals, Integer<width>(0, PUBLIC));

			// Store resulting revenue inside extendedPrice
			output_groups_join[i].count = output_groups_join[i].count.select(equals, Integer<width>(0, PUBLIC));
			output_groups_join[i + 1].count = output_groups_join[i + 1].count.select(equals, sum);
		}

		// Copy into output groups

		std::cout << "Sorting output" << std::endl;
		std::vector<row_123_o<width>> final_groups(s_problem_size);
		for (int i = 0; i < s_problem_size; i++) {
			final_groups[i].orderPriority = output_groups_join[i].orderPriority;
			final_groups[i].count = output_groups_join[i].count;
		}

		// orderBy(final_groups, 0, s_problem_size, true);

		for (int i = 0; i < s_problem_size; i++) {
			output_data.push_back(final_groups[i].orderPriority);
			output_data.push_back(final_groups[i].count);
		}

		return;
	}

}
