#pragma once

#include "headers_dependency.h"

namespace based
{
	//操作系统相关api的封装
	class os
	{
	public:
		os() = delete;

		//得到不带'\'的exe路径，比如，由d:\aa\bb\cc.exe，得到d:\aa\bb
		template<typename char_set_t>
		static typename std::enable_if<std::is_same<char_set_t, char>::value, std::string>::type get_exe_path_without_backslash()
		{
			char exe_full_path[MAX_PATH];
			char exe_full_path_fix[MAX_PATH];
			GetModuleFileNameA(NULL, exe_full_path, MAX_PATH);
			PathCanonicalizeA(exe_full_path_fix, exe_full_path);
			std::string str_exe_path(exe_full_path_fix);

			return str_exe_path.erase(str_exe_path.rfind(('\\')));
		}

		template<typename char_set_t>
		static typename std::enable_if<std::is_same<char_set_t, wchar_t>::value, std::wstring>::type get_exe_path_without_backslash()
		{
			wchar_t exe_full_path[MAX_PATH];
			wchar_t exe_full_path_fix[MAX_PATH];
			GetModuleFileNameW(NULL, exe_full_path, MAX_PATH);
			PathCanonicalizeW(exe_full_path_fix, exe_full_path);
			std::wstring str_exe_path(exe_full_path);

			return str_exe_path.erase(str_exe_path.rfind((L'\\')));
		}
	};
}


