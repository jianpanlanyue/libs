#pragma once

#include "headers_dependency.h"
#include "def.h"

namespace based
{
	class scope_guard
	{
	public:
		scope_guard(const std::function<void()>& on_exit = nullptr)
			:on_exit_(on_exit)
		{}

		scope_guard(scope_guard const&) = delete;
		scope_guard& operator=(scope_guard const&) = delete;

		~scope_guard()
		{
			if (on_exit_)
			{
				on_exit_();
			}
		}

		void cancel()
		{
			on_exit_ = nullptr;
		}

	protected:
		std::function<void()> on_exit_;
	};
}

#define SCOPE_GUARD(callback) based::scope_guard NAMES_CAT(scope_guard_,__LINE__)(callback)



//example:
//void fun()
//{
//	FILE* file = NULL;
//	bool success = false;
//
//	SCOPE_GUARD([&] {
//		if (file)
//			fclose(file);
//
//		if (!success)
//			std::cout << "fail" << std::endl;
//	});
//
//	file = fopen("c:\\a", "r+");
//	if (file == NULL)
//		return;
//
//	char buf[10];
//	if (fread(buf, 1, 10, file) != 10)
//		return;
//
//	if (fwrite(buf, 1, 10, file) != 10)
//		return;
//
//	success = true;
//	std::cout << "success" << std::endl;
//}
//
//int main()
//{
//	fun();
//  return 0;
//}
