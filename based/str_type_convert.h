//这是一个字符串与其他类型互相转换的类。
//to_type是字符串转其他类型；to_str是其他类型转字符串。

#pragma once

#if defined(_MSC_VER) && _MSC_VER < 1800
	#error "Compiler need to support c++11, please use vs2013 or above, vs2015 e.g."
#endif

#include "headers_dependency.h"

namespace based
{
	class str_type_convert
	{
	public:
		//将字符串src转换为dest对应的类型，并存储到dest中。
		template<typename T>
		bool to_type(T& dest, const char* src, unsigned long src_length=0)
		{
			if (src == NULL || (src_length == 0 && (src_length = strlen(src)) == 0))
			{
				dest = T{};
				return false;
			}
			
			return _to_type(dest, src, src_length),true;
		}
		//dest为指针类型的偏特化版本。
		template<typename T>
		bool to_type(T* dest, const char* src, unsigned long src_length=0)
		{
			if (dest == NULL)
				return false;

			if (src == NULL || (src_length == 0 && (src_length = strlen(src)) == 0))
			{
				*(char*)dest = 0;
				return false;
			}

			return _to_type(dest, src, src_length), true;
		}

		//将src转换为字符串，存储到char_array_中，并返回char_array_的地址。
		template <typename T>
		char* to_str(const T& src)
		{
			return _to_str(src);
		}

		//设置字符串性质的输入被转换后，是否追加引号（单引号或双引号）
		void set_quote_when_characters_to_str(char quote = '\0')
		{
			quote_when_characters_to_str_ = quote;
		}

	protected:
		//str_to_type的具体实现函数。特化类型以外的其他类型，编译提示失败。确实需要的，请手动增加相应类型的特化实现。
		template<typename T>
		void _to_type(T& dest, const char* src, unsigned long src_length)
		{
			static_assert(false, "no implementation for type T by default, add your own specialization version for T first.");
		}

		//_type_to_str的其他类型特化版，转换其他类型为字符串
		template<> 
		void _to_type(int& dest, const char* src, unsigned long src_length)
		{
			dest = (int)strtol(src, NULL, 10);
		}

		template<> 
		void _to_type(unsigned int& dest, const char* src, unsigned long src_length)
		{
			dest = (unsigned)strtoul(src, NULL, 10);
		}

		template<>
		void _to_type(short& dest, const char* src, unsigned long src_length)
		{
			dest = (short)strtol(src, NULL, 10);
		}

		template<>
		void _to_type(unsigned short& dest, const char* src, unsigned long src_length)
		{
			dest = (unsigned short)strtoul(src, NULL, 10);
		}

		template<> 
		void _to_type(long& dest, const char* src, unsigned long src_length)
		{
			dest = strtol(src, NULL, 10);
		}

		template<> 
		void _to_type(unsigned long& dest, const char* src, unsigned long src_length)
		{
			dest = strtoul(src, NULL, 10);
		}

		template<> 
		void _to_type(long long& dest, const char* src, unsigned long src_length)
		{
			dest = strtoll(src, NULL, 10);
		}

		template<> 
		void _to_type(unsigned long long& dest, const char* src, unsigned long src_length)
		{
			dest = strtoull(src, NULL, 10);
		}

		template<> 
		void _to_type(double& dest, const char* src, unsigned long src_length)
		{
			dest = strtold(src, NULL);
		}

		template<> 
		void _to_type(float& dest, const char* src, unsigned long src_length)
		{
			dest = strtof(src, NULL);
		}

		template<> 
		void _to_type(char& dest, const char* src, unsigned long src_length)
		{
			dest = src[0];
		}

		template<> 
		void _to_type(bool& dest, const char* src, unsigned long src_length)
		{
			dest = *(const bool*)src;
		}

		template<> 
		void _to_type(std::string& dest, const char* src, unsigned long src_length)
		{
			dest = std::string(src, src_length);
		}

		template<> 
		void _to_type(char*& dest, const char* src, unsigned long src_length)
		{
			#pragma warning(push)
			#pragma warning(disable:4996)
			strncpy(dest, src,src_length);
			dest[src_length] = 0;
			#pragma warning(pop)
		}

		template<>
		void _to_type(std::tm& dest, const char* src, unsigned long src_length)
		{
			memset((char*)&dest, 0, sizeof(std::tm));

			char* buf = (char*)src;
			long ele[6];

			for (int i = 0; i < 6; i++)
			{
				ele[i] = strtol(buf, (char**)&buf, 10);
				if (src == buf || *buf == 0)
					break;
				else
					++buf;
			}

			dest.tm_year = ele[0] - 1900;
			dest.tm_mon = ele[1] - 1;
			dest.tm_mday = ele[2];
			dest.tm_hour = ele[3];
			dest.tm_min = ele[4];
			dest.tm_sec = ele[5];

			std::mktime(&dest);
		}

	protected:
		//type_to_str的具体实现函数。特化类型以外的其他类型，编译提示失败。确实需要的，请手动增加相应类型的特化实现。
		template <typename T>
		char* _to_str(const T& src)
		{
			static_assert(false, "No implementation for type T by default, add your own specialization for T first.");
			return nullptr;
		}

		//to_str的指针偏特化版，当src为指针类型时，优先匹配
		template<typename T> 
		char* _to_str(T* src)
		{
			sprintf_s(char_array_, sizeof(char_array_), "0x%08X", src);
			return (char*)&char_array_;
		}

		//to_str的字符串指针特化版，当src为字符串指针时，优先匹配
		template<> 
		char* _to_str(char* src)
		{
			int src_len = strlen(src);
			static int buf_len = 256;
			static std::unique_ptr<char> ptr(new char[buf_len]);

			if (quote_when_characters_to_str_ != '\0')
			{
				if (buf_len < src_len + 2 + 1)
					buf_len = src_len + 2 + 1;

				ptr.reset(new char[buf_len]);

				ptr.get()[0] = quote_when_characters_to_str_;
				memcpy(ptr.get() + 1, src, src_len);
				ptr.get()[src_len + 1] = quote_when_characters_to_str_;
				ptr.get()[src_len + 2] = 0;
			}
			else
			{
				if (buf_len < src_len + 1)
					buf_len = src_len + 1;

				ptr.reset(new char[buf_len]);
				strcpy_s(ptr.get(), buf_len, src);
			}

			return ptr.get();
		}

		//to_str的字符串指针特化版，当p为const字符串指针时，优先匹配
		template<> 
		char* _to_str(const char* src)
		{
			return to_str((char*)src);
		}

		template<> 
		char* _to_str(const std::string& src)
		{
			return to_str(src.c_str());
		}

		template<> 
		char* _to_str(const int& src)
		{
			sprintf_s(char_array_, sizeof(char_array_), "%d", src);
			return (char*)&char_array_;
		}

		template<> 
		char* _to_str(const unsigned& src)
		{
			sprintf_s(char_array_, sizeof(char_array_), "%u", src);
			return (char*)&char_array_;
		}

		template<> 
		char* _to_str(const long& src)
		{
			sprintf_s(char_array_, sizeof(char_array_), "%ld", src);
			return (char*)&char_array_;
		}

		template<> 
		char* _to_str(const unsigned long& src)
		{
			sprintf_s(char_array_, sizeof(char_array_), "%lu", src);
			return (char*)&char_array_;
		}

		template<> 
		char* _to_str(const long long& src)
		{
			sprintf_s(char_array_, sizeof(char_array_), "%lld", src);
			return (char*)&char_array_;
		}

		template<> 
		char* _to_str(const unsigned long long& src)
		{
			sprintf_s(char_array_, sizeof(char_array_), "%llu", src);
			return (char*)&char_array_;
		}

		template<> 
		char* _to_str(const double& src)
		{
			sprintf_s(char_array_, sizeof(char_array_), "%lf", src);
			return (char*)&char_array_;
		}

		template<> 
		char* _to_str(const float& src)
		{
			sprintf_s(char_array_, sizeof(char_array_), "%f", src);
			return (char*)&char_array_;
		}

		template<> 
		char* _to_str(const char& src)
		{
			if(quote_when_characters_to_str_ != '\0')
				sprintf_s(char_array_, sizeof(char_array_), "%c%c%c", quote_when_characters_to_str_,src, quote_when_characters_to_str_);
			else
				sprintf_s(char_array_, sizeof(char_array_), "%c", src);

			return (char*)&char_array_;
		}

		template<> 
		char* _to_str(const bool& src)
		{
			sprintf_s(char_array_, sizeof(char_array_), "%d", (int)src);
			return (char*)&char_array_;
		}

		template<> 
		char* _to_str(const tm& src)
		{
			if (quote_when_characters_to_str_ != '\0')
				sprintf_s(char_array_, sizeof(char_array_), "%c%04d-%02d-%02d %02d:%02d:%02d%c",
					quote_when_characters_to_str_, src.tm_year + 1900, src.tm_mon + 1, src.tm_mday, src.tm_hour, src.tm_min, src.tm_sec, quote_when_characters_to_str_);
			else
				sprintf_s(char_array_, sizeof(char_array_), "%04d-%02d-%02d %02d:%02d:%02d",
					src.tm_year + 1900, src.tm_mon + 1, src.tm_mday, src.tm_hour, src.tm_min, src.tm_sec);

			return (char*)&char_array_;
		}

	protected:
		char char_array_[32];	
		char quote_when_characters_to_str_ = '\0';
	};
}


//// example:
//#include "str_type_convert.h"
//int main()
//{
//	printf("%s\n", based::str_type_convert().to_str(1.23));
//	printf("%s\n", based::str_type_convert().to_str("1.23"));
//	printf("%s\n", based::str_type_convert().to_str(1));
//	printf("%s\n", based::str_type_convert().to_str((long long)1));
//
//	time_t now = time(NULL);
//	printf("%s\n", based::str_type_convert().to_str(*localtime(&now)));
//
//	int a1;
//based::str_type_convert().to_type(a1, "123");
//	std::cout << a1 << std::endl;
//
//	double a2;
//based::str_type_convert().to_type(a2, "");
//	std::cout << a2 << std::endl;
//
//	std::tm a3;
//based::str_type_convert().to_type(a3, "2015-08-18 15:29:00");
//	std::cout << a3.tm_year + 1900 << "\t" << a3.tm_hour << std::endl;
//
//char buf[20];
//based::str_type_convert().to_type(buf, "i'm str.");
//std::cout << buf << std::endl;
//	return 0;
//}