namespace frontend {
	inline std::unordered_multimap<identifier_t, std::vector<identifier_t>> productions;

	inline std::size_t nt_index = (sizeof(std::size_t) == 8) ? 0x8000'0000'0000'0000 : 0x8000'0000;
	inline identifier_t get_terminal(std::string s) {
		if (terminals.contains(s)) return { terminals.at(s) };
		else {
			panic("terminal \"", s, "\" not defined in lexer");
		}
	}
	inline identifier_t get_nonterminal(std::string s) {
		if (nonterminals.contains(s)) return { nonterminals.at(s) };
		else {
			nonterminals.emplace(s, nt_index);
			identifiers.emplace(nt_index, s);
			return { nt_index++ };
		}
	}

	struct identifier_group_t {
		std::uint8_t n;
		identifier_t arr[7];
		constexpr bool push_back(identifier_t const& i) noexcept {
			if (n == 7) return false;
			/*else*/
			arr[n++] = i;
			return true;
		}
		constexpr bool is_empty() const noexcept {
			return n == 0;
		}
	};

	struct grammar_file_parser_t {
		identifier_t lhs{};
		std::vector<identifier_group_t> rhs;
		std::vector<std::uint8_t> option_set;
		std::vector<identifier_t> buffer;
		bool is_parsing_rhs{};
		void init() {
			rhs.clear();
			option_set.clear();
			buffer.clear();
			rhs.resize(1);
			option_set.resize(1);
		}
		grammar_file_parser_t() {
			init();
		}
		void finalize(std::size_t i) {
			if (i == option_set.size()) {
				productions.emplace(lhs, buffer);
				return;
			}
			auto const& cur_group = rhs[i];
			if (option_set[i] == 0) {
				buffer.insert(buffer.end(), cur_group.arr, cur_group.arr + cur_group.n);
				return finalize(i + 1);
			}
			else {
				auto cur_size = buffer.size();
				finalize(i + 1);
				buffer.insert(buffer.begin() + cur_size, cur_group.arr, cur_group.arr + cur_group.n);
				return finalize(i + 1);
			}
		}
		void parse_file(char const* grammar_file_name) {
			// reuse scanner
			fast_io::native_file_loader fl{ fast_io::mnp::os_c_str(grammar_file_name) };
			scanner_t lex{ grammar_file_name, {fl.begin(), fl.end()} };
			token_t token;
			bool unbalanced{};		
			while ((token = lex.next_token()).tokenkind != token_e::L_EOF) {
				identifier_t ident;
				if (token.tokenkind == token_e::IDENT && !(*token.begin == 'l' && *(token.begin + 1) == '_'))
					ident = get_nonterminal(std::string{ token.begin, token.end });
				else
					ident = get_terminal(std::string{ token.begin, token.end });
				if (!is_parsing_rhs) {
					if (token.tokenkind == token_e::ASSIGN) // '='
						is_parsing_rhs = true;
					else if (ident.is_nonterminal())
						lhs = ident;
					else [[unlikely]]
						panic("the lhs of the production must be a nonterminal, but found \"", identifiers[ident.index], "\"\n");
					continue;
				}
				/*else*/
				if (token.tokenkind == token_e::SEMICOLON) { // ';'
					if (unbalanced) {
						rhs.pop_back();
						unbalanced = false;
					}
					finalize(0);
					init();
					is_parsing_rhs = false;
				}
				else if (token.tokenkind == token_e::BOR) { // '|'
					if (unbalanced) {
						rhs.pop_back();
						unbalanced = false;
					}
					finalize(0);
					init();
				}
				else if (token.tokenkind == token_e::LBRACKET) { // '['
					option_set.push_back(1);
					if (!unbalanced)
						rhs.push_back({});
					else
						unbalanced = false;
				}
				else if (token.tokenkind ==token_e::RBRACKET) { // ']'
					rhs.push_back({});
					unbalanced = true;
				}
				else {
					if (unbalanced) {
						option_set.push_back(0);
						unbalanced = false;
					}
					if (rhs.back().push_back(ident)) // success
						continue;
					/*else*/
					if (option_set.back()) [[unlikely]]
						panic("too many option terms!(> ", sizeof(identifier_group_t) / sizeof(identifier_t) - 1, ") Turn the identifier_group_t larger");
					else {
						option_set.push_back(0);
						rhs.push_back({});
						rhs.back().push_back(ident);
					}
				}
			}
			if (is_parsing_rhs)
				panic("unexpected eof, possibly missing ';' at the end\n");
		}
	};

	inline void check_all_nonterminals_with_productions()
	{
		for (auto const& nt : nonterminals) {
			auto [beg_itr, end_itr] = productions.equal_range(identifier_t{ nt.second });
			if (beg_itr == productions.end())
				panic("\"", nt.first, "\" don't have its production\n");
		}
		for (auto const& p : productions) {
			if (p.second.size() == 0)
				panic("\"", identifiers.at(p.first.index), "\"'s production is empty, which is not suportted\n");
		}
	}

	struct item_t {
		identifier_t lhs;
		decltype(std::to_address(productions.begin())) prod;
		std::size_t step;
		identifier_t lookahead;
		bool operator==(item_t const& other) const noexcept = default;
		bool same_core_with(item_t const& other) const noexcept {
			return prod == other.prod && step == other.step;
		}
		bool is_reducable() const noexcept {
			return step == prod->second.size();
		}
		bool is_shiftable() const noexcept {
			return !is_reducable();
		}
		void pretty_print() const {
			print(identifiers[lhs.index], " ->");
			for (std::size_t i{}; i < step; ++i) {
				print(" ", identifiers[prod->second.at(i).index]);
			}
			print(" .");
			for (std::size_t i{ step }; i < prod->second.size(); ++i) {
				print(" ", identifiers[prod->second.at(i).index]);
			}
			println("\t, ", identifiers[lookahead.index]);
		}
	};

	inline void first(identifier_t other, std::unordered_set<identifier_t>& lookahead) noexcept {
		if (other.is_terminal()) {
			lookahead.insert(other);
			return;
		}
		// else
		auto [beg_itr, end_itr] = productions.equal_range(other);
		for (; beg_itr != end_itr; ++beg_itr) {
			// if a production is directly left recurssive
			// then it will contribute no first element
			if (beg_itr->second[0] == other)
				continue;
			else
				first(beg_itr->second[0], lookahead);
		}
	}

	struct state_t {
		std::vector<item_t> items;
		// a state is viewed as some basic items and the items that are derived from the former
		// the cnt to note which are the basic items
		// the first {cnt} items are basic
		// considering this, the items can't be stored in std::set
		std::size_t cnt;
		std::unordered_map<identifier_t, std::size_t> next;
	};
	inline bool operator==(state_t const& lhs, state_t const& rhs) noexcept {
		if (lhs.cnt != rhs.cnt) return false;
		for (std::size_t i{}; i < lhs.cnt; ++i) {
			for (std::size_t j{}; j < lhs.cnt; ++j) {
				if (lhs.items[i] == rhs.items[j]) goto SUCCESS;
			}
			return false;
		SUCCESS:; // continue
		}
		return true;
	}
	inline bool same_core_with_helper(state_t const& lhs, state_t const& rhs) noexcept {
		if (lhs.cnt != rhs.cnt) return false;
		for (std::size_t i{}; i < lhs.cnt; ++i) {
			for (std::size_t j{}; j < lhs.cnt; ++j) {
				if (lhs.items[i].same_core_with(rhs.items[j])) goto SUCCESS;
			}
			return false;
		SUCCESS:; // continue
		}
		return true;
	}
	inline bool same_core_with(state_t const& lhs, state_t const& rhs) noexcept {
		return same_core_with_helper(lhs, rhs) && same_core_with_helper(lhs, rhs);
	}

	using project_t = std::vector<state_t>;

	project_t make_project() noexcept
	{
		project_t c = { state_t{{item_t{productions.begin()->first, std::to_address(productions.begin()), 0, {{0}}}}, 1, {}} };
		for (std::size_t i{}; i < c.size(); ++i) {
			auto& cur_state = c[i];
			auto& cur_items = cur_state.items;
			std::unordered_set<identifier_t> handled_identifiers;
			for (std::size_t j{}; j < cur_items.size(); ++j) {
				auto const& item_ref = cur_items[j];
				if (item_ref.is_reducable()) continue;
				auto const& next_identifier = item_ref.prod->second.at(item_ref.step);
				handled_identifiers.insert(next_identifier);
				if (next_identifier.is_terminal()) continue;
				// is nonterminal
				// need to add the related production
				auto [prod_beg_itr, prod_end_itr] = productions.equal_range(next_identifier);
				// generate item for each lookahead identifier
				auto apply = [&, cur_items_size = cur_items.size()](identifier_t lookahead) {
					for (; prod_beg_itr != prod_end_itr; ++prod_beg_itr) {
						// pushback items that do not exist
						// assert during every path, the newly added items don't repeat
						item_t tmp_item{ next_identifier, std::to_address(prod_beg_itr), 0, lookahead };
						for (std::size_t i{}; i < cur_items_size; ++i) {
							if (tmp_item == cur_items[i]) return;
						}
						cur_items.push_back(std::move(tmp_item));
					}
				};
				// first(NULL) == NULL, so the lookahead is inheritated
				if (item_ref.step == item_ref.prod->second.size() - 1) {
					apply(item_ref.lookahead);
				}
				else {
					std::unordered_set<identifier_t> lookahead;
					first(item_ref.prod->second.at(item_ref.step + 1), lookahead);
					if (lookahead.contains({ 0 })) {
						lookahead.insert(item_ref.lookahead);
					}
					std::ranges::for_each(lookahead, apply);
				}
			}
			// produce next state(s)
			// 0 for unhandled, 1 for used
			std::vector<std::uint8_t> handled_items(cur_items.size());
			c.reserve(c.size() + handled_identifiers.size());
			auto& updated_items = c[i].items;
			for (auto const& ident : handled_identifiers) {
				state_t tmp_state;
				for (std::size_t j{}; j < handled_items.size(); ++j) {
					if (handled_items[j]) continue;
					auto const& cur_item = updated_items[j];
					if (cur_item.is_reducable()) {
						handled_items[j] = true;
						continue;
					}
					if (ident != cur_item.prod->second.at(cur_item.step)) continue;
					// now the item can pass the identifier i to the following item
					item_t tmp_item = cur_item;
					++tmp_item.step;
					tmp_state.items.push_back(std::move(tmp_item));
					handled_items[j] = true;
				}
				tmp_state.cnt = tmp_state.items.size();
				// before doing these two
				// cur_state.next.insert({ i, c.end() });
				// c.push_back(state_t{ tmp_state, {} });
				// whether tmp_state is equal to the previous state should be judged first.
				for (auto c_itr = c.begin(); c_itr != c.end(); ++c_itr) {
					if (tmp_state == *c_itr) {
						c[i].next.insert({ ident, c_itr - c.begin() });
						goto NEXT_LOOP;
					}
				}
				{
					c[i].next.insert({ ident, c.size() });
					c.push_back(std::move(tmp_state));
				}
			NEXT_LOOP:;
			}
		}
		return c;
	}

	enum class action_e : std::uint8_t {
		ERROR = 0,
		SHIFT,
		REDUCE,
		ACC
	};

	struct action_table_unit_t {
		action_e action;
		union {
			std::size_t state;
			decltype(std::to_address(productions.begin())) prod;
		} des;
		using act_t = void(*)(void);
		act_t other_act;
	};
	struct goto_table_unit_t {
		std::size_t des;
	};

	struct table_t {
		action_table_unit_t* action_table;
		goto_table_unit_t* goto_table;
		std::size_t terminal_cnt;
		std::size_t nonterminal_cnt;
		std::size_t state_cnt;
		table_t() : action_table(nullptr), goto_table(nullptr), terminal_cnt(0), nonterminal_cnt(0), state_cnt(0) {}
		table_t(std::size_t tc, std::size_t nc, std::size_t sc)
			: action_table(new action_table_unit_t[sc * tc]{}), goto_table(new goto_table_unit_t[sc * nc])
			, terminal_cnt(tc), nonterminal_cnt(nc), state_cnt(sc)
		{
			std::fill_n(goto_table, sc * nc, goto_table_unit_t{ static_cast<std::size_t>(-1) });
		}
		table_t(table_t const&) = delete;
		table_t(table_t&& other) noexcept : table_t() {
			*this = std::move(other);
		}
		table_t& operator=(table_t const&) = delete;
		table_t& operator=(table_t&& other) noexcept {
			if (this == &other) return *this;
			terminal_cnt = other.terminal_cnt;
			nonterminal_cnt = other.nonterminal_cnt;
			state_cnt = other.state_cnt;
			delete[] action_table;
			delete[] goto_table;
			action_table = other.action_table;
			goto_table = other.goto_table;
			other.action_table = nullptr;
			other.goto_table = nullptr;
			return *this;
		}
		~table_t() {
			delete[] action_table;
			delete[] goto_table;
		}
		auto action_table_at(std::size_t row) noexcept {
			assert(row < state_cnt);
			return action_table + row * terminal_cnt;
		}
		auto& action_table_at(std::size_t row, std::size_t col) noexcept {
			assert(row < state_cnt);
			assert(col < terminal_cnt);
			return action_table[row * terminal_cnt + col];
		}
		auto& action_table_at(std::size_t row, identifier_t col) noexcept {
			return action_table_at(row, col.index);
		}
		auto goto_table_at(std::size_t row) noexcept {
			assert(row < state_cnt);
			return goto_table + row * nonterminal_cnt;
		}
		auto& goto_table_at(std::size_t row, std::size_t col) noexcept {
			assert(row < state_cnt);
			assert(col < nonterminal_cnt);
			return goto_table[row * nonterminal_cnt + col];
		}
		auto& goto_table_at(std::size_t row, identifier_t col) noexcept {
			return goto_table_at(row, col.index & 0x7fff'ffff'ffff'ffff);
		}
		auto action_table_at(std::size_t row) const noexcept {
			return action_table + row * terminal_cnt;
		}
		auto const& action_table_at(std::size_t row, std::size_t col) const noexcept {
			return action_table[row * terminal_cnt + col];
		}
		auto const& action_table_at(std::size_t row, identifier_t col) const noexcept {
			return action_table_at(row, col.index);
		}
		auto goto_table_at(std::size_t row) const noexcept {
			return goto_table + row * nonterminal_cnt;
		}
		auto const& goto_table_at(std::size_t row, std::size_t col) const noexcept {
			return goto_table[row * nonterminal_cnt + col];
		}
		auto const& goto_table_at(std::size_t row, identifier_t col) const noexcept {
			return goto_table_at(row, col.index & 0x7fff'ffff'ffff'ffff);
		}
	};

	inline table_t make_table(project_t const& c)
	{
		table_t table{ terminals.size(), nonterminals.size(), c.size() };
		std::size_t i{};
		for (auto const& s : c) {
			for (auto const& cur_ident : s.next) {
				if (cur_ident.first.is_terminal())
					table.action_table_at(i, cur_ident.first) = { action_e::SHIFT, {.state = cur_ident.second}, nullptr };
				else
					table.goto_table_at(i, cur_ident.first) = { .des = cur_ident.second };
			}
			for (auto const& cur_item : s.items) {
				if (!cur_item.is_reducable()) continue;
				auto& action_table_unit = table.action_table_at(i, cur_item.lookahead);
				if (action_table_unit.action != action_e::ERROR)
					panic("shift-reduce conflict!");
				action_table_unit = { action_e::REDUCE, {.prod = cur_item.prod}, nullptr };
			}
			++i;
		}
		// if 2 states have "same core" with each other
		// that is, all items in it except for their lookahead set are the same
		// then the 2 states with the same core may be combined into 1 state when
		// the 2 lines in the table doesn't interfere with each other.
		std::vector<std::size_t> redirect_table(table.state_cnt);
		for (std::size_t i{}; i < table.state_cnt; ++i) {
			redirect_table[i] = i;
		}
		std::size_t new_state_cnt{ table.state_cnt };
		for (std::size_t i{}; i < table.state_cnt; ++i) {
			for (std::size_t j{ i + 1 }; j < table.state_cnt; ++j) {
				if (!same_core_with(c[i], c[j]))
					goto NEXT_LOOP_FOR_J;
				// else
				for (std::size_t te_index{}; te_index < table.terminal_cnt; ++te_index) {
					auto const& table_unit_i = table.action_table_at(i, te_index);
					auto const& table_unit_j = table.action_table_at(j, te_index);
					if (table_unit_i.action == action_e::ERROR || table_unit_j.action == action_e::ERROR) continue;
					if (table_unit_i.action != table_unit_j.action) {
						// exist conflict, cannot combine
						goto NEXT_LOOP_FOR_J;
					}
					else {
						if (table_unit_i.action == action_e::ACC) continue;
						else if (table_unit_i.action == action_e::SHIFT &&
							redirect_table[table_unit_i.des.state] == redirect_table[table_unit_j.des.state]) continue;
						else if (table_unit_i.action == action_e::REDUCE &&
							table_unit_i.des.prod == table_unit_j.des.prod &&
							table_unit_i.other_act == table_unit_j.other_act) continue;
						else {
							// exist conflict
							goto NEXT_LOOP_FOR_J;
						}
					}
				}
				for (std::size_t nt_index{}; nt_index < table.nonterminal_cnt; ++nt_index) {
					if (auto const& table_unit_i = table.goto_table_at(i, nt_index);
						table_unit_i.des == static_cast<std::size_t>(-1)) continue;
					else if (auto const& table_unit_j = table.goto_table_at(j, nt_index);
						table_unit_j.des != static_cast<std::size_t>(-1)) {
						if (redirect_table[table_unit_i.des] == redirect_table[table_unit_j.des]) continue;
						goto NEXT_LOOP_FOR_J;
					}
				}
				// has no conflict
				// do 1. combine 2 lines
				//	  2. record the redirect table
				//	  3. --new_state_cnt
				for (std::size_t te_index{}; te_index < table.terminal_cnt; ++te_index) {
					if (auto const& table_unit_j = table.action_table_at(j, te_index);
						table_unit_j.action != action_e::ERROR) {
						table.action_table_at(i, te_index) = table_unit_j;
					}
				}
				for (std::size_t nt_index{}; nt_index < table.nonterminal_cnt; ++nt_index) {
					if (auto const& table_unit_j = table.goto_table_at(j, nt_index);
						table_unit_j.des != static_cast<std::size_t>(-1)) {
						table.goto_table_at(i, nt_index) = table_unit_j;
					}
				}
				redirect_table[j] = redirect_table[i];
				for (std::size_t k{ j + 1 }; k < table.state_cnt; ++k)
					--redirect_table[k];
				--new_state_cnt;
			NEXT_LOOP_FOR_J:;
			}
		}
		++new_state_cnt;
		table_t new_table{ table.terminal_cnt, table.nonterminal_cnt, new_state_cnt };
		struct vector_bool {
			std::uint8_t* data;
			std::size_t size;
			vector_bool(std::size_t size_)
				: data(new std::uint8_t[size_ / 8 + 1]{}), size(size_)
			{ }
			~vector_bool() {
				delete[] data;
			}
			bool operator[](std::size_t pos) const noexcept {
				return data[pos / 8] & (1u << (pos % 8));
			}
			struct reference {
				std::uint8_t* data;
				std::uint8_t pos;
				reference& operator=(bool other) {
					if (other)
						*data |= (1u << pos);
					else
						*data &= ~(1u << pos);
					return *this;
				}
				operator bool() const noexcept {
					return *data & (1u << pos);
				}
			};
			reference operator[](std::size_t pos) noexcept {
				return { data + pos / 8, std::uint8_t(pos % 8) };
			}
		};
		vector_bool copied(new_state_cnt);
		// delete and redirect the useless lines according to redirect_table
		for (std::size_t from_i{}; from_i < table.state_cnt; ++from_i) {
			auto to_i{ redirect_table[from_i] };
			if (copied[to_i]) continue;
			std::memcpy(new_table.action_table_at(to_i), table.action_table_at(from_i), sizeof(action_table_unit_t) * table.terminal_cnt);
			std::memcpy(new_table.goto_table_at(to_i), table.goto_table_at(from_i), sizeof(goto_table_unit_t) * table.nonterminal_cnt);
			copied[to_i] = true;
		}
		return new_table;
	}

	inline void cache_table(table_t const& table, char const* filename)
	{
		// todo
	}
	inline table_t load_table(fast_io::native_file_loader&& file)
	{
		// todo
		return{};
	}

	struct parser_t {
		scanner_t scanner;
		table_t table;
		std::vector<std::size_t> state_stack;
		std::vector<std::size_t> identifier_stack;
		std::vector<void*> prop_stack;
		parser_t(char const* grammar_file_name)
		{
			std::string tmp_file_name;
			auto len = std::strlen(grammar_file_name);
			tmp_file_name.resize(len + 3);
			std::memcpy(tmp_file_name.data(), grammar_file_name, len);
			std::memcpy(tmp_file_name.data() + len, ".o", 3);
			try
			{
				table = load_table(fast_io::native_file_loader{ tmp_file_name });
			}
			catch (fast_io::error)
			{
				grammar_file_parser_t{}.parse_file(grammar_file_name);
				check_all_nonterminals_with_productions();
				auto project = make_project();
				table = make_table(project);
				cache_table(table, tmp_file_name.c_str());
			}
		}
	};

}
