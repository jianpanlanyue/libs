//����һ��c++���ݿ���ʲ㣬���������ṩ���ӿڵ�ͬʱ����ʧ����ԡ�Ŀǰֻ֧��mysql���������������������ݿ��֧�֡�
//��Щʱ��������Ҫִ����ɾ�Ĳ�֮��������������������ṩ�Ľӿں��Ѹ������ݿ��ȫ��ԭ���ӿڣ������ͨ��get_backend_impl()��ȡ�ײ����ݿ�����������Ե������ݿ�ԭ���ӿ��������
//����Ƶ�ʱ�򣬲�û��ʹ�ò�ѯ�࣬�������ȳ�����ģʽ������ֱ��ģ�����ݿ�Ĳ�ѯ���ṩһ��query()���ķ��������ݿ���ʹ��ʲô����sql��䣬query()�Ĳ�������ʲô���ġ���������������
//����sql����ʱ��֧�ֱ��ز�����sql����Ҫ�����ĵط���"?"���棩������ӱ��ز����� ֧�ֱ����ɱ�������ͣ���demo��
//֧�����ݿ����ӳء�֧�ִ洢���̡�
//������ֱ���ʱ����socket��غ������ظ�������󣬿�����֮ǰ�Ѿ������˵Ͱ汾��socketͷ�ļ�������ͷ�ļ��ŵ��Ͱ汾socketͷ�ļ�ǰ�棬����ֱ�ӷ������������ǰ�棬���ɽ����

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
			//mysql_init()�����ڵ�һ�γ�ʼ����ʱ��,�����̰߳�ȫ��,����֮�����̰߳�ȫ�ġ�c++11��׼�涨���ֲ���̬�����ʼ�����̰߳�ȫ�ģ��˴�ȷ��mysql_init��ȫ����һ�Ρ�
			//mysql_init��ȫ����һ�κ󣬺�����mysql_init���ö����̰߳�ȫ�ġ�
			//����mysql_initֱ�Ӵ�NULL������lib_init_dummy().init_state()��ԭ��1����ʱ!init_stateһ����false 2����ֹrelease�¶�lib_init_dummy�ĵ��ñ��Ż���
			my_sql_ = mysql_init((MYSQL*)(!lib_init_dummy().init_state()));
			ENSURE(my_sql_ != NULL).warn(-1, "Mysql init fail, maybe memory insufficient.");
			type_conv_.set_quote_when_characters_to_str('\'');
		}

		session(MYSQL* my_sql)
			:my_sql_(my_sql),
			not_destroy_(lib_init_dummy().init_state())
		{
			//����not_destroy_ֱ�ӳ�ʼ��Ϊtrue������lib_init_dummy().init_state()��ԭ��1����ʱinit_stateһ����true 2����ֹrelease�¶�lib_init_dummy�ĵ��ñ��Ż���
			my_sql_ = mysql_init(my_sql_);
			ENSURE(my_sql_ != NULL).warn(-1, "Mysql init fail, maybe memory insufficient.");
			type_conv_.set_quote_when_characters_to_str('\'');
		}

		//�����ӳ��й��죬���ӳ����޿������ӣ���ȴ�timeout_second��
		session(session_pool* sp, unsigned timeout_second = (unsigned)-1)
		{
			index_in_pool_ = lib_init_dummy().init_state();			//��index_in_pool_��ֵû��ʵ�����ã�ֻ�Ƿ�ֹrelease�¶�lib_init_dummy�ĵ��ñ��Ż���
			my_sql_ = (sp_=sp)->get(&index_in_pool_, timeout_second);
			ENSURE(my_sql_ != NULL).warn(-1, "Mysql init fail, maybe memory insufficient.");
			type_conv_.set_quote_when_characters_to_str('\'');
		}

		//�����ӳ��й��죬�򻹸����ӳأ�����ر�����
		~session()
		{
			free_all_result();

			if (sp_ && index_in_pool_ != -1)
				sp_->release(index_in_pool_);
			else if (!not_destroy_)
				close();
		}

		//ȡ���һ��sql query�Ĵ�������
		const char* get_error_msg()
		{
			return mysql_error(my_sql_);
		}

		//ȡ���һ��sql query�Ĵ�����
		unsigned get_error_no()
		{
			return mysql_errno(my_sql_);
		}

		//ȡ�е���Ŀ
		unsigned get_field_count()
		{
			return mysql_field_count(my_sql_);
		}

		//ȡ�����ݿ��̨
		MYSQL* get_backend_impl()
		{
			return my_sql_;
		}
		
		//ȡ�ý����
		MYSQL_RES* get_result_set()
		{
			return res_;
		}

		//�������ݿ�
		void open(const char* host, unsigned port, const char* user, const char* passwd, const char* database, 
			unsigned long client_flag = CLIENT_MULTI_STATEMENTS | CLIENT_REMEMBER_OPTIONS)
		{
			ENSURE(mysql_real_connect(my_sql_, host, user, passwd, database, port, NULL, client_flag) != NULL).warn(get_error_no(), get_error_msg());
		}
		
		void set_option(const int option_type, const char* option_value_ptr)
		{
			mysql_options(my_sql_, (mysql_option)option_type, option_value_ptr);
		}

		//����sql��䡣
		//sql_str��Ҫִ�е�sql��䣬��Ҫ������ʱ��ʹ��'?'��ռλ�������Դ��ݶ���sql��䣬��';'�ָ���  
		//args...����ʵ�Ĳ�����������sql_str�е�'?'һ����������׳��쳣�� �����ڲ��Ὣ'?'�ַ��滻Ϊargs...����ʵֵ����ִ�в�ѯ��
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

		//�洢������������ص�ǰ�������������
		//����ֵ����������������������������������û������Ϊ0��������������ݵ�Ϊ���ݵ�������ȡ��������̳�����׳��쳣��
		unsigned long long next_result()
		{
			free_former_result();
			
			if (!has_next_result_)
				return 0;

			res_ = mysql_store_result(my_sql_);

			if (mysql_next_result(my_sql_) != 0)								//Ԥ����һ��������Ƿ����
				has_next_result_ = false;

			if (res_ == NULL)
			{
				unsigned error_no = get_error_no();
				ENSURE(error_no == 0).warn(error_no, get_error_msg());			//֮ǰ��sql���������������store result ��������

				return 0;														//֮ǰ��sql��䲻���������
			}

			return mysql_num_rows(res_);										//���ؽ��������0��ʾ����������������������û�н��
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

			has_next_result_ = true;						//��λhas_next_result_�����������query���н����
		}

		//ʹ�ô˺�������"�Ȳ�����ʽ����"�����Ĳ������ƿ��������
		template <typename ... Args>
		void parameter_pack_dummy_wrapper(const Args&... t) {};


		//ȡ�������һ�е�args��
		//����ֵ��true�ɹ�ȡ����falseû�н����ȡ������ı��ز�����Ŀ������С�ڵ���sql����в�ѯ��������Ŀ��
		template<typename... Args>
		bool fetch_row(Args&... args)
		{
			ENSURE(res_ != NULL).warn(-1, "be sure to successfully store result first");

			MYSQL_ROW row = mysql_fetch_row(res_);
			if (row == NULL)								//res_��Ľ������ȡ��
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

		//fetch_row�ľ���ʵ�ֺ�����
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

		//��ʽ��sql str��Ϊ���������sql str��׼����
		template<typename int=0>
		int format_sql_str(std::string& sql_str, int last_read_pos) { return 0; }


		//��ʽ��sql str����'?'�滻Ϊarg��ֵ
		//��Ȼƥ�䵽����������argһ�������һ�����������'?'��arg�����Ƿ�ƥ�䡣 ����arg��������'?'���������ƥ��
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

//��ʾ��
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
//		//��ѯ����id<10������Ϊ"cfg_back"�ļ�¼�����浽���ر���charge_rate��short_msg�С�
//		//���У�idΪ�����ͣ�typeΪ�ַ����ͣ�charge_rateΪ�����ͣ�short_msgΪ�ַ�����
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