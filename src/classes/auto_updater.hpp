#pragma once

namespace fs = std::filesystem;

namespace updater {
	const std::string file_path = "offsets.json";

#ifndef _UC
	bool check_and_update(bool prompt_update);
	bool get_last_commit_date(json& commit);
	bool download_file(const char* url, const char* localPath);
#endif
	bool file_good(const std::string& name);

	extern bool read();
	extern void save();

	const inline std::string github_repo_api = "https://api.github.com/repos/IMXNOOBX/cs2-external-esp/commits";
	const inline std::string raw_updated_offsets = "https://github.com/IMXNOOBX/cs2-external-esp/raw/main/offsets/offsets.json";

	inline int build_number = 0;
}
