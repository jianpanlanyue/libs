//这是一个c++数据库访问层，初衷在于提供简洁接口的同时，不失灵活性。目前只支持mysql，后续考虑增加其他数据库的支持。
//有些时候，我们需要执行增删改查之外的其他操作，而类中提供的接口很难覆盖数据库的全部原生接口，则可以通过get_backend_impl()获取底层数据库的描述符，以调用数据库原生接口完成需求。
//在设计的时候，并没有使用查询类，结果集类等常见的模式，而是直接模拟数据库的查询，提供一个query()核心方法，数据库中使用什么样的sql语句，query()的参数就是什么样的——所见即所见。
//传递sql语句的时候，支持本地参数（sql中需要参数的地方以"?"代替），后面接本地参数。 支持变参与可变参数类型，见demo。
//支持数据库连接池、支持存储过程。
//如果出现编译时出现socket相关函数的重复定义错误，可能是之前已经包含了低版本的socket头文件。将本头文件放到低版本socket头文件前面，或者直接放在所有语句最前面，即可解决。

#pragma once

#if defined(_MSC_VER) && _MSC_VER < 1800
	#error "Compiler need to support c++11, please use vs2013 or above, vs2015 e.g."
#endif

#include "../based/headers_all.h"
#include "session_pool.h"

namespace sqler
{
	typedef based::except sql_error;

	#define THROW_SQL_EXCEPTION THROW_SQL_EXCEPTION0
	#define THROW_SQL_EXCEPTION0() throw(sqler::sql_error((int)get_error_no(),get_error_msg(),__FILE__,__LINE__))
	#define THROW_SQL_EXCEPTION2(ERROR_NO,ERROR_STR) throw(sqler::sql_error(ERROR_NO,ERROR_STR,__FILE__,__LINE__))

	class session
	{
	protected:
		struct mysql_lib_init
		{
			mysql_lib_init()
			{
				if (init_state())
					return;

				ENSURE(mysql_library_init(0,NULL,NULL) == 0).warn(-1, "mysql lib init fail");

				init_state() = true;
			}

			~mysql_lib_init()
			{
				mysql_library_end();
			}

			bool& init_state()
			{
				static bool init_ok = false;
				return init_ok;
			}
		};

		mysql_lib_init& lib_init_dummy()
		{
			static mysql_lib_init dummy;
			return dummy;
		}

	public:
		session()
		{
			//mysql_init()函数在第一次初始化的时候,不是线程安全的,但是之后是线程安全的。c++11标准规定，局部静态对象初始化是线程安全的，此处确保mysql_init安全调用一次。
			//mysql_init安全调用一次后，后续的mysql_init调用都是线程安全的。
			//不对mysql_init直接传NULL，而用lib_init_dummy().init_state()的原因，1、此时!init_state一定是false 2、防止release下对lib_init_dummy的调用被优化掉
			my_sql_ = mysql_init((MYSQL*)(!lib_init_dummy().init_state()));
			ENSURE(my_sql_ != NULL).warn(-1, "Mysql init fail, maybe memory insufficient.");
			type_conv_.set_quote_when_characters_to_str('\'');
		}

		session(MYSQL* my_sql)
			:my_sql_(my_sql),
			not_destroy_(lib_init_dummy().init_state())
		{
			//不对not_destroy_直接初始化为true，而用lib_init_dummy().init_state()的原因，1、此时init_state一定是true 2、防止release下对lib_init_dummy的调用被优化掉
			my_sql_ = mysql_init(my_sql_);
			ENSURE(my_sql_ != NULL).warn(-1, "Mysql init fail, maybe memory insufficient.");
			type_conv_.set_quote_when_characters_to_str('\'');
		}

		//从连接池中构造，连接池中无可用连接，则等待timeout_second秒
		session(session_pool* sp, unsigned timeout_second = (unsigned)-1)
		{
			index_in_pool_ = lib_init_dummy().init_state();			//给index_in_pool_赋值没有实际作用，只是防止release下对lib_init_dummy的调用被优化掉
			my_sql_ = (sp_=sp)->get(&index_in_pool_, timeout_second);
			ENSURE(my_sql_ != NULL).warn(-1, "Mysql init fail, maybe memory insufficient.");
			type_conv_.set_quote_when_characters_to_str('\'');
		}

		//从连接池中构造，则还给连接池；否则关闭连接
		~session()
		{
			free_all_result();

			if (sp_ && index_in_pool_ != -1)
				sp_->release(index_in_pool_);
			else if (!not_destroy_)
				close();
		}

		//取最近一次sql query的错误描述
		const char* get_error_msg()
		{
			return mysql_error(my_sql_);
		}

		//取最近一次sql query的错误码
		unsigned get_error_no()
		{
			return mysql_errno(my_sql_);
		}

		//取列的数目
		unsigned get_field_count()
		{
			return mysql_field_count(my_sql_);
		}

		//取得数据库后台
		MYSQL* get_backend_impl()
		{
			return my_sql_;
		}
		
		//取得结果集
		MYSQL_RES* get_result_set()
		{
			return res_;
		}

		//连接数据库
		void open(const char* host, unsigned port, const char* user, const char* passwd, const char* database, 
			unsigned long client_flag = CLIENT_MULTI_STATEMENTS | CLIENT_REMEMBER_OPTIONS)
		{
			ENSURE(mysql_real_connect(my_sql_, host, user, passwd, database, port, NULL, client_flag) != NULL).warn(get_error_no(), get_error_msg());
		}
		
		void set_option(const int option_type, const char* option_value_ptr)
		{
			mysql_options(my_sql_, (mysql_option)option_type, option_value_ptr);
		}

		//传递sql语句。
		//sql_str，要执行的sql语句，需要参数的时候，使用'?'做占位符。可以传递多条sql语句，以';'分隔。  
		//args...，真实的参数，个数与sql_str中的'?'一样，否则会抛出异常。 函数内部会将'?'字符替换为args...的真实值，并执行查询。
		//demo:
		//int my_id = 1;
		//s.query("select * from table_a where id=? and name=?",my_id,"my_name");
		template<typename... Args>
		unsigned long long query(const char* sql_str, const Args&... args)
		{
			ENSURE(sql_str != NULL && strlen(sql_str) != 0)(sql_str)(strlen(sql_str)).warn(-1, "sql statement passed can not be empty.");

			free_all_result();

			std::string formated_sql = sql_str;
			int place_holders_count = 0;
			int args_count = sizeof...(args);
			for (auto& ele : formated_sql)	
				if (ele == '?')	
					++place_holders_count;
			ENSURE(place_holders_count == args_count)(place_holders_count)(args_count).warn(-1,"arguments passed should be compatiable with placeholders' count in sql.");
			
			formated_sql.reserve(256);
			int last_read_pos = formated_sql.length()-1;
			parameter_pack_dummy_wrapper(format_sql_str(formated_sql, last_read_pos, args)...);

			ENSURE(mysql_real_query(my_sql_, formated_sql.c_str(), formated_sql.size()) == 0)(formated_sql).warn(get_error_no(), get_error_msg());

			return next_result();
		}
		
		template<typename... Args>
		unsigned long long query(const std::string& sql_str, const Args&... args)
		{
			return query(sql_str.c_str(), args...);
		}

		//存储结果集，并返回当前结果集的行数。
		//返回值：结果集行数。不产生结果集，或结果集内没有数据为0；结果集内有数据的为数据的行数；取结果集过程出错的抛出异常。
		unsigned long long next_result()
		{
			free_former_result();
			
			if (!has_next_result_)
				return 0;

			res_ = mysql_store_result(my_sql_);

			if (mysql_next_result(my_sql_) != 0)								//预判下一个结果集是否存在
				has_next_result_ = false;

			if (res_ == NULL)
			{
				unsigned error_no = get_error_no();
				ENSURE(error_no == 0).warn(error_no, get_error_msg());			//之前的sql语句产生结果集，但store result 发生错误

				return 0;														//之前的sql语句不产生结果集
			}

			return mysql_num_rows(res_);										//返回结果行数，0表示语句产生结果集，但结果集里没有结果
		}


		void free_former_result()
		{
			if (res_ != NULL)
			{
				mysql_free_result(res_);
				res_ = NULL;
			}
		}

		void free_all_result()
		{
			free_former_result();
			
			while (has_next_result_ || mysql_next_result(my_sql_) == 0)
			{
				has_next_result_ = false;
				res_ = mysql_store_result(my_sql_);
				free_former_result();
			}

			has_next_result_ = true;						//复位has_next_result_，假设后续的query会有结果集
		}

		//使用此函数接收"先参与表达式后解包"出来的参数，绕开编译错误
		template <typename ... Args>
		void parameter_pack_dummy_wrapper(const Args&... t) {};


		//取结果集的一行到args中
		//返回值：true成功取出，false没有结果可取。传入的本地参数数目，必须小于等于sql语句中查询出的列数目。
		template<typename... Args>
		bool fetch_row(Args&... args)
		{
			ENSURE(res_ != NULL).warn(-1, "be sure to successfully store result first");

			MYSQL_ROW row = mysql_fetch_row(res_);
			if (row == NULL)								//res_里的结果集已取完
				return false;

			int field_count = mysql_num_fields(res_);
			int args_count = sizeof...(args);
			ENSURE(args_count <= field_count)(args_count)(field_count).warn(-1, "arguments you passed should be less or equal than fields count selected in sql.");

			unsigned long* field_length = mysql_fetch_lengths(res_);
			ENSURE(field_length != NULL).warn(get_error_no(), get_error_msg());
			
			parameter_pack_dummy_wrapper(_fetch_row(row, --args_count, field_length, args)...);

			return true;
		}

	protected:
		void close()
		{
			if (my_sql_ != NULL)
			{
				free_all_result();
				mysql_close(my_sql_);
			}
		}

		//fetch_row的具体实现函数。
		template<typename Arg>
		int _fetch_row(MYSQL_ROW& row, int field_index, unsigned long* field_len_array, Arg& dest)
		{
			ENSURE(field_index>=0)(field_index)(field_len_array).warn(-1,"field_index is negative...");

			long field_len = field_len_array[field_index];
			const char* src;
			field_len == 0 ? src = NULL : src = row[field_index];

			type_conv_.to_type(dest, src, field_len);
			return 0;
		}

		//格式化sql str。为无需参数的sql str所准备。
		template<typename int=0>
		int format_sql_str(std::string& sql_str, int last_read_pos) { return 0; }


		//格式化sql str，将'?'替换为arg的值
		//既然匹配到本函数，则arg一定是最后一个参数，检查'?'跟arg个数是否匹配。 存在arg但不存在'?'，则个数不匹配
		template<typename Arg>
		int format_sql_str(std::string& sql_str, int& last_read_pos, const Arg& arg)
		{
			size_t pos = sql_str.rfind("?", last_read_pos);
			ENSURE(pos != sql_str.npos)(sql_str)(last_read_pos).warn(-1, "can not reverse find '?' from last_read_pos in sql statement.");

			const char* sql_param = type_conv_.to_str(arg);
			sql_str.replace(pos, 1, sql_param);
			last_read_pos = pos-1;

			return 0;
		}

	protected:
		based::str_type_convert type_conv_;
		session_pool* sp_;
		int index_in_pool_ = -1;
		bool not_destroy_ = false;
		MYSQL* my_sql_ = NULL;
		MYSQL_RES* res_ = NULL;
		bool has_next_result_ = true;
	};
}

//演示：
//#include "session.h"
//int main()
//{
//	sqler::session s;
//	try {
//		s.open("host_ip", 3306, "user", "password", "db_name");
//
//		int id = 10;
//		std::string cfg_type = "cfg_back";
//		double charge_rate;
//		std::string short_msg;
//		//查询所有id<10且类型为"cfg_back"的记录，保存到本地变量charge_rate，short_msg中。
//		//表中，id为整数型，type为字符串型，charge_rate为浮点型，short_msg为字符串型
//		s.query("select charge_rate,short_msg from system_config where id < ? and type = ?", id, cfg_type);
//		while (s.fetch_row(charge_rate, short_msg))
//		{
//			//do something..
//			std::cout << charge_rate << short_msg;
//		}
//	}
//	catch (sqler::sql_error& er)
//	{
//		std::cout << er.what();
//	}
//}