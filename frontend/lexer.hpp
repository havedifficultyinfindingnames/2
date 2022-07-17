namespace frontend {
	struct token_t {
		char const* filename;
		token_e tokenkind;
		char const* begin;
		char const* end;
		std::size_t line;
		std::size_t column;
		std::string to_string() {
			return fast_io::concat(
				"Kind:", identifiers.at(std::to_underlying(tokenkind)),
				" (In file ", fast_io::mnp::os_c_str(filename),
				", Line:", line,
				" Column:", column,
				")");
		}
	};

	inline constexpr bool isspace(char c) noexcept
	{
		return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v';
	}
	inline constexpr bool is_inline_space(char c) noexcept
	{
		if (c == '\n') return false;
		return isspace(c);
	}
	inline constexpr bool isalpha(char c) noexcept
	{
		return ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z');
	}
	inline constexpr bool isdigit(char c) noexcept
	{
		return '0' <= c && c <= '9';
	}
	inline constexpr bool isxdigit(char c) noexcept
	{
		return ('0' <= c && c <= '9') || ('A' <= c && c <= 'F') || ('a' <= c && c <= 'f');
	}
	inline constexpr bool isalnum(char c) noexcept
	{
		return isalpha(c) || isdigit(c);
	}
	inline constexpr bool ispunct(char c) noexcept
	{
		switch (c)
		{
		case '!': case '"': case '#': case '$': case '%': case '&': case '\'':
		case '(': case ')': case '*': case '+': case ',': case '-': case '.':
		case '/': case ':': case ';': case '<': case '=': case '>': case '?':
		case '@': case '[': case '\\': case ']': case '^': case '_': case '`':
		case '{': case '|': case '}': case '~': return true;
		default: return false;
		}
	}

	struct scanner_t
	{
		char const* filename;
		char const* file_begin;
		char const* file_end;

		char const* ptr;
		std::size_t line_count;
		std::size_t column_count;

		scanner_t() = default;
		scanner_t(char const* file_name, std::string_view f)
			: filename(file_name)
			, file_begin(std::to_address(f.begin()))
			, file_end(std::to_address(f.end()))
			, ptr(std::to_address(f.begin()))
			, line_count(0)
			, column_count(0)
		{}
		template <std::size_t N>
		scanner_t(char const (&content)[N])
			: filename("")
			, file_begin(content)
			, file_end(content + N - 1)
			, ptr(content)
			, line_count(0)
			, column_count(0)
		{}

		void consume_comment(token_t& retval)
		{
			++ptr;
			if (ptr == file_end)
				unexpected_eof();
			else if (*ptr == '/') {
				while (++ptr < file_end) {
					if (*ptr == '\n') {
						++ptr;
						++line_count;
						column_count = 0;
						retval.end = ptr;
						retval.tokenkind = token_e::COMMENT;
						return;
					}
				}
				unexpected_eof();
			}
			else if (*ptr == '*') {
				while (++ptr < file_end) {
					++column_count;
					if (*ptr == '*') {
						++ptr;
						++column_count;
						if (ptr == file_end) {
							consume_exceptions(retval, "Annotation is not terminated.");
						}
						if (*ptr == '/') {
							++ptr;
							++column_count;
							retval.end = ptr;
							retval.tokenkind = token_e::COMMENT;
							return;
						}
						else if (*ptr == '*') {
							--ptr;
							--column_count;
						}
					}
				}
				unexpected_eof();
			}
			// not annotation
			else if (*ptr == '=') {
				++ptr;
				++column_count;
				retval.end = ptr;
				retval.tokenkind = token_e::MUL_ASSIGN;
				return;
			}
			else {
				retval.end = ptr;
				retval.tokenkind = token_e::MUL;
				return;
			}
		}
		void consume_string(token_t& retval)
		{
			while (++ptr < file_end) {
				++column_count;
				// don't consider the situation where '\n' follows '\\'
				if (*ptr == '\\') ++ptr;
				else if (*ptr == '\n') {
					goto STRING_NOT_TERMINATED;
				}
				else if (*ptr == '"') {
					++retval.begin;
					retval.end = ptr;
					++ptr;
					++column_count;
					retval.tokenkind = token_e::STRING;
					return;
				}
			}
		STRING_NOT_TERMINATED:
			consume_exceptions(retval, "String is not terminated.");
		}
		void consume_raw_string(token_t& retval)
		{
			// TODO
		}
		void consume_char(token_t& retval) {
			++ptr;
			++column_count;
			if (*ptr == '\'')
				goto EXCEPTION;
			else if (*ptr == '\\') {
				++ptr;
				++column_count;
			}
			++ptr;
			++column_count;
			if (*ptr != '\'') {
			EXCEPTION:
				consume_exceptions(retval, "Char literal should contain one character.");
			}
			retval.end = ptr;
			retval.tokenkind = token_e::INT_CHAR;
		}
		void consume_number(token_t& retval)
		{
			// ERROR!

			//UINT_LIT:
			//	0|[1-9][0-9]*
			//	0[bB][01]+
			//	0[0-7]+
			//	0[xX][0-9a-fA-F]+
			//FLOAT_LIT:
			//	([0-9]*\.[0-9]+|[0-9]+\.[0-9]*)([eE]-?[0-9]+)?
			//	0[xX]([0-9a-fA-F]*\.[0-9a-fA-F]+|[0-9a-fA-F]+\.[0-9a-fA-F]*)[pP]-?[0-9]+
			// The only special case not started with digit (e.g. ".123" and ".1e2") is not included here
			// So the FLOAT_LIT rule is now ([0-9]+\.[0-9]+|[0-9]+\.[0-9]*)([eE]-?[0-9]+)?
			if (*ptr == '0') {
				++ptr;
				++column_count;
				if (ptr == file_end)
					goto RETURN_TOKEN_0;
				// binary intergral
				else if (*ptr == 'b' || *ptr == 'B') {
					auto next_ptr = ptr + 1;
					if (next_ptr == file_end || (*next_ptr != '0' && *next_ptr != '1'))
						goto RETURN_TOKEN_0;
					while (++ptr < file_end) {
						++column_count;
						if (*ptr != '0' && *ptr != '1') {
							break;
						}
					}
					retval.end = ptr;
					retval.tokenkind = token_e::INT_BIN;
					return;
				}
				// hex
				else if (*ptr == 'x' || *ptr == 'X') {
					auto next_ptr = ptr + 1;
					if (next_ptr == file_end || !isxdigit(*next_ptr))
						goto RETURN_TOKEN_0;
					while (++ptr < file_end) {
						++column_count;
						if (!isxdigit(*ptr))
							break;
					}
					retval.end = ptr;
					retval.tokenkind = token_e::INT_HEX;
					if (ptr == file_end) return;
					// hex
					if (*ptr == 'p' || *ptr == 'P') {
						auto next_ptr = ptr + 1;
						if (next_ptr == file_end || !isdigit(*next_ptr)) {
							return;
						}
						while (++ptr < file_end) {
							++column_count;
							if (!isdigit(*ptr)) {
								break;
							}
						}
						retval.end = ptr;
						retval.tokenkind = token_e::FLOAT_HEX;
						return;
					}
					else if (*ptr == '.') {
						while (++ptr < file_end) {
							++column_count;
							if (!isxdigit(*ptr))
								break;
						}
						if (*ptr != 'p' && *ptr != 'P' && isdigit(*++ptr)) {
							consume_exceptions(retval, "Invalid hex float: missing binary exponent part.");
						}
						++column_count;
						while (++ptr < file_end) {
							++column_count;
							if (!isxdigit(*ptr))
								break;
						}
						retval.end = ptr;
						retval.tokenkind = token_e::FLOAT_HEX;
						return;
					}
					else
						return;
				}
				// decimal start with 0.
				else if (*ptr == '.') {
					while (++ptr < file_end) {
						++column_count;
						if (!isdigit(*ptr)) {
							break;
						}
					}
					if (*ptr == 'e' || *ptr == 'E') {
						auto next_ptr = ptr + 1;
						if (next_ptr == file_end || !isdigit(*next_ptr)) {
							return;
						}
						while (++ptr < file_end) {
							++column_count;
							if (!isdigit(*ptr)) {
								break;
							}
						}
					}
					retval.end = ptr;
					retval.tokenkind = token_e::FLOAT_DEC;
					return;
				}
				// oct integral or decimal float
				else if (isdigit(*ptr)) {
					while (++ptr < file_end) {
						++column_count;
						if (!('0' <= *ptr && *ptr <= '7')) {
							break;
						}
					}
					retval.end = ptr;
					retval.tokenkind = token_e::INT_OCT;
					// if there is digit, then may be decimal float
					if (ptr == file_end)
						return;
					if (*ptr == '.') {
						while (++ptr < file_end) {
							++column_count;
							if (!isdigit(*ptr)) {
								break;
							}
						}
						if (*ptr == 'e' || *ptr == 'E') {
							auto next_ptr = ptr + 1;
							if (next_ptr == file_end || !isdigit(*next_ptr)) {
								return;
							}
							while (++ptr < file_end) {
								++column_count;
								if (!isdigit(*ptr)) {
									break;
								}
							}
						}
						retval.end = ptr;
						retval.tokenkind = token_e::FLOAT_DEC;
						return;
					}
					else if (*ptr == 'e' || *ptr == 'E') {
						auto next_ptr = ptr + 1;
						// is still oct, but followed by a 'e'
						if (next_ptr == file_end || !isdigit(*next_ptr)) {
							return;
						}
						while (++ptr < file_end) {
							++column_count;
							if (!isdigit(*ptr)) {
								break;
							}
						}
						retval.end = ptr;
						retval.tokenkind = token_e::FLOAT_DEC;
						return;
					}
					else if (isdigit(*ptr)) {
						while (++ptr < file_end) {
							++column_count;
							if (!isdigit(*ptr))
								break;
						}
						if (*ptr == '.') {
							while (++ptr < file_end) {
								++column_count;
								if (!isdigit(*ptr))
									break;
							}
							retval.end = ptr;
							retval.tokenkind = token_e::FLOAT_DEC;
							return;
						}
						else if (*ptr == 'e' || *ptr == 'E') {
							auto next_ptr = ptr + 1;
							if (next_ptr == file_end || !isdigit(*next_ptr)) {
								consume_exceptions(retval, "Missing exponent part.");
							}
							while (++ptr < file_end) {
								++column_count;
								if (!isdigit(*ptr)) {
									break;
								}
							}
							retval.end = ptr;
							retval.tokenkind = token_e::FLOAT_DEC;
							return;
						}
						else
							consume_exceptions(retval, "Octave number should only contains 0-7.");
					}
					else
						return;
				}
				// 0 with other token
				else {
				RETURN_TOKEN_0:
					retval.end = ptr;
					retval.tokenkind = token_e::INT_DEC;
					return;
				}
			}
			// dec
			else {
				while (++ptr < file_end) {
					++column_count;
					if (!isdigit(*ptr)) break;
				}
				// float
				if (*ptr == '.') {
					while (++ptr < file_end) {
						++column_count;
						if (!isdigit(*ptr)) break;
					}
					retval.end = ptr;
					retval.tokenkind = token_e::FLOAT_DEC;
					return;
				}
				else if (*ptr == 'e' || *ptr == 'E') {
					auto next_ptr = ptr + 1;
					if (next_ptr == file_end)
						goto RETURN_TOKEN_INT;
					if (*next_ptr == '-') {
						++ptr;
						++column_count;
					}
					else if (!isdigit(*next_ptr))
						goto RETURN_TOKEN_INT;
					while (++ptr < file_end) {
						++column_count;
						if (!isdigit(*ptr)) break;
					}
					retval.end = ptr;
					retval.tokenkind = token_e::FLOAT_DEC;
					return;
				}
				else {
				RETURN_TOKEN_INT:
					retval.end = ptr;
					retval.tokenkind = token_e::INT_DEC;
					return;
				}
			}
		}
		void consume_dot(token_t& retval) noexcept
		{
			//FLOAT_LIT:
			//	([0-9]*\.[0-9]+|[0-9]+\.[0-9]*)([eE][0-9]+)?
			++ptr;
			++column_count;
			if (isdigit(*ptr)) {
				while (ptr < file_end && isdigit(*ptr)) {
					++ptr;
					++column_count;
				}
				if (ptr != file_end && (*ptr == 'e' || *ptr == 'E')) {
					auto next_ptr = ptr + 1;
					if (next_ptr == file_end || !isdigit(*(next_ptr))) {
						--ptr;
						goto FUNC_RETURN_FLOAT;
					}
					while (++ptr < file_end) {
						++column_count;
						if (!isdigit(*ptr))
							goto FUNC_RETURN_FLOAT;
					}
				}
				else
					goto FUNC_RETURN_FLOAT;
			}
			else {
				retval.end = ptr;
				retval.tokenkind = token_e::DOT;
				return;
			}
			if (false) {
			FUNC_RETURN_FLOAT:
				retval.end = ptr;
				retval.tokenkind = token_e::FLOAT_DEC;
				return;
			}
		}
		void consume_identifier(token_t& retval) noexcept
		{
			// [_a-zA-Z][_a-zA-Z0-9]*
			++ptr;
			++column_count;
			while (ptr < file_end && (isalnum(*ptr) || *ptr == '_')) {
				++ptr;
				++column_count;
			}
			retval.end = ptr;
			if (terminals.contains(std::string{ retval.begin, retval.end })) {
				retval.tokenkind = token_e{ terminals.at(std::string{ retval.begin, retval.end }) };
			}
			else
				retval.tokenkind = token_e::IDENT;
		}
		void consume_one_token_op(token_t& retval) noexcept
		{
			++ptr;
			++column_count;
			retval.end = ptr;
		}
		void consume_eof(token_t& retval) noexcept
		{
			retval = { filename, token_e::L_EOF, ptr, ptr, line_count, column_count };
		}
		template <std::size_t N>
		[[noreturn]] void consume_exceptions(token_t& retval, char const (&error_message)[N]) {
			while (ptr != file_end && *ptr != '\n') ++ptr;
			perrln(error_message, " In Line:", retval.line, ", Column:", retval.column, " in file ", fast_io::mnp::os_c_str(filename));
			throw std::exception{};
		}
		[[noreturn]] void unexpected_eof()
		{
			perrln("unexpected eof in file: ",
				fast_io::mnp::os_c_str(filename));
			throw std::exception{};
		}

		token_t next_token()
		{
			token_t retval;
			// skip white space
			while (ptr != file_end && is_inline_space(*ptr)) {
				++ptr;
				++column_count;
			}
			if (ptr == file_end) {
				consume_eof(retval);
				return retval;
			}
			else if (*ptr == '\n') {
				++ptr;
				++line_count;
				column_count = 0;
				return next_token();
			}
			retval.filename = filename;
			retval.line = line_count;
			retval.column = column_count;
			retval.begin = ptr;
			// annotation
			if (*ptr == '/') {
				consume_comment(retval);
				return retval;
			}
			// string
			else if (*ptr == '"') {
				consume_string(retval);
				return retval;
			}
			// raw string
			else if (*ptr == '`') {
				consume_raw_string(retval);
				return retval;
			}
			// char
			else if (*ptr == '\'') {
				consume_char(retval);
			}
			// number
			else if (isdigit(*ptr)) {
				consume_number(retval);
				return retval;
			}
			else if (*ptr == '.') {
				consume_dot(retval);
				return retval;
			}
			// identifier
			else if (isalpha(*ptr) || *ptr == '_') {
				consume_identifier(retval);
				return retval;
			}
			// change line
			else if (*ptr == '\\') {
				++ptr;
				if (*ptr == '\r')
					++ptr;
				if (*ptr != '\n')
					consume_exceptions(retval, "Invalid token '\\'");
				++ptr;
				++line_count;
				column_count = 0;
				return next_token();
			}
			// operators
			else if (*ptr == '(') {
				consume_one_token_op(retval);
				retval.tokenkind = token_e::LPAREN;
				return retval;
			}
			else if (*ptr == ')') {
				consume_one_token_op(retval);
				retval.tokenkind = token_e::RPAREN;
				return retval;
			}
			else if (*ptr == '[') {
				++ptr;
				++column_count;
				if (*ptr == '[') {
					consume_one_token_op(retval);
					retval.tokenkind = token_e::LDBRACKET;
				}
				else {
					retval.end = ptr;
					retval.tokenkind = token_e::LBRACKET;
					return retval;
				}
			}
			else if (*ptr == ']') {
				++ptr;
				++column_count;
				if (*ptr == ']') {
					consume_one_token_op(retval);
					retval.tokenkind = token_e::RDBRACKET;
				}
				else {
					retval.end = ptr;
					retval.tokenkind = token_e::RBRACKET;
					return retval;
				}
			}
			else if (*ptr == '{') {
				consume_one_token_op(retval);
				retval.tokenkind = token_e::LBRACE;
				return retval;
			}
			else if (*ptr == '}') {
				consume_one_token_op(retval);
				retval.tokenkind = token_e::RBRACE;
				return retval;
			}
			else if (*ptr == ',') {
				consume_one_token_op(retval);
				retval.tokenkind = token_e::COMMA;
				return retval;
			}
			else if (*ptr == ':') {
				++ptr;
				++column_count;
				if (*ptr == ':') {
					consume_one_token_op(retval);
					retval.tokenkind = token_e::SCOPE;
				}
				else {
					retval.end = ptr;
					retval.tokenkind = token_e::COLON;
					return retval;
				}
			}
			else if (*ptr == ';') {
				consume_one_token_op(retval);
				retval.tokenkind = token_e::SEMICOLON;
				return retval;
			}
			else if (*ptr == '+') {
				++ptr;
				++column_count;
				if (*ptr == '+') {
					consume_one_token_op(retval);
					retval.tokenkind = token_e::DADD;
					return retval;
				}
				else if (*ptr == '=') {
					consume_one_token_op(retval);
					retval.tokenkind = token_e::ADD_ASSIGN;
					return retval;
				}
				else {
					retval.end = ptr;
					retval.tokenkind = token_e::ADD;
					return retval;
				}
			}
			else if (*ptr == '-') {
				++ptr;
				++column_count;
				if (*ptr == '-') {
					consume_one_token_op(retval);
					retval.tokenkind = token_e::DSUB;
					return retval;
				}
				else if (*ptr == '=') {
					consume_one_token_op(retval);
					retval.tokenkind = token_e::SUB_ASSIGN;
					return retval;
				}
				else if (*ptr == '>') {
					consume_one_token_op(retval);
					retval.tokenkind = token_e::POINTER;
					return retval;
				}
				else {
					retval.end = ptr;
					retval.tokenkind = token_e::SUB;
					return retval;
				}
			}
			else if (*ptr == '*') {
				++ptr;
				++column_count;
				if (*ptr == '=') {
					consume_one_token_op(retval);
					retval.tokenkind = token_e::MUL_ASSIGN;
					return retval;
				}
				else {
					retval.end = ptr;
					retval.tokenkind = token_e::MUL;
					return retval;
				}
			}
			else if (*ptr == '%') {
				++ptr;
				++column_count;
				if (*ptr == '=') {
					consume_one_token_op(retval);
					retval.tokenkind = token_e::MOD_ASSIGN;
					return retval;
				}
				else {
					retval.end = ptr;
					retval.tokenkind = token_e::MOD;
					return retval;
				}
			}
			else if (*ptr == '&') {
				++ptr;
				++column_count;
				if (*ptr == '&') {
					consume_one_token_op(retval);
					retval.tokenkind = token_e::LAND;
				}
				else if (*ptr == '=') {
					consume_one_token_op(retval);
					retval.tokenkind = token_e::BAND_ASSIGN;
					return retval;
				}
				else {
					retval.end = ptr;
					retval.tokenkind = token_e::BAND;
					return retval;
				}
			}
			else if (*ptr == '|') {
				++ptr;
				++column_count;
				if (*ptr == '|') {
					consume_one_token_op(retval);
					retval.tokenkind = token_e::LOR;
				}
				else if (*ptr == '=') {
					consume_one_token_op(retval);
					retval.tokenkind = token_e::BOR_ASSIGN;
					return retval;
				}
				else {
					retval.end = ptr;
					retval.tokenkind = token_e::BOR;
					return retval;
				}
			}
			else if (*ptr == '^') {
				++ptr;
				++column_count;
				if (*ptr == '=') {
					consume_one_token_op(retval);
					retval.tokenkind = token_e::BXOR_ASSIGN;
					return retval;
				}
				else {
					retval.end = ptr;
					retval.tokenkind = token_e::XOR;
					return retval;
				}
			}
			else if (*ptr == '<') {
				++ptr;
				++column_count;
				if (*ptr == '<') {
					++ptr;
					++column_count;
					if (*ptr == '=') {
						consume_one_token_op(retval);
						retval.tokenkind = token_e::SHL_ASSIGN;
						return retval;
					}
					else {
						retval.end = ptr;
						retval.tokenkind = token_e::SHL;
						return retval;
					}
				}
				else if (*ptr == '=') {
					++ptr;
					++column_count;
					if (*ptr == '>') {
						consume_one_token_op(retval);
						retval.tokenkind = token_e::COMPARE;
						return retval;
					}
					else {
						retval.end = ptr;
						retval.tokenkind = token_e::LE;
						return retval;
					}
				}
				else {
					retval.end = ptr;
					retval.tokenkind = token_e::LT;
					return retval;
				}
			}
			else if (*ptr == '>') {
				++ptr;
				++column_count;
				if (*ptr == '>') {
					++ptr;
					++column_count;
					if (*ptr == '=') {
						consume_one_token_op(retval);
						retval.tokenkind = token_e::SHR_ASSIGN;
						return retval;
					}
					else {
						retval.end = ptr;
						retval.tokenkind = token_e::SHR;
						return retval;
					}
				}
				else if (*ptr == '=') {
					consume_one_token_op(retval);
					retval.tokenkind = token_e::GE;
					return retval;
				}
				else {
					retval.end = ptr;
					retval.tokenkind = token_e::GT;
					return retval;
				}
			}
			else if (*ptr == '=') {
				++ptr;
				++column_count;
				if (*ptr == '=') {
					consume_one_token_op(retval);
					retval.tokenkind = token_e::EQ;
					return retval;
				}
				else {
					retval.end = ptr;
					retval.tokenkind = token_e::ASSIGN;
					return retval;
				}
			}
			else
				consume_exceptions(retval, "Invalid letter.");
//			__assume(0);
		}
		token_t peek_token() const
		{
			auto tmp = *this;
			return tmp.next_token();
		}
	};
}