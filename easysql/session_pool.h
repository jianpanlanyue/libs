#pragma once

#include "../based/headers_dependency.h"
#include <mysql.h>
#pragma comment(lib,"libmysql.lib")

namespace easysql
{
	class session_pool
	{
	public:
		session_pool(int size = 8)
		{
			semaphore_ = CreateSemaphore(NULL, size, size, NULL);
			InitializeCriticalSection(&critical_section_);

			while (size-- > 0)
				pool_.emplace_back(false, MYSQL());
		}

		~session_pool()
		{
			for (auto& ele : pool_)
			{
				if (ele.first)
				{
					mysql_close(&ele.second);
					mysql_library_end();
				}
			}

			CloseHandle(semaphore_);
			DeleteCriticalSection(&critical_section_);
		}

		MYSQL* at(unsigned index)
		{
			EnterCriticalSection(&critical_section_);
			MYSQL* my_sql=  &pool_.at(index).second;
			LeaveCriticalSection(&critical_section_);
			return my_sql;
		}

		MYSQL* get(int* index, unsigned timeout_second = (unsigned)-1)
		{
			*index = -1;

			timeout_second = (timeout_second == INFINITE ? INFINITE : timeout_second * 1000);
			DWORD dwRet = WaitForSingleObject(semaphore_, timeout_second);
			if (dwRet != WAIT_OBJECT_0)
				return NULL;

			MYSQL* mysql = NULL;
			int i = 0;
			EnterCriticalSection(&critical_section_);
			for (auto& it = pool_.begin(); it != pool_.end(); ++it, ++i)
			{
				if (!it->first)
				{
					mysql = &it->second;
					it->first = true;
					break;
				}
			}
			LeaveCriticalSection(&critical_section_);

			*index = i;
			return assert(mysql), mysql;
		}

		void release(int ele_pos)
		{
			EnterCriticalSection(&critical_section_);
			pool_.at(ele_pos).first = false;
			LeaveCriticalSection(&critical_section_);

			ReleaseSemaphore(semaphore_, 1, NULL);
		}

	private:
		std::vector<std::pair<bool, MYSQL>> pool_;			//bool是否已被占用
		HANDLE semaphore_;
		CRITICAL_SECTION critical_section_;
	};
}