#pragma once

#if defined(_MSC_VER) && _MSC_VER < 1800
#error "Compiler need to support c++11, please use vs2013 or above, vs2015 e.g."
#endif

#include "headers_dependency.h"
#include "str_type_convert.h"
#include "os.h"
#ifdef WIN32
	#include <crtdbg.h>
	#include <DbgHelp.h>
	#pragma comment(lib,"DbgHelp.lib")
#endif

#if defined(ENSURE_ENABLE_LOG)
	#define GLOG_NO_ABBREVIATED_SEVERITIES
	#define GOOGLE_GLOG_DLL_DECL
	#include "../../../../sharedproject/glog-0.3.3/src/windows/glog/logging.h"
	#include "../../../../sharedproject/glog-0.3.3/src/windows/glog/log_severity.h"
	#ifdef _DEBUG
		#pragma comment(lib,"../../../../sharedproject/glog-0.3.3/vsprojects/libglog_static/Debug/libglog_static.lib")
	#else
		#pragma comment(lib,"../../../../sharedproject/glog-0.3.3/vsprojects/libglog_static/Release/libglog_static.lib")
	#endif
	namespace google { namespace glog_internal_namespace_ { bool IsGoogleLoggingInitialized(); } }
#endif

//可以由用户手动定义的宏：
//ENSURE_VALUE_INFO_LENGTH:info信息的预留长度
//ENSURE_ENABLE_LOG:是否启用日志功能
//ENSURE_ERROR_HAS_WINDOW:ENSURE宏以error级别结束时，是否显示断言失败对话框

#ifndef ENSURE_VALUE_INFO_LENGTH
	#define ENSURE_VALUE_INFO_LENGTH				512
#endif

namespace based
{
	class except : public std::exception
	{
	public:
		except(const std::string& except_info) :
			except_no_(-1),
			std::exception(except_info.c_str())
		{}

		except(int except_no, const std::string& except_info):
			except_no_(except_no),
			std::exception(except_info.c_str())
		{}

		except(int except_no, const std::string& except_info, const char* file_name, int file_line) :
			except_no_(except_no),
			std::exception((std::string("Exception in ") + file_name + ", line " + std::to_string(file_line) + ":\n" + except_info).c_str())
		{};

		bool operator == (const except& right)
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
	class exception_minidump : public except
	{
	#ifdef WIN32
	public:	
		exception_minidump(DWORD thread_id, PEXCEPTION_POINTERS except_info, BOOL client_pointers=TRUE, MINIDUMP_TYPE dump_type = MiniDumpNormal)
			:except(-1,"this is minidump exception."),
			thread_id_(thread_id),
			except_info_(except_info),
			client_pointers_(client_pointers),
			dump_type_(dump_type)
		{}

		bool save(std::wstring minidump_path, std::wstring minidump_name=L"")
		{
			if (*minidump_path.rbegin() != L'\\')
				minidump_path += L'\\';

			if (!PathFileExistsW(minidump_path.c_str()))
				CreateDirectoryW(minidump_path.c_str(), NULL);

			wchar_t name[32];
			time_t now_t = time(NULL);
			std::tm* now_tm = localtime(&now_t);
			swprintf_s(name, L"%04d-%02d-%02d-%02d-%02d-%02d.dmp", now_tm->tm_year + 1900, now_tm->tm_mon + 1, now_tm->tm_mday, now_tm->tm_hour, now_tm->tm_min, now_tm->tm_sec);

			HANDLE hFile = CreateFileW((minidump_path + minidump_name + name).c_str(), GENERIC_ALL, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			DWORD d = GetLastError();
			if (hFile != INVALID_HANDLE_VALUE)
			{
				MINIDUMP_EXCEPTION_INFORMATION mei;
				mei.ThreadId = thread_id_;
				mei.ExceptionPointers = except_info_;
				mei.ClientPointers = client_pointers_;

				BOOL ret = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, dump_type_, &mei, NULL, NULL);
				CloseHandle(hFile);

				return *(bool*)&ret;
			}

			return false;
		}

	protected:
		DWORD thread_id_;
		PEXCEPTION_POINTERS except_info_;
		BOOL client_pointers_;
		MINIDUMP_TYPE dump_type_;
	#endif
	};


	#if defined(ENSURE_ENABLE_LOG)
	class logger_init
	{
	private:
		logger_init()
		{
			init_status_ = 0;
			if (google::glog_internal_namespace_::IsGoogleLoggingInitialized())
			{
				init_status_ = 1;
				return;
			}
		
			std::string exe_path = os::get_exe_path_without_backslash<char>();
			FLAGS_log_dir = exe_path + "\\logs";
		#ifdef WIN32
			if (!CreateDirectoryA(FLAGS_log_dir.c_str(), NULL))
			{
				if (GetLastError() != ERROR_ALREADY_EXISTS)
				{
					std::cout << "create directory " << FLAGS_log_dir << "failed!" << std::endl;

				#if defined(_DEBUG)
					_CrtDbgReport(_CRT_ASSERT, __FILE__, __LINE__, NULL, NULL);
				#endif
				}
			}
		#endif
			FLAGS_stop_logging_if_full_disk = true;
			google::InitGoogleLogging(os::get_exe_path<char>().c_str());

			init_status_ = 2;
		}

	public:
		//单例之，使全局只有一份。C++11标准保证局部静态对象初始化的线程安全性。
		static logger_init& get_glog()
		{
			static logger_init gi;
			return gi;
		}

		~logger_init()
		{
			if (init_status_ == 2 && google::glog_internal_namespace_::IsGoogleLoggingInitialized())
				google::ShutdownGoogleLogging();
		}

		int get_init_status()
		{
			return init_status_;
		}

	private:
		//1初始化成功，防止release下优化掉logger_init	//2初始化成功，且是被logger_init初始化，防止析构时释放别的模块正在使用的glog引擎
		int init_status_;
	};
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
				type_conv_.pointer_of_str(type_conv_.to_str(line)) + ":\nExpression: " + expression_name + "\nValues:";

			return *this;
		}

		template<typename T>
		ensure& save_value(const char* value_name, T&& value)
		{
			value_info_ = value_info_ + value_name + " = " + type_conv_.pointer_of_str(type_conv_.to_str(value)) + "\n";
			return *this;
		}

		//追加更多的提示信息到value_info_
		ensure& tips(const char* more_tips)
		{
			value_info_ = value_info_ + "More tips: "+more_tips + "\n\n";

			return *this;
		}

		//以info级别结束：显示并返回info信息
		std::string info()
		{
			std::cout << value_info_.c_str();
			return value_info_;
		}

		//以error级别结束：显示信息，抛出异常
		void warn(int error_no = -1)
		{
			std::cout << value_info_.c_str();
			throw except(error_no, value_info_.c_str());
		}

		//以error级别结束：记录日志，可选择的弹出断言失败对话框，抛出异常，
		void error(int error_no = -1)
		{
		#if defined(ENSURE_ENABLE_LOG)
			if (logger_init::get_glog().get_init_status() != 0)
				LOG(ERROR) << value_info_;
		#endif

		#if defined(_DEBUG) && defined(WIN32) && defined(ENSURE_ERROR_HAS_WINDOW)
			_CrtDbgReport(_CRT_ASSERT,__FILE__,__LINE__,NULL,NULL);
		#endif

			throw except(error_no, value_info_.c_str());
		}

		//以fatal级别结束：生成minidump，并结束执行。有待完善
		void fatal(int error_no = 0)
		{
			exit(error_no);
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