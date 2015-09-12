#pragma once

//连接两个名字: NAMES_CAT(OBJ,A)得到OBJA
#define NAMES_CAT_IMPL(NAME1,NAME2) NAME1##NAME2
#define NAMES_CAT(NAME1, NAME2) NAMES_CAT_IMPL(NAME1,NAME2)

#ifndef tstring
	#if defined(_UNICODE) || defined(UNICODE)
		#define tstring std::wstring
	#else
		#define tstring std::string
	#endif
#endif

#ifndef NAMESPACE_BEGIN
#define NAMESPACE_BEGIN(SPACE_NAME) namespace SPACE_NAME{
#define NAMESPACE_END }
#endif

