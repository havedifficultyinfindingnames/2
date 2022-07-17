#pragma once
#include "../utils.hpp"


namespace fs {
struct file_t
{
	char const* filename;
	fast_io::native_file_loader fileloader;
	file_t(char const* file_name, fast_io::native_file_loader&& file_loader)
		: filename(file_name), fileloader(std::move(file_loader)) {}
	file_t(char const* file_name) : filename(file_name),
		fileloader(fast_io::mnp::os_c_str(file_name)) {}
	std::string_view get_file_content() const noexcept {
		return { fileloader.begin(), fileloader.end() };
	}
};
inline std::vector<file_t> opened_files;
inline std::string_view open_file(char const* filename) {
	for (auto const& f : opened_files) {
		if (std::strcmp(filename, f.filename) == 0)
			return f.get_file_content();
	}
	opened_files.emplace_back(filename);
	return opened_files.back().get_file_content();
}

}