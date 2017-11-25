
#include <vector>
#include <iostream>
#include <map>
#include <queue>
#include <numeric>


#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/smart_ptr.hpp>

#include <unordered_map>
#include <unordered_set>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <set>
#ifdef WIN32
#include <Windows.h>
#endif

#include "../ArtLib/ArtNet.h"
#include "../ArtLib/AutoCS.h"
#include "../ArtLib/ArtNet.h"
#include "../ArtLib/ArtTools.h"

#include "../TourDeGiroData/TDGInterface.h"

#include "../TourDeGiroCommon/CommStructs.h"
#include "../TourDeGiroCommon/Player.h"
#include "../TourDeGiroCommon/Map.h"
#include "../TourDeGiroCommon/StatsStore.h"

#include "../Minixml/mxml.h"

