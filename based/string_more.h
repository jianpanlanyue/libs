#pragma once

#include "headers_dependency.h"

namespace based
{
	class string_more
	{
	public:
		template<typename string_t>
		static string_t& replace_all(string_t& dest_str, const string_t& old_value, const string_t& new_value)
		{
			string_t::size_type pos(0);

			while (true) {
				if ((pos = dest_str.find(old_value, pos)) != string_t::npos)
				{
					dest_str.replace(pos, old_value.length(), new_value);
					pos += new_value.length();
				}
				else
					break;
			}
			return dest_str;
		}
	};

}

//example:
// int main()
// {
// 	std::wstring str = L"ab \n ";
// 	based::string_more::replace_all<std::wstring>(str, L" ", L"");
// 	std::wcout << str;
// 
// 	return 0;
// }