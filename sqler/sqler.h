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
	public:
		struct mysql_lib_init
		{
			mysql_lib_init()
			{
				if (init_state())
					return;

				ENSURE(mysql_library_init(0,NULL,NULL) == 0).tips("mysql lib init fail").warn(-1);

				init_state() = true;
			}

// 			~mysql_lib_init()
// 			{
// 				mysql_library_end();
// 			}

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
		
		session()
		{
			//mysql_init()函数在第一次初始化的时候,不是线程安全的,但是之后是线程安全的。c++11标准规定，局部静态对象初始化是线程安全的，此处确保mysql_init安全调用一次。
			//mysql_init安全调用一次后，后续的mysql_init调用都是线程安全的。
			//不对mysql_init直接传NULL，而用lib_init_dummy().init_state()的原因，1、此时!init_state一定是false 2、防止release下对lib_init_dummy的调用被优化掉
			my_sql_ = mysql_init((MYSQL*)(!lib_init_dummy().init_state()));
			type_conv_.set_quote_when_characters_to_str('\'');
		}

		session(MYSQL* my_sql)
			:my_sql_(my_sql),
			not_destroy_(lib_init_dummy().init_state())
		{
			//不对not_destroy_直接初始化为true，而用lib_init_dummy().init_state()的原因，1、此时init_state一定是true 2、防止release下对lib_init_dummy的调用被优化掉
			mysql_init(my_sql_);
			type_conv_.set_quote_when_characters_to_str('\'');
		}

		//从连接池中构造，连接池中无可用连接，则等待timeout_second秒
		session(session_pool* sp, unsigned timeout_second = (unsigned)-1)
		{
			index_in_pool_ = lib_init_dummy().init_state();			//给index_in_pool_赋值没有实际作用，只是防止release下对lib_init_dummy的调用被优化掉
			my_sql_ = (sp_=sp)->get(&index_in_pool_, timeout_second);
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
			ENSURE(my_sql_ != NULL).tips("be sure successfully connect to database first").warn(-1);
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
			ENSURE(mysql_real_connect(my_sql_, host, user, passwd, database, port, NULL, client_flag) != NULL).tips(get_error_msg()).warn(get_error_no());
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
			free_all_result();

			std::string formated_sql = sql_str;

			formated_sql.reserve(256);
			format_sql_str(formated_sql, 0, args...);

			ENSURE(mysql_real_query(my_sql_, formated_sql.c_str(), formated_sql.size()) == 0)(formated_sql).tips(get_error_msg()).warn(get_error_no());

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
				ENSURE(error_no == 0).tips(get_error_msg()).warn(error_no);	//之前的sql语句产生结果集，但store result 发生错误

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
			} ;

			has_next_result_ = true;						//复位has_next_result_，假设后续的query会有结果集
		}

		//取结果集的一行到args中
		//返回值：true成功取出，false没有结果可取。
		template<typename... Args>
		bool fetch_row(Args&... args)
		{
			ENSURE(res_ != NULL).tips("be sure to successfully store result first").warn(-1);

			MYSQL_ROW row = mysql_fetch_row(res_);
			if (row == NULL)								//res_里的结果集已取完
				return false;

			unsigned long* field_length = mysql_fetch_lengths(res_);
			ENSURE(field_length != NULL).tips(get_error_msg()).warn(get_error_no());

			_fetch_row(row, 0, field_length, args...);

			return true;
		}


	private:
		void close()
		{
			if (my_sql_ != NULL)
			{
				free_all_result();
				mysql_close(my_sql_);
				mysql_library_end();
			}
		}

		//fetch_row的具体实现函数。
		template<typename Arg>
		void _fetch_row(MYSQL_ROW& row, int field_index, unsigned long* field_len_array, Arg& dest)
		{
			long field_len = field_len_array[field_index];
			const char* src;
			field_len == 0 ? src = NULL : src = row[field_index];

			type_conv_.to_type(dest, src, field_len);
		}


		//fetch_row的具体实现函数，将行中的列值逐个转换到dests中
		template<typename Arg, typename... Args>
		void _fetch_row(MYSQL_ROW& row, int field_index, unsigned long* field_len_array, Arg& dest, Args&... dests)
		{
			long field_len = field_len_array[field_index];
			const char* src;
			field_len == 0 ? src = NULL : src = row[field_index];
			
			type_conv_.to_type(dest, src, field_len);
			_fetch_row(row, ++field_index, field_len_array, dests...);
		}


		//格式化sql str。为无需参数的sql str所准备。
		template<typename int=0>
		void format_sql_str(std::string& sql_str, int last_read_pos){}


		//格式化sql str，将'?'替换为arg的值
		//既然匹配到本函数，则arg一定是最后一个参数，检查'?'跟arg个数是否匹配。 存在arg但不存在'?'，则个数不匹配
		template<typename Arg>
		void format_sql_str(std::string& sql_str, int last_read_pos, const Arg& arg)
		{
			size_t pos = sql_str.find("?", last_read_pos);
			ENSURE(pos != sql_str.npos)(sql_str).tips("placeholders' count in sql str not compatiable with real parameters").warn(-1);

			const char* sql_param = type_conv_.pointer_of_str(type_conv_.to_str(arg));
			sql_str.replace(pos, 1, sql_param);
			pos += strlen(sql_param);

			//至此，arg...全部替换完成，如果还有'?'，则'?'和arg...的个数不一致
			ENSURE(sql_str.find("?", pos) == sql_str.npos)(sql_str).tips("placeholders' count in sql str not compatiable with real parameters").warn(-1);
		}


		//格式化sql str，将'?'替换为arg的值
		//会检查'?'跟arg个数是否匹配。 既然匹配到本函数，则arg一定存在，如果不存在'?'，则个数不匹配
		//递归替换args中的每一个参数，直到最后一个参数（最后一个参数时，会匹配为format_sql_str(std::string& sql_str, int last_read_pos, const Arg& arg)）
		template<typename Arg, typename... Args>
		void format_sql_str(std::string& sql_str, int last_read_pos, const Arg& arg, const Args&... args)
		{
			size_t pos = sql_str.find("?", last_read_pos);
			ENSURE(pos != sql_str.npos)(sql_str).tips("placeholders' count in sql str not compatiable with real parameters").warn(-1);

			const char* sql_param = type_conv_.pointer_of_str(type_conv_.to_str(arg));
			sql_str.replace(pos, 1, sql_param);
			pos += strlen(sql_param);				

			format_sql_str(sql_str, pos, args...);
		}

	public:
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
// #include "sqler.h"
// int main()
// {
// 	sql::session s;
// 	try{
// 		s.open("192.168.1.222", 3306, "root", "111111", "chuwugui");
// 		s.query("set names gbk;");
// 		s.query("select id,charge_rate,short_msg_template_postman_fetch from system_config where id = ?", 1);
// 		s.store_result();
// 
// 		int id;
// 		std::string charge_rate;
// 		std::string short_msg_template_postman_fetch;
// 		s.fetch_row(id, charge_rate, short_msg_template_postman_fetch);
// 	}
// 	catch (sql::sql_error& er)
// 	{
// 
// 	}
// }