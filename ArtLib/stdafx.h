
#include <boost/shared_ptr.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/locks.hpp>
#include <boost/asio.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/date_time/local_time_adjustor.hpp>
#include <boost/date_time/c_local_time_adjustor.hpp>
#include <boost/filesystem.hpp>
#include "../libcurl/include/curl/curl.h"


#ifdef _MSC_VER
#include <WinSock2.h>
#include <Windows.h>
#endif

#include <math.h> // for rand()
#include <stdlib.h>
#include <stdio.h> // for swprintf
#include <string.h>
#include <stdint.h>

#include <set>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <map>
#include <sstream>