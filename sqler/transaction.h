//事务支持类。
//构造时开启事务，析构时回滚事务，commit提交事务，rollback回滚事务。 如果在析构之前，执行了commit或者rollback，则析构时不再执行回滚操作。

#pragma once

#include "session.h"

namespace sqler
{
	class transaction
	{
	public:
		transaction(session& s):
			session_(s),
			need_rollback(true)
		{
			session_.query("BEGIN;");
		}

		~transaction()
		{
			if(need_rollback)
				rollback();
		}

		void commit()
		{
			session_.query("COMMIT;");
			need_rollback = false;
		}

		void rollback()
		{
			session_.query("ROLLBACK");
			need_rollback = false;
		}

	protected:
		session& session_;
		bool need_rollback;
	};
}