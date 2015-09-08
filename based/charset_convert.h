#pragma once

#if defined(_MSC_VER) && _MSC_VER < 1800
#error "Compiler need to support c++11, please use vs2013 or above, vs2015 e.g."
#endif

#include "headers_dependency.h"

namespace based
{
	class charset_convert
	{
	public:

		~charset_convert()
		{
			if (char_buf_)
			{
				delete char_buf_;
				char_buf_ = nullptr;
			}
			if (wchar_buf_)
			{
				delete wchar_buf_;
				wchar_buf_ = nullptr;
			}
		}

	#ifdef WIN32
		wchar_t* chars_to_wchars(const char* chars, int chars_bytes = 0)
		{
			if (chars_bytes==0 && (chars_bytes=strlen(chars))==0)
				return L"";

			words_converted_ = MultiByteToWideChar(CP_ACP, 0, chars, chars_bytes, NULL, 0);
			if (words_converted_ == 0)
				throw std::exception("convert fail");

			words_converted_ = MultiByteToWideChar(CP_ACP, 0, chars, chars_bytes, wchar_buf_, adjust_wchar_buf(words_converted_+1));
			if (words_converted_ == 0)
				throw std::exception("convert fail");

			wchar_buf_[words_converted_++] = L'\0';
			return wchar_buf_;
		}

		char* wchars_to_chars(const wchar_t* wchars, int wchars_words = 0)
		{
			if (wchars_words==0 && (wchars_words=wcslen(wchars))==0)
				return "";

			bytes_converted_ = WideCharToMultiByte(CP_ACP, 0, wchars, wchars_words, NULL, 0, NULL, NULL);
			if(bytes_converted_ == 0)
				throw std::exception("convert fail");

			bytes_converted_ = WideCharToMultiByte(CP_ACP, 0, wchars, wchars_words, char_buf_, adjust_char_buf(bytes_converted_+1), NULL, NULL);
			if (bytes_converted_ == 0)
				throw std::exception("convert fail");

			char_buf_[bytes_converted_++] = '\0';
			return char_buf_;
		}

		wchar_t* utf8_to_wchars(const char* utf8, int utf8_bytes = 0)
		{
			if (utf8_bytes==0 && (utf8_bytes=strlen(utf8))==0)
				return L"";

			words_converted_ = ::MultiByteToWideChar(CP_UTF8, NULL, utf8, utf8_bytes, NULL, 0);
			if(words_converted_ == 0)
				throw std::exception("convert fail");

			words_converted_ = MultiByteToWideChar(CP_UTF8, NULL, utf8, utf8_bytes, wchar_buf_, adjust_wchar_buf(words_converted_+1));
			if (words_converted_ == 0)
				throw std::exception("convert fail");

			wchar_buf_[words_converted_++] = L'\0';
			return wchar_buf_;
		}

		char* wchars_to_utf8(const wchar_t* wchars, int wchars_words = 0)
		{
			if (wchars_words==0 && (wchars_words=wcslen(wchars))==0)
				return "";

			bytes_converted_ = ::WideCharToMultiByte(CP_UTF8, NULL, wchars, wchars_words, NULL, 0, NULL, NULL);
			if (bytes_converted_ == 0)
				throw std::exception("convert fail");

			bytes_converted_ = WideCharToMultiByte(CP_UTF8, NULL, wchars, wchars_words, char_buf_, adjust_char_buf(bytes_converted_+1), NULL, NULL);
			if (bytes_converted_ == 0)
				throw std::exception("convert fail");

			char_buf_[bytes_converted_++] = '\0';
			return char_buf_;
		}
	#endif

		char* chars_to_utf8(const char* chars, int chars_bytes = 0)
		{
			if (chars_bytes==0 && (chars_bytes=strlen(chars))==0)
				return "";

			wchar_t* wchars = chars_to_wchars(chars, chars_bytes);
			return wchars_to_utf8(wchars, words_converted_);
		}
		
		char* utf8_to_chars(const char* utf8, int u8_bytes = 0)
		{
			if (u8_bytes==0 && (u8_bytes=strlen(utf8))==0)
				return "";

			wchar_t* wchars = utf8_to_wchars(utf8, u8_bytes);
			return wchars_to_chars(wchars,bytes_converted_);
		}

		std::wstring string_to_wstring(const std::string& s, int s_length_bytes=0)
		{
			s_length_bytes == 0 ? s_length_bytes = s.length() : 0;
			return std::wstring(chars_to_wchars(s.c_str(), s_length_bytes));
		}

		std::string wstring_to_string(const std::wstring& ws, int ws_length_words=0)
		{
			ws_length_words == 0 ? ws_length_words = ws.length() : 0;
			return std::string(wchars_to_chars(ws.c_str(), ws_length_words));
		}

		std::wstring utf8_string_to_wstring(const std::string& utf8, int u8_length_bytes=0)
		{
			u8_length_bytes == 0 ? u8_length_bytes = utf8.length() : 0;
			return std::wstring(utf8_to_wchars(utf8.c_str(),u8_length_bytes));
		}

		std::string wstring_to_utf8_string(const std::wstring& ws, int ws_length_words=0)
		{
			ws_length_words == 0 ? ws_length_words = ws.length() : 0;
			return std::string(wchars_to_utf8(ws.c_str(),ws_length_words));
		}

		std::string utf8_string_to_string(const std::string& utf8, int u8_length_bytes=0)
		{
			u8_length_bytes == 0 ? u8_length_bytes = utf8.length() : 0;
			return std::string(utf8_to_chars(utf8.c_str(), u8_length_bytes));
		}

		std::string string_to_utf8_string(const std::string& s, int s_length_bytes=0)
		{
			s_length_bytes == 0 ? s_length_bytes = s.length() : 0;
			return std::string(chars_to_utf8(s.c_str(), s_length_bytes));
		}

		int get_last_converted_bytes()
		{
			return bytes_converted_;
		}

		int get_last_converted_words()
		{
			return words_converted_;
		}

	protected:
		//调整缓冲区长度，返回调整后的长度
		int adjust_char_buf(int new_len)
		{
			if (char_buf_len_ < new_len)
			{
				char_buf_len_ = new_len;
				delete char_buf_;
				char_buf_ = new char[char_buf_len_];
			}
			return char_buf_len_;
		}

		int adjust_wchar_buf(int new_len)
		{
			if (wchar_buf_len_ < new_len)
			{
				wchar_buf_len_ = new_len;
				delete wchar_buf_;
				wchar_buf_ = new wchar_t[wchar_buf_len_];
			}
			return wchar_buf_len_;
		}

	protected:
		char* char_buf_ = nullptr;
		int char_buf_len_ = 0;
		wchar_t* wchar_buf_ = nullptr;
		int wchar_buf_len_ = 0;
		int bytes_converted_ = 0;
		int words_converted_ = 0;
	};
}


//example:
// int main()
// {
// 	based::charset_convert conv;
// 	wchar_t* wp = conv.chars_to_wchars("我的");
// 
// 	std::wstring wstr = L"我的";
// 	std::string u8_str = conv.wstring_to_utf8_string(wstr);
// 
// 	return 0;
// }