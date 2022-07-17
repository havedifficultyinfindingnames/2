#pragma once
#include "../utils.hpp"

#include "../filesystem/filesystem.hpp"

namespace frontend {
	enum class token_e : std::size_t {
		ILLEGAL = static_cast<unsigned>(-1),
		L_EOF = 0,
		COMMENT = 1,

		literal_beg = 1,
		// Number type has to be parsed during tokenizing.
		// So keep the information.
		INT_CHAR,		// 'a'
		INT_BIN,		// 0b01
		INT_OCT,		// 012
		INT_DEC,		// 0, 123
		INT_HEX,		// 0x1f
		FLOAT_DEC,			// 0.1, 0.1e0
		FLOAT_HEX,		// 0x1f.2p3
		STRING,			// ""
		RAW_STRING,		// ``, ###``###
		IDENT,			// main
		literal_end,

		keyword_beg = literal_end,
#define KEYWORD(x, y) x,
#define OPERATOR(x, y)
#include TOKEN_GRAMMAR_FILE
#undef OPERATOR
#undef KEYWORD
		keyword_end,

		operator_beg = keyword_end,
#define KEYWORD(x, y)
#define OPERATOR(x, y) x,
#include TOKEN_GRAMMAR_FILE
#undef OPERATOR
#undef KEYWORD
		operator_end,
	};

	inline std::unordered_map<std::string, std::size_t> nonterminals;
	// may be std::vector is better because the indexes are mainly contiguous
	inline std::unordered_map<std::size_t, std::string> identifiers;
	inline std::unordered_map<std::string, std::size_t> terminals;
	inline void init_terminal_identifiers()
	{
		using enum token_e;
		terminals.emplace("l_eof", static_cast<std::size_t>(L_EOF));
		identifiers.emplace(static_cast<std::size_t>(L_EOF), "l_eof");
		terminals.emplace("l_comment", static_cast<std::size_t>(COMMENT));
		identifiers.emplace(static_cast<std::size_t>(COMMENT), "l_comment");
		terminals.emplace("l_int_char", static_cast<std::size_t>(INT_CHAR));
		identifiers.emplace(static_cast<std::size_t>(INT_CHAR), "l_int_char");
		terminals.emplace("l_int_bin", static_cast<std::size_t>(INT_BIN));
		identifiers.emplace(static_cast<std::size_t>(INT_BIN), "l_int_bin");
		terminals.emplace("l_int_oct", static_cast<std::size_t>(INT_OCT));
		identifiers.emplace(static_cast<std::size_t>(INT_OCT), "l_int_oct");
		terminals.emplace("l_int_dec", static_cast<std::size_t>(INT_DEC));
		identifiers.emplace(static_cast<std::size_t>(INT_DEC), "l_int_dec");
		terminals.emplace("l_int_hex", static_cast<std::size_t>(INT_HEX));
		identifiers.emplace(static_cast<std::size_t>(INT_HEX), "l_int_hex");
		terminals.emplace("l_float_dec", static_cast<std::size_t>(FLOAT_DEC));
		identifiers.emplace(static_cast<std::size_t>(FLOAT_DEC), "l_float_dec");
		terminals.emplace("l_float_hex", static_cast<std::size_t>(FLOAT_HEX));
		identifiers.emplace(static_cast<std::size_t>(FLOAT_HEX), "l_float_hex");
		terminals.emplace("l_string", static_cast<std::size_t>(STRING));
		identifiers.emplace(static_cast<std::size_t>(STRING), "l_string");
		terminals.emplace("l_raw_string", static_cast<std::size_t>(RAW_STRING));
		identifiers.emplace(static_cast<std::size_t>(RAW_STRING), "l_raw_string");
		terminals.emplace("l_ident", static_cast<std::size_t>(IDENT));
		identifiers.emplace(static_cast<std::size_t>(IDENT), "l_ident");
#define KEYWORD(x, y)									\
	terminals.emplace(y, static_cast<std::size_t>(x));	\
	identifiers.emplace(static_cast<std::size_t>(x), y);
#define OPERATOR(x, y)									\
	terminals.emplace(y, static_cast<std::size_t>(x));	\
	identifiers.emplace(static_cast<std::size_t>(x), y);
#include TOKEN_GRAMMAR_FILE
#undef OPERATOR
#undef KEYWORD
	}

	inline constexpr bool is_literal(token_e t) noexcept {
		return token_e::literal_beg < t&& t < token_e::literal_end;
	}
	inline constexpr bool is_keyword(token_e t) noexcept {
		return token_e::keyword_beg < t&& t < token_e::keyword_end;
	}
	inline constexpr bool is_operator(token_e t) noexcept {
		return token_e::operator_beg < t&& t < token_e::operator_end;
	}
	inline constexpr bool is_int(token_e t) noexcept {
		return t == token_e::INT_BIN ||
			t == token_e::INT_DEC ||
			t == token_e::INT_HEX ||
			t == token_e::INT_OCT;
	}
	inline constexpr bool is_float(token_e t) noexcept {
		return t == token_e::FLOAT_DEC ||
			t == token_e::FLOAT_HEX;
	}
	inline constexpr bool is_number(token_e t) noexcept {
		return is_int(t) || is_float(t);
	}
	inline constexpr bool is_string(token_e t) noexcept {
		return t == token_e::STRING ||
			t == token_e::RAW_STRING;
	}
	inline bool is_keyword(std::string_view str) noexcept {
		return terminals.contains(std::string{ str });
	}
	inline bool is_identifier(char* begin, std::size_t n) noexcept
	{
		// [_a-zA-Z][_a-zA-Z0-9]*
		if (n == 0 || is_keyword({ begin, n }))
			return false;
		else
		{
			for (auto end{ begin + n }; begin < end; ++begin)
			{
				if (!isalnum(*begin) && *begin != '_')
					return false;
			}
			return true;
		}
	}

	struct identifier_t {
		std::size_t index;
		constexpr auto operator<=>(identifier_t const&) const noexcept = default;
		constexpr bool is_terminal() const noexcept {
			return static_cast<std::make_signed_t<decltype(index)>>(index) >= 0;
		}
		constexpr bool is_nonterminal() const noexcept {
			return !is_terminal();
		}
		std::string to_string() const noexcept {
			return identifiers[index];
		}
	};
} // namespace frontend
namespace std {
	template <> struct hash<frontend::identifier_t> {
		auto operator()(frontend::identifier_t const& i) const noexcept {
			return hash<size_t>{}(i.index);
		}
	};
} // namespace std

#include "lexer.hpp"
#include "parser.hpp"
