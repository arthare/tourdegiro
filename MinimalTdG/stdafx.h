
#include <set>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <queue>
#include <stack>
#include <string>
#include <map>
#include <numeric>
#include <vector>

#include <unordered_map>
#include <unordered_set>


#include <boost/thread.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/circular_buffer.hpp>

#ifdef WIN32
#include <Windows.h>
#include <Psapi.h>
#include <ShellAPI.h>
#endif

#include "../TourDeGiroData/TDGInterface.h"

#include "../ArtLib/ArtTools.h"
#include "../ArtLib/md5.h"
#include "../ArtLib/AutoCS.h"
#include "../ArtLib/ArtTools.h"
#include "../ArtLib/ArtNet.h"
#include "../ArtLib/ArtVector.h"

#include "../TourDeGiroCommon/StatsStore.h"
#include "../TourDeGiroCommon/commstructs.h"
#include "../TourDeGiroCommon/CommStructs.h"
#include "../TourDeGiroCommon/Player.h"
#include "../TourDeGiroCommon/map.h"

#include "../ServerLib/SimpleClient.h"
#include "../ServerLib/TourServer.h"


#include "../TourDeGiroData/PainterIFace.h"


#include "../TourDeGiroData/TDGInterface.h"


#include "../Minixml/mxml.h"
