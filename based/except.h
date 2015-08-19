#pragma once

#include "headers_dependency.h"
#include "str_type_convert.h"


namespace based
{
	class exception : public std::exception
	{
	public:
		exception(const std::string& except_info) :
			except_no_(0),
			std::exception(except_info.c_str())
		{}

		exception(int except_no, const std::string& except_info):
			except_no_(except_no),
			std::exception(except_info.c_str())
		{}

		exception(int except_no, const std::string& except_info, const char* file_name, int file_line) :
			except_no_(except_no),
			std::exception((std::string("Exception in ") + file_name + ", line " + std::to_string(file_line) + ":\n" + except_info).c_str())
		{};

		bool operator == (const exception& right)
		{
			return except_no_ == right.except_no_;
		}

		int no()
		{
			return except_no_;
		}

	protected:
		int except_no_;
	};


	//带minidump的异常
	class exception_minidump : public exception
	{

	};


	//可以由用户手动定义的宏：
	//ENSURE_VALUE_INFO_LENGTH:info信息的预留长度
	//ENSURE_INFO_TO_CONSOLE:info信息输出到cmd窗口
	//ENSURE_INFO_TO_WINDOW:info信息弹出到messagebox
	#ifndef ENSURE_VALUE_INFO_LENGTH
	#define ENSURE_VALUE_INFO_LENGTH				512
	#endif

	class ensure
	{
	public:
		ensure()
			:SET_INFO_A(*this),
			SET_INFO_B(*this)
		{
			value_info_.reserve(ENSURE_VALUE_INFO_LENGTH);
			type_conv_.set_quote_when_characters_to_str('\"');
		}

		ensure& set_context(const char* file_name, int line, const char* expression_name)
		{
			value_info_ = value_info_ + "Ensure failed in " + file_name + ", line "+ 
				type_conv_.pointer_of_str(type_conv_.to_str(line)) + ":\nExpression: " + expression_name + "\nValues:\t";

			return *this;
		}

		template<typename T>
		ensure& save_value(const char* value_name, T&& value)
		{
			value_info_ = value_info_ + value_name + " = " + type_conv_.pointer_of_str(type_conv_.to_str(value)) + "\n\t";
			return *this;
		}

		//追加更多的提示信息到value_info_
		ensure& tips(const char* more_tips)
		{
			adjust_value_info();
			value_info_ = value_info_ + more_tips + "\n";

			return *this;
		}

		//以info级别结束：显示并返回info信息
		std::string info()
		{
			adjust_value_info();

		#ifdef ENSURE_INFO_TO_CONSOLE
			std::cout << value_info_.c_str();
		#elif ENSURE_INFO_TO_WINDOW
			MessageBoxA(NULL,value_info_.c_str(),"提示",NULL);
		#endif
			return value_info_;
		}

		//以error级别结束：调试器附加的状态下，先触发断点，否则抛出异常
		void error(int error_no = 0)
		{
			adjust_value_info();

		#ifdef _MSC_VER
			[]{
				__try {
					RaiseException(EXCEPTION_BREAKPOINT, 0, 0, 0);
				}__except (EXCEPTION_EXECUTE_HANDLER) {}
			}();
		#endif

			throw exception(error_no, value_info_.c_str());
		}

		//以fatal级别结束：生成minidump，并结束执行。有待完善
		void fatal(int error_no = 0)
		{
			adjust_value_info();

			exit(error_no);
		}

	protected:
		void adjust_value_info()
		{
			auto pos = value_info_.find_last_of("\t");
			value_info_.replace(pos, value_info_.length() - pos, "More tips: ");
		}

	public:
		ensure& SET_INFO_A;
		ensure& SET_INFO_B;
		str_type_convert type_conv_;
		std::string value_info_;
	};
}


#define SET_INFO_A(expression)	save_value(#expression,expression).SET_INFO_B
#define SET_INFO_B(expression)	save_value(#expression,expression).SET_INFO_A

#define ENSURE(expression)			\
	if(!(expression))				\
		based::ensure().set_context(__FILE__,__LINE__,#expression).SET_INFO_A

#define ENSURE_WIN32(expression)	ENSURE(expression)(GetLastError())


//example:
// int main()
// {
// 	try
// 	{
// 		//do something...
// 		//...
// 		FILE* file1 = fopen("c:\\a", "r");
// 		FILE* file2 = fopen("d:\\a", "r");
// 		ENSURE(file1 != NULL && file2 != NULL)(file1)(file2)(errno).tips("can't find file..").error();
//		//...
// 		//char buf[10];
// 		//fread(buf,1,3,file1);
// 		//...
// 	}
// 	catch (std::exception& e)
// 	{
// 		std::cout << e.what();
// 	}
// }