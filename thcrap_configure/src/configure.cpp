/**
  * Touhou Community Reliant Automatic Patcher
  * Cheap command-line patch stack configuration tool
  */

#include <thcrap.h>
#include <thcrap/src/thcrap_update_wrapper.h>
#include "configure.h"
#include "repo.h"
#include "console.h"

int file_write_error(const char *fn)
{
	static int error_nag = 0;
	log_printf(
		"\n"
		"写入 %s 失败！\n"
		"你可能没有当前目录的写入权限，\n"
		"或者该文件本身是只读的。\n",
		fn
	);
	if (!error_nag) {
		log_print("后续文件写入也很可能会继续失败。\n");
		error_nag = 1;
	}
	return console_ask_yn("仍然继续配置吗？") == 'y';
}

int file_write_text(const char *fn, const char *str)
{
	int ret;
	FILE *file = fopen_u(fn, "wt");
	if (!file) {
		return -1;
	}
	ret = fputs(str, file);
	fclose(file);
	return ret;
}

void self_restart()
{
	// Re-run an up-to-date configure process
	LPSTR commandLine = GetCommandLine();
	log_printf("发现更新！正在重新启动 %s\n", commandLine);

	STARTUPINFOA sa;
	PROCESS_INFORMATION pi;
	memset(&sa, 0, sizeof(sa));
	memset(&pi, 0, sizeof(pi));
	sa.cb = sizeof(sa);
	CreateProcessU(nullptr, commandLine, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &sa, &pi);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
}

std::string run_cfg_fn_build(const patch_sel_stack_t& sel_stack)
{
	bool skip = false;
	std::string ret;

	// If we have any translation patch, skip everything below that
	for (const patch_desc_t& sel : sel_stack) {
		if (strnicmp(sel.patch_id, "lang_", 5) == 0) {
			skip = true;
			break;
		}
	}

	for (const patch_desc_t& sel : sel_stack) {
		std::string patch_id;

		if (strnicmp(sel.patch_id, "lang_", 5) == 0) {
			patch_id = sel.patch_id + 5;
			skip = false;;
		}
		else {
			patch_id = sel.patch_id;
		}

		if (!skip) {
			if (!ret.empty()) {
				ret += "-";
			}
			ret += patch_id;
		}
	}

	return ret;
}

std::string EnterRunCfgFN(std::string& run_cfg_fn)
{
	int ret = 0;
	do {
		log_printf(
			"\n"
			"请输入此配置的自定义名称，留空则使用默认值\n"
			"（%s）：\n", run_cfg_fn.c_str()
		);

		std::wstring run_cfg_fn_new = console_read();
		if (run_cfg_fn_new[0])
			run_cfg_fn = to_utf8(run_cfg_fn_new);

		std::string run_cfg_fn_js = run_cfg_fn + ".js";
		if (PathFileExists(run_cfg_fn_js.c_str())) {
			log_printf("\"%s\" 已存在。", run_cfg_fn_js.c_str());
			ret = console_ask_yn("要覆盖吗？") == 'n';
		} else {
			ret = 0;
		}
	} while (ret);
	return run_cfg_fn;
}

struct progress_state_t
{
    // This callback can be called from a bunch of threads
    std::mutex mutex;
    std::map<std::string, std::chrono::steady_clock::time_point> files;
};

bool progress_callback(progress_callback_status_t *status, void *param)
{
    using namespace std::literals;
    progress_state_t *state = (progress_state_t*)param;
    std::scoped_lock lock(state->mutex);

    switch (status->status) {
        case GET_DOWNLOADING: {
            // Using the URL instead of the filename is important, because we may
            // be downloading the same file from 2 different URLs, and the UI
            // could quickly become very confusing, with progress going backwards etc.
            auto& file_time = state->files[status->url];
            auto now = std::chrono::steady_clock::now();
            if (file_time.time_since_epoch() == 0ms) {
                file_time = now;
            }
            else if (now - file_time > 5s) {
                log_printf("[%u/%u] %s：下载中（%ub/%ub）...\n", status->nb_files_downloaded, status->nb_files_total,
                           status->url, status->file_progress, status->file_size);
                file_time = now;
            }
            return true;
        }

        case GET_OK:
            log_printf("[%u/%u] %s/%s: OK (%ub)\n", status->nb_files_downloaded, status->nb_files_total, status->patch->id, status->fn, status->file_size);
            return true;

        case GET_CLIENT_ERROR:
		case GET_SERVER_ERROR:
		case GET_SYSTEM_ERROR:
			log_printf("%s: %s\n", status->url, status->error);
            return true;
        case GET_CRC32_ERROR:
            log_printf("%s：CRC32 校验错误\n", status->url);
            return true;
        case GET_CANCELLED:
            // Another copy of the file have been downloader earlier. Ignore.
            return true;
        default:
            log_printf("%s：未知状态\n", status->url);
            return true;
    }
}


char **games_json_to_array(json_t *games)
{
    char **array;
    const char *key;

    array = strings_array_create();
    json_object_foreach_key(games, key) {
        array = strings_array_add(array, key);
    }
    return array;
}

extern "C" {
	extern void strings_mod_init(void);
}

#include <win32_utf8/entry_winmain.c>
#include "thcrap_i18n/src/thcrap_i18n.h"
int TH_CDECL win32_utf8_main(int argc, const char *argv[])
{
	VLA(char, current_dir, MAX_PATH);
	GetModuleFileNameU(NULL, current_dir, MAX_PATH);
	PathRemoveFileSpecU(current_dir);
	PathAppendA(current_dir, "..");
	SetCurrentDirectoryU(current_dir);
	VLA_FREE(current_dir);
	int ret = 0;
	i18n_lang_init(THCRAP_I18N_APPDOMAIN);
	// Global URL cache to not download anything twice
	json_t *url_cache = json_object();
	// Repository ID cache to prioritize the most local repository if more
	// than one repository with the same name is discovered in the network
	json_t *id_cache = json_object();

	repo_t **repo_list = nullptr;

	const char *start_repo = nullptr;

	patch_sel_stack_t sel_stack;
	json_t *new_cfg = json_pack("{s[]}", "patches");

	DWORD cur_dir_len = GetCurrentDirectoryU(0, NULL);
	VLA(char, cur_dir, (size_t)cur_dir_len + 1);
	json_t *games = NULL;
	games_js_entry *gamesArray = nullptr;

	std::string run_cfg_fn;
	std::string run_cfg_fn_js;
	char *run_cfg_str = NULL;

	progress_state_t state;

	strings_mod_init();

	globalconfig_init();
	exception_load_config();

	log_async = false;
	log_init(globalconfig_get_boolean("console", false));
	console_init();

	GetCurrentDirectoryU(cur_dir_len, cur_dir);
	PathAddBackslashA(cur_dir);
	str_slash_normalize(cur_dir);

	if (argc > 1) {
		start_repo = argv[1];
	}

	console_prepare_prompt();
	log_print(_A(
		"==========================================\n"
		"thcrap - 补丁配置工具\n"
		"==========================================\n"
		"\n"
		"\n"
		"本工具将为 thcrap 创建新的补丁配置。\n"
		"\n"
		"\n"
	));
	if (thcrap_update_module()) {
		log_print(_A(
			"配置流程共四步：\n"
			"\n"
			"\t\t1. 选择补丁\n"
			"\t\t2. 下载与游戏无关的基础数据\n"
			"\t\t3. 定位游戏安装目录\n"
			"\t\t4. 下载与游戏相关的数据\n"
			"\n"
			"\n"
			"\n"
			"你可以在命令行参数中指定仓库发现 URL。\n"
			"此外，当前目录子目录中已发现仓库的补丁也会加入可选列表。\n")
		);
	}
	else {
		log_print(_A(
			"当前配置流程共两步：\n"
			"\n"
			"\t\t1. 选择补丁\n"
			"\t\t2. 定位游戏安装目录\n"
			"\n"
			"\n"
			"\n"
			"更新功能已禁用，因此只能从本地可用仓库中选择补丁。\n")
		);
	}
	log_print(
		"\n"
		"\n"
	);
	pause();

	if (update_notify_thcrap_wrapper() == SELF_OK) {
		self_restart();
		goto end;
	}

	CreateDirectoryU("repos", NULL);
	repo_list = RepoDiscover_wrapper(start_repo);
	if (!repo_list || !repo_list[0]) {
		log_print(_A("没有可用的补丁仓库。\n"));
		pause();
		goto end;
	}
	sel_stack = SelectPatchStack(repo_list);
	if (!sel_stack.empty()) {
		log_print(_A("正在下载基础数据（与游戏无关）...\n"));
		stack_update_wrapper(update_filter_global_wrapper, NULL, progress_callback, &state);
		state.files.clear();

		/// Build the new run configuration
		json_t *new_cfg_patches = json_object_get(new_cfg, "patches");
		for (patch_desc_t& sel : sel_stack) {
			patch_t patch = patch_build(&sel);
			json_array_append_new(new_cfg_patches, patch_to_runconfig_json(&patch));
			patch_free(&patch);
		}
	}


	// Other default settings
	json_object_set_new(new_cfg, "dat_dump", json_false());
	json_object_set_new(new_cfg, "patched_files_dump", json_false());
	json_object_set_new(new_cfg, "developper_mode", json_false());

	run_cfg_fn = run_cfg_fn_build(sel_stack);
	run_cfg_fn = EnterRunCfgFN(run_cfg_fn);
	run_cfg_fn_js = std::string("config/") + run_cfg_fn + ".js";


	CreateDirectoryU("config", NULL);
	run_cfg_str = json_dumps(new_cfg, JSON_INDENT(2) | JSON_SORT_KEYS);
	if (!file_write_text(run_cfg_fn_js.c_str(), run_cfg_str)) {
		log_printf(_A("\n\n已将以下运行配置写入 %s：\n"), run_cfg_fn_js.c_str());
		log_print(run_cfg_str);
		log_print("\n\n");
		pause();
	}
	else if (!file_write_error(run_cfg_fn_js.c_str())) {
		goto end;
	}

	// Step 2: Locate games
	games = ConfigureLocateGames(cur_dir);
	if (json_object_size(games) <= 0) {
		goto end;
	}

	if (console_ask_yn(_A("要创建快捷方式吗？（首次运行必需）")) != 'n') {
		gamesArray = games_js_to_array(games);
		if (CreateShortcuts(run_cfg_fn.c_str(), gamesArray, SHDESTINATION_THCRAP_DIR, SHTYPE_AUTO) != 0) {
			goto end;
		}
	}

	{
		char **filter = games_json_to_array(games);
		log_print(_A("\n正在下载已定位游戏的专用数据...\n"));
		stack_update_wrapper(update_filter_games_wrapper, filter, progress_callback, &state);
		state.files.clear();
		strings_array_free(filter);
	}
	log_printf(_A(
		"\n"
		"\n"
		"完成。\n"
		"\n"
		"你现在可以通过当前目录中创建的快捷方式，\n"
		"使用所选配置启动对应游戏：\n"
		"(%s)\n"
		"\n"
		"这些快捷方式可在任意位置使用，你可以按需移动。\n"
		"如果快捷方式太多，也可以试试这个工具：\n"
		"https://github.com/thpatch/Universal-THCRAP-Launcher/releases\n"
		"\n"), cur_dir
	);
	con_can_close = true;
	pause();

end:
	con_can_close = true;
	delete[] gamesArray;
	SAFE_FREE(run_cfg_str);
	json_decref(new_cfg);
	json_decref(games);

	VLA_FREE(cur_dir);
	stack_free();
    for (auto& sel : sel_stack) {
        free(sel.repo_id);
        free(sel.patch_id);
    }
	for (size_t i = 0; repo_list[i]; i++) {
		RepoFree(repo_list[i]);
	}
	free(repo_list);
	json_decref(url_cache);
	json_decref(id_cache);

	globalconfig_release();
	runconfig_free();
	
	thcrap_update_exit_wrapper();
	con_end();
	return 0;
}
