#include "utils.hpp"

#include "frontend/frontend.hpp"

int main()
{
	frontend::init_terminal_identifiers();
	frontend::parser_t parser{ "a.gf" };
	
	frontend::scanner_t s(R"(
int main()
{
	puts("Hello, World!");
}
)");
	auto t = s.next_token();
	while (t.tokenkind != frontend::token_e::L_EOF) {
		println(t.to_string());
		t = s.next_token();
	}
}