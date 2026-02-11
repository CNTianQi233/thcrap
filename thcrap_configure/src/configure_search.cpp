/**
  * Touhou Community Reliant Automatic Patcher
  * Cheap command-line patch stack configuration tool
  *
  * ----
  *
  * Game searching front-end code.
  */

#include <thcrap.h>
#include <algorithm>
#include <stdexcept>
#include "configure.h"

static const char games_js_fn[] = "config/games.js";

static const game_search_result* ChooseLocation(game_search_result *locs, size_t& pos_begin)
{
	game_search_result *ret = nullptr;

	std::string id = locs[pos_begin].id;
	size_t pos_end = pos_begin + 1;
	while (locs[pos_end].id && id == locs[pos_end].id)
		pos_end++;

	size_t num_versions = pos_end - pos_begin;
	if(num_versions == 1) {
		log_printf("已找到 %s（%s）：%s\n", locs[pos_begin].id, locs[pos_begin].description, locs[pos_begin].path);

		ret = &locs[pos_begin];
	} else if(num_versions > 1) {
		size_t loc_num;

		log_printf("找到 %zu 个 %s 的版本：\n\n", num_versions, id.c_str());

		for (size_t i = 0; pos_begin + i < pos_end; i++) {
			con_clickable(std::to_wstring(i + 1),
				to_utf16(stringf(" [%2zu] %s: %s", i + 1, locs[pos_begin + i].path, locs[pos_begin + i].description)));
		}
		putchar('\n');
		do {
			con_printf("请选择要应用补丁的版本（1 - %zu）：\n", num_versions);

			if (swscanf(console_read().c_str(), L"%zu", &loc_num) != 1)
				loc_num = 0;
		} while (loc_num < 1 || loc_num > num_versions);

		ret = &locs[pos_begin + loc_num - 1];
	}

	pos_begin = pos_end;
	return ret;
}

// Work around a bug in Windows 7 and later by sending
// BFFM_SETSELECTION a second time.
// https://connect.microsoft.com/VisualStudio/feedback/details/518103/bffm-setselection-does-not-work-with-shbrowseforfolder-on-windows-7
typedef struct {
	LPITEMIDLIST path;
	int attempts;
} initial_path_t;

int CALLBACK SetInitialBrowsePathProc(HWND hWnd, UINT uMsg, LPARAM lp, LPARAM pData)
{
	initial_path_t *ip = (initial_path_t *)pData;
	if(ip) {
		switch(uMsg) {
		case BFFM_INITIALIZED:
			ip->attempts = 0;
			// fallthrough
		case BFFM_SELCHANGED:
			if(ip->attempts < 2) {
				SendMessageW(hWnd, BFFM_SETSELECTION, FALSE, (LPARAM)ip->path);
				ip->attempts++;
			}
		}
	}
	return 0;
}

template<typename T>
struct ComRAII {
	T *p;

	operator bool() const { return !!p; }
	operator T*() const { return p; }
	T **operator&() { return &p; }
	T *operator->() const { return p; }
	T &operator*() const { return *p; }
	
	ComRAII() :p(NULL) {}
	explicit ComRAII(T *p) :p(p) {}
	ComRAII(const ComRAII<T> &other) = delete;
	ComRAII<T> &operator=(const ComRAII<T> &other) = delete;
	~ComRAII() { if (p) { p->Release(); p = NULL; } }
};

static int SelectFolderVista(HWND owner, PIDLIST_ABSOLUTE initial_path, PIDLIST_ABSOLUTE& pidl, const wchar_t* window_title) {
	// Those two functions are absent in XP, so we have to load them dynamically
	HMODULE shell32 = GetModuleHandleW(L"Shell32.dll");
	auto pSHCreateItemFromIDList = (HRESULT(WINAPI *)(PCIDLIST_ABSOLUTE, REFIID, void**))GetProcAddress(shell32, "SHCreateItemFromIDList");
	auto pSHGetIDListFromObject = (HRESULT(WINAPI *)(IUnknown*, PIDLIST_ABSOLUTE*))GetProcAddress(shell32, "SHGetIDListFromObject");
	if (!pSHCreateItemFromIDList || !pSHGetIDListFromObject)
		return -1;

	ComRAII<IFileDialog> pfd;
	CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
	if (!pfd)
		return -1;

	{
		ComRAII<IShellItem> psi;
		pSHCreateItemFromIDList(initial_path, IID_PPV_ARGS(&psi));
		if (!psi)
			return -1;
		pfd->SetDefaultFolder(psi);
	}

	pfd->SetOptions(
		FOS_NOCHANGEDIR | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM
		| FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST | FOS_DONTADDTORECENT);
	pfd->SetTitle(window_title);
	HRESULT hr = pfd->Show(owner);
	ComRAII<IShellItem> psi;
	if (SUCCEEDED(hr) && SUCCEEDED(pfd->GetResult(&psi))) {
		pSHGetIDListFromObject(psi, &pidl);
		return 0;
	}

	if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
		return 0;
	return -1;
}

static int SelectFolderXP(HWND owner, PIDLIST_ABSOLUTE initial_path, PIDLIST_ABSOLUTE& pidl, const wchar_t* window_title) {
	BROWSEINFOW bi = { 0 };
	initial_path_t ip = { 0 };
	ip.path = initial_path;

	bi.lpszTitle = window_title;
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NONEWFOLDERBUTTON | BIF_USENEWUI;
	bi.hwndOwner = owner;
	bi.lpfn = SetInitialBrowsePathProc;
	bi.lParam = (LPARAM)&ip;
	pidl = SHBrowseForFolderW(&bi);
	return 0;
}
PIDLIST_ABSOLUTE SelectFolder(HWND owner, PIDLIST_ABSOLUTE initial_path, const wchar_t* window_title) {
	PIDLIST_ABSOLUTE pidl = NULL;
	if (-1 == SelectFolderVista(owner, initial_path, pidl, window_title)) {
		SelectFolderXP(owner, initial_path, pidl, window_title);
	}
	return pidl;
}

json_t *sort_json(json_t *in)
{
	std::vector<const char*> keys;

	const char *key;
	json_object_foreach_key(in, key) {
		keys.push_back(key);
	}

	std::sort(keys.begin(), keys.end(), [](const char *s1, const char *s2) {
		WCHAR_T_DEC(s1);
		WCHAR_T_DEC(s2);
		WCHAR_T_CONV(s1);
		WCHAR_T_CONV(s2);

		int sort_result = CompareStringW(LOCALE_INVARIANT, 0,
			s1_w, -1, s2_w, -1);

		WCHAR_T_FREE(s1);
		WCHAR_T_FREE(s2);

		if (sort_result == 0) {
			throw std::runtime_error("CompareStringsEx failed");
		}
		return sort_result == CSTR_LESS_THAN;
	});

	json_t *out = json_object();
	for (const char *key : keys) {
		json_object_set(out, key, json_object_get(in, key));
	}
	json_decref(in);
	return out;
}

games_js_entry *games_js_to_array(json_t *games_js)
{
	games_js_entry *games_array = new games_js_entry[json_object_size(games_js) + 1];
	const char *key;
	json_t *value;
	size_t i = 0;
	json_object_foreach_fast(games_js, key, value) {
		games_array[i].id = key;
		games_array[i].path = json_string_value(value);
		i++;
	}
	games_array[i].id = nullptr;
	games_array[i].path = nullptr;
	return games_array;
}

json_t* ConfigureLocateGames(const char *games_js_path)
{
	json_t *games;

	cls(0);

	log_print(
		"--------------\n"
		"定位游戏\n"
		"--------------\n"
		"\n"
		"\n"
	);

	games = json_load_file_report("config/games.js");
	if(json_object_size(games) != 0) {
		log_printf("你已有 %s，内容如下：\n\n", games_js_fn);
		json_dump_log(games, JSON_INDENT(2) | JSON_SORT_KEYS);
		log_print(
			"\n"
			"\n"
			"将为列表中的所有游戏下载或更新补丁数据。\n"
			"\n"
		);
		con_clickable(L"a", L"\t* （A）保留现有条目并新增游戏？");
		con_clickable(L"r", L"\t* 清空列表并（R）重新扫描？");
		con_clickable(L"k", L"\t* （K）保持当前列表并继续？");
		log_print("\n");
		char ret = Ask<3>(nullptr, { 'a', 'r', 'k' | DEFAULT_ANSWER });
		if(ret == 'k') {
			return games;
		} else if(ret == 'r') {
			json_object_clear(games);
		}
	} else {
		games = json_object();
		log_printf(
			"你还没有 %s。\n"
			"\n"
			"接下来需要搜索系统中可打补丁的游戏。\n"
			"通常只需执行一次，除非你之后移动了游戏目录。\n"
			"\n"
			"根据磁盘数量和目录结构，这一步可能耗时较长。\n"
			"你可以提供一个游戏共用的根目录来加速搜索。\n"
			"\n"
			"例如，如果你把《东方辉针城》放在：\n"
			"\n"
				"\tC:\\Games\\Touhou\\DDC\\\n"
			"\n"
			"《东方永夜抄》放在：\n"
			"\n"
				"\tC:\\Games\\Touhou\\IN\\\n"
			"\n"
			"那么这里应填写：\n"
			"\n"
				"\tC:\\Games\\Touhou\\\n"
			"\n"
			"如果你的东方游戏分散在不同磁盘，就只能执行全盘搜索。\n"
			"\n"
			"另外，预先打过静态英文补丁的游戏可能会出现兼容问题，\n"
			"建议尽量使用原版游戏。\n"
			"\n"
			"\n",
			games_js_fn
		);
		pause();
	}

	PIDLIST_ABSOLUTE initial_path = NULL;
	// BFFM_SETSELECTION does this anyway if we pass a string, so we
	// might as well do the slash conversion in win32_utf8's wrapper
	// while we're at it.
	SHParseDisplayNameU(games_js_path, NULL, &initial_path, 0, NULL);
	CoInitialize(NULL);
	bool repeat;
	do {
		wchar_t search_path_w[MAX_PATH] = {0};
		game_search_result *found = nullptr;

		PIDLIST_ABSOLUTE pidl = SelectFolder(con_hwnd(), initial_path, L"请选择游戏搜索根目录（取消则全盘搜索）：");
		if (pidl && SHGetPathFromIDListW(pidl, search_path_w)) {
			PathAddBackslashW(search_path_w);
			CoTaskMemFree(pidl);
		}

		std::string search_path = to_utf8(search_path_w);
		repeat = false;
		log_printf(
			search_path[0]
				? "正在 %s 中搜索游戏，可能需要一些时间...\n\n"
				: "正在全盘搜索游戏，可能需要一些时间...\n\n",
			search_path.c_str()
		);
		console_print_percent(-1);

		games_js_entry *games_array = games_js_to_array(games);
		LPCWSTR search_array[] = {
			search_path_w,
			nullptr,
		};
		found = SearchForGames(search_array, games_array);
		delete[] games_array;
		
		if(found && found[0].id) {
			char *games_js_str = NULL;

			for (size_t i = 0; found[i].id; ) {
				const game_search_result *loc = ChooseLocation(found, i);
				json_object_set_new(games, loc->id, json_string(loc->path));
				putchar('\n');
			}

			SetCurrentDirectory(games_js_path);

			games = sort_json(games);
			games_js_str = json_dumps(games, JSON_INDENT(2));
			if(!file_write_text(games_js_fn, games_js_str)) {
				log_printf(
					"已识别到以下游戏路径，并写入 %s：\n"
					"%s\n"
					, games_js_fn
					, games_js_str
				);
			} else if(!file_write_error(games_js_fn)) {
				games = json_decref_safe(games);
			}
			SAFE_FREE(games_js_str);
		} else if(json_object_size(games) > 0) {
			log_printf("未发现新的游戏路径。\n");
		} else {
			log_printf("未找到任何游戏路径。\n");
			if(search_path_w[0]) {
				repeat = console_ask_yn("要换一个目录继续搜索吗？") == 'y';
			}
			if(!repeat) {
				log_printf(
					"将不会创建补丁快捷方式。\n"
					"请在获得受支持的游戏后重新运行本配置工具。\n"
				);
				pause();
			}
		}
		SearchForGames_free(found);
	} while(repeat);
	CoUninitialize();
	CoTaskMemFree(initial_path);
	return games;
}
