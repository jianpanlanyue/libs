#pragma once

//based下的功能文件经常使用到的库头文件
#ifdef WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#include<Shlwapi.h>
	#pragma comment(lib,"Shlwapi.lib")
#endif

#include <assert.h>
#include <iostream>
#include <string>
#include <ctime>
#include <type_traits>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <exception>
#include <algorithm>
