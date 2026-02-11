/**
  * Touhou Community Reliant Automatic Patcher
  * Loader
  *
  * ----
  *
  * Parse patch setup and launch game
  */

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#include <thcrap.h>
#include <string>
#include <vector>
#include <filesystem>
#include <thcrap_update_wrapper.h>

const char EXE_HELP[] =
	"可执行文件参数既可以是游戏 ID（会在 games.js 中查找），也可以是 .exe 的相对或绝对路径。\n"
	"你可以在命令行里直接传入，也可以在运行配置里用 \"game\"（游戏 ID）或 \"exe\"（可执行文件路径）指定。\n"
	"如果两者都提供，则优先使用 \"exe\"。";

const char *game_missing = NULL;
size_t current_dir_len = 0;

bool update_finalize(std::vector<std::string>& logs);

const char* game_lookup(const json_t *games_js, const char *game, const char *base_dir)
{
	const json_t *game_path = json_object_get(games_js, game);
	if (!json_string_length(game_path)) {
		game_missing = game;
		return nullptr;
	}
	const char *game_path_str = json_string_value(game_path);
	if (PathIsRelativeA(game_path_str)) {
		char* ret = (char*)malloc(strlen(base_dir) + strlen(game_path_str) + 1);
		strcpy(ret, base_dir);
		PathAppendA(ret, game_path_str);
		return ret;
	}
	return game_path_str;
}

json_t *load_config_from_file(const char *rel_start, json_t *run_cfg, const char *config_path)
{
	const char* run_cfg_fn;

	if (PathIsRelativeU(config_path)) {
		if (strchr(config_path, '\\')) {
			run_cfg_fn = alloc_str(rel_start, config_path);
		}
		else {
			run_cfg_fn = alloc_str<char>(std::filesystem::current_path().u8string(), "\\config\\", config_path);
		}
	}
	else {
		run_cfg_fn = strdup(config_path);
	}

	log_printf("正在加载运行配置 %s... ", run_cfg_fn);
	json_t *new_run_cfg = json_load_file_report(run_cfg_fn);
	log_print(new_run_cfg != nullptr ? "已找到\n" : "未找到\n");

	if (!run_cfg) {
		run_cfg = new_run_cfg;
	}
	else if (new_run_cfg) {
		run_cfg = json_object_merge(run_cfg, new_run_cfg);
	}
	json_array_append_new(json_object_get_create(run_cfg, "runcfg_fn", JSON_ARRAY), json_string(run_cfg_fn));
	free((void*)run_cfg_fn);
	return run_cfg;
}

json_t *load_config_from_string(json_t *run_cfg, const char *config_string)
{
	log_print("正在从命令行加载运行配置... ");
	json_t *new_run_cfg = json5_loadb(config_string, strlen(config_string), nullptr);
	log_print(new_run_cfg != nullptr ? "成功\n" : "失败\n");

	if (!run_cfg) {
		run_cfg = new_run_cfg;
	}
	else if (new_run_cfg) {
		run_cfg = json_object_merge(run_cfg, new_run_cfg);
	}
	json_array_append_new(json_object_get_create(run_cfg, "runcfg_fn", JSON_ARRAY), json_string(
		("stdin:"s + config_string).c_str()
	));
	return run_cfg;
}

char *find_exe_from_cfg(const char *rel_start, json_t *run_cfg, json_t *games_js)
{
	const char *new_exe_fn = NULL;
	char *cfg_exe_fn = NULL;

	new_exe_fn = json_object_get_string(run_cfg, "exe");
	if (!new_exe_fn) {
		const char *game = json_object_get_string(run_cfg, "game");
		if (game) {
			new_exe_fn = game_lookup(games_js, game, (const char*)std::filesystem::current_path().u8string().c_str());
		}
	}
	if (new_exe_fn) {
		if (PathIsRelativeU(new_exe_fn)) {
			cfg_exe_fn = (char*)malloc(current_dir_len + strlen(new_exe_fn) + 2);
			strcpy(cfg_exe_fn, rel_start);
			PathAppendU(cfg_exe_fn, new_exe_fn);
		}
		else {
			cfg_exe_fn = strdup(new_exe_fn);
		}
	}

	return cfg_exe_fn;
}

#include <win32_utf8/entry_winmain.c>

int TH_CDECL win32_utf8_main(int argc, const char *argv[])
{
	DWORD rel_start_len = GetCurrentDirectoryU(0, NULL);
	VLA(char, rel_start, (size_t)rel_start_len + 1);
	GetCurrentDirectoryU(rel_start_len, rel_start);
	PathAddBackslashU(rel_start);

	char current_dir[MAX_PATH];
	GetModuleFileNameU(NULL, current_dir, MAX_PATH);
	PathRemoveFileSpecU(current_dir);
	PathAppendU(current_dir, "..");
	PathAddBackslashU(current_dir);
	SetCurrentDirectoryU(current_dir);

	size_t current_dir_len = strlen(current_dir);

	int ret;
	json_t *games_js = NULL;

	std::string run_cfg_fn;
	json_t *run_cfg = NULL;

	const char *cmd_exe_fn = NULL;
	char *cfg_exe_fn = NULL;
	const char *final_exe_fn = NULL;
	size_t run_cfg_fn_len = 0;

	char* cmdline = NULL;
	

	// If thcrap just updated itself, finalize the update by moving things around if needed.
	// This can be done before parsing the command line.
	// We can't call log_init before this because we might move some log files, but we still want to log things,
	// so we'll buffer the logs somewhere and display them a bit later.
	std::vector<std::string> update_finalize_logs;
	bool update_finalize_ret = update_finalize(update_finalize_logs);

	globalconfig_init();
	log_init(globalconfig_get_boolean("console", false));

	for (auto& it : update_finalize_logs) {
		log_printf("%s\n", it.c_str());
	}
	if (!update_finalize_ret) {
		ret = 1;
		goto ret;
	}

	if(argc < 2) {
		log_mboxf(NULL, MB_OK | MB_ICONINFORMATION,
			"这是 %s 的命令行加载器组件。\n"
			"\n"
			"你可能要找的是配置工具，用它可以创建快捷方式来简化使用：thcrap_configure。\n"
			"\n"
			"如果你确实要用命令行，请按下面格式运行：\n"
			"\n"
			"\tthcrap_loader runconfig.js executable\n"
			"\n"
			"- 运行配置文件必须以 .js 结尾才会被识别。\n"
			"- %s\n"
			"- 当提供多个命令行参数时，后出现的参数优先级更高。\n",
			PROJECT_NAME, EXE_HELP
		);
		ret = -1;
		goto end;
	}

	// Load games.js
	{
		DWORD games_js_dir_len = GetCurrentDirectoryU(0, NULL);
		size_t games_js_fn_len = (size_t)games_js_dir_len + 1 + strlen("config\\games.js") + 1;
		VLA(char, games_js_fn, games_js_fn_len);

		GetCurrentDirectoryU(games_js_dir_len, games_js_fn);
		PathAddBackslashA(games_js_fn);
		strcat(games_js_fn, "config\\games.js");
		games_js = json_load_file_report(games_js_fn);
		VLA_FREE(games_js_fn);
	}

	// Parse command line
	log_print("正在解析命令行...\n");
	for(int i = 1; i < argc; i++) {
		const char *arg = argv[i];
		const char *param_ext = PathFindExtensionA(arg);

		if (!stricmp(param_ext, ".js")) {
			run_cfg = load_config_from_file(rel_start, run_cfg, arg);
		}
		else if (arg[0] == '{') {
			run_cfg = load_config_from_string(run_cfg, arg);
		}
		else if(!stricmp(param_ext, ".exe")) {
			cmd_exe_fn = arg;
		}
		else {
			// Need to set game_missing even if games_js is null.
			cmd_exe_fn = game_lookup(games_js, arg, current_dir);
		}
	}
	cfg_exe_fn = find_exe_from_cfg(rel_start, run_cfg, games_js);

	if(!run_cfg) {
		if(run_cfg_fn.empty()) {
			log_mbox(NULL, MB_OK | MB_ICONEXCLAMATION,
				"未提供运行配置 .js 文件。\n"
				"将继续使用空配置。\n"
				"\n"
				"如果你还没有配置文件，请先使用 "
				"thcrap_configure 创建。\n"
			);
			run_cfg = json_object();
		} else {
			log_mboxf(NULL, MB_OK | MB_ICONEXCLAMATION,
				"未找到运行配置文件 \"%s\"。\n",
				run_cfg_fn.c_str()
			);

			ret = -4;
			goto end;
		}
	}
	// Command-line arguments take precedence over run configuration values
	final_exe_fn = cmd_exe_fn ? cmd_exe_fn : cfg_exe_fn;
	log_printf("使用可执行文件: %s\n", final_exe_fn);

	// Still none?
	if(!final_exe_fn) {
		if(game_missing || !games_js) {
			log_mboxf(NULL, MB_OK | MB_ICONEXCLAMATION,
				"games.js 中缺少游戏 ID \"%s\"。",
				game_missing
			);
		} else {
			log_mboxf(NULL, MB_OK | MB_ICONEXCLAMATION,
				"未提供目标可执行文件。\n\n%s", EXE_HELP
			);
		}
		ret = -3;
		goto end;
	}

	runconfig_load(run_cfg, RUNCONFIG_NO_BINHACKS);

	if (const char *temp_cmdline = runconfig_cmdline_get()) {
		cmdline = strdup(temp_cmdline);
	}

	log_print("命令行解析完成\n");
	ret = loader_update_with_UI_wrapper(final_exe_fn, cmdline);
end:
	SAFE_FREE(cmdline);
	json_decref(games_js);
	json_decref(run_cfg);
	runconfig_free();
	globalconfig_release();
ret:
	VLA_FREE(rel_start);
	return ret;
}
