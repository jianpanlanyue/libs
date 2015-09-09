//一个跟系统有关的常用功能api的封装。

#pragma once

#if defined(_MSC_VER) && _MSC_VER < 1800
	#error "Compiler need to support c++11, please use vs2013 or above, vs2015 e.g."
#endif

#include "headers_dependency.h"

namespace based
{
	//操作系统相关api的封装
	class os
	{
	#ifdef WIN32
	public:
		
		//得到exe全路径，比如d:\aa\bb\cc.exe
		template<typename char_set_t>
		static typename std::enable_if<std::is_same<char_set_t, char>::value, std::string>::type get_exe_path()
		{
			char* exe_full_path = new char[MAX_PATH];
			char* exe_full_path_fix = new char[MAX_PATH];

			GetModuleFileNameA(NULL, exe_full_path, MAX_PATH);
			PathCanonicalizeA(exe_full_path_fix, exe_full_path);
			std::string str_exe_path(exe_full_path_fix);

			delete[] exe_full_path;
			delete[] exe_full_path_fix;
			return str_exe_path;
		}

		template<typename char_set_t>
		static typename std::enable_if<std::is_same<char_set_t, wchar_t>::value, std::wstring>::type get_exe_path()
		{
			wchar_t* exe_full_path = new wchar_t[MAX_PATH];
			wchar_t* exe_full_path_fix = new wchar_t[MAX_PATH];

			GetModuleFileNameW(NULL, exe_full_path, MAX_PATH);
			PathCanonicalizeW(exe_full_path_fix, exe_full_path);
			std::wstring str_exe_path(exe_full_path);

			delete[] exe_full_path;
			delete[] exe_full_path_fix;

			return str_exe_path;
		}

		//得到不带'\'的exe路径，比如，由d:\aa\bb\cc.exe，得到d:\aa\bb
		template<typename char_set_t>
		static typename std::enable_if<std::is_same<char_set_t, char>::value, std::string>::type get_exe_path_without_backslash()
		{
			char* exe_full_path = new char[MAX_PATH];
			char* exe_full_path_fix = new char[MAX_PATH];

			GetModuleFileNameA(NULL, exe_full_path, MAX_PATH);
			PathCanonicalizeA(exe_full_path_fix, exe_full_path);
			std::string str_exe_path(exe_full_path_fix);

			delete[] exe_full_path;
			delete[] exe_full_path_fix;

			return str_exe_path.erase(str_exe_path.rfind(('\\')));
		}

		template<typename char_set_t>
		static typename std::enable_if<std::is_same<char_set_t, wchar_t>::value, std::wstring>::type get_exe_path_without_backslash()
		{
			wchar_t* exe_full_path = new wchar_t[MAX_PATH];
			wchar_t* exe_full_path_fix = new wchar_t[MAX_PATH];

			GetModuleFileNameW(NULL, exe_full_path, MAX_PATH);
			PathCanonicalizeW(exe_full_path_fix, exe_full_path);
			std::wstring str_exe_path(exe_full_path);

			delete[] exe_full_path;
			delete[] exe_full_path_fix;
			return str_exe_path.erase(str_exe_path.rfind((L'\\')));
		}

		template<typename char_set_t>
		static bool make_dir_recursive(const char_set_t* path)
		{
			int str_len;
			if (path == NULL || (str_len=strlen(path)) == 0)
				return false;

			std::string str_path(path);
			std::replace(str_path.begin(), str_path.end(), '/', '\\');

			if (str_path.at(str_path.length()-1) != '\\')
				str_path += '\\';

			str_len = str_path.length();
			for (int i = 0; i < str_len; i++)
			{
				if (str_path.at(i) == '\\')
				{
					std::string& sub_str = str_path.substr(0, i);
					if(!PathFileExistsA(sub_str.c_str()))
						CreateDirectoryA(sub_str.c_str(), NULL);
				}	
			}
			return true;
		}

		template<>
		static bool make_dir_recursive(const wchar_t* path)
		{
			int str_len;
			if (path == NULL || (str_len = wcslen(path)) == 0)
				return false;

			std::wstring str_path(path);
			std::replace(str_path.begin(), str_path.end(), L'/', L'\\');

			if (str_path.at(str_path.length() - 1) != L'\\')
				str_path += L'\\';

			str_len = str_path.length();
			for (int i = 0; i < str_len; i++)
			{
				if (str_path.at(i) == L'\\')
				{
					std::wstring& sub_str = str_path.substr(0, i);
					if (!PathFileExistsW(sub_str.c_str()))
						CreateDirectoryW(sub_str.c_str(), NULL);
				}
			}
			return true;
		}

	#else
		#error "Please add corresponding platform's code."
	#endif
	};
}


