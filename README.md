# Tour De Giro

### History

Tour de Giro was created in 2012 by Art Hare, because he got a cool ANT+-compatible powertap and had always wanted to be able to race people online to help motivate his workouts.  Initially it was intended to be an open-source app, but a bunch of friends jerkily encouraged Art to try to make a business out of it (protip: Friends don't let or encourage friends start businesses).  In late 2012, he was joined by Eric Greig, who helped with running the business, customer support, graphics, and basically everything that wasn't writing code.  From 2013 until 2015 it was (or at least seemed to us) to be quite popular by online-racing standards at the time, peaking at about 70 people online concurrently on Tuesday evenings.  In early 2015, Zwift launched and ate our lunch.  This was good timing anyway, as running Tour de Giro for the fairly meagre revenue it generated was burning us out.  Congrats and Kudos to the Zwift team for making mad cash where we could not!

Now that its star has faded, it seems reasonable to return it to the FOSS community that it was originally intended for.

### Status of the Code

Since TdG was written in Art's part time while he was still at a fulltime job, there's a decent amount of code that probably needed some more time in the oven.  Some components would be written with some new fancy approach, some components would be written hackishly with the main criteria of "does this work well _enough_, and is it stable?".  The general goal for all the networking and server code was to be able to support about 200 riders simultaneously without taking huge amounts of bandwidth or CPU resources.  The servers did (and still do) run on $10/mth linode VMs without too many memory or CPU-resource constraints.

### There's not a lot of code here!  Where did your time go?

The graphics, crappy as they were, took a ton of time to build, maintain and test.  Also, running a business with real customers required an immense amount of customer support.  There were also many, many features that were built and never released or built and never become popular.  On top of that, I had a real job at a real electronics company and still wanted to have a normal life as well.

### What are each of the sub-libraries for?
- `ArtLib` - Lower-level helper functions, networking/sockets abstraction layer.  And yes, this is one of many things in the codebase named after the author.
- `boost` - boost
- `GameAI` - The actual AIs that TdG used.  Writing proper AIs to race and win on a limited wattage budget would probably be _really_ fun.
- `GameLib` - The guts of the main game client.  If you want to write your own client, you'll want to link to GameLib, instantiate a `TDGGameClient`, and tell it to connect to a server.  See MinimalTDG for an example.
- `libcurl` - The CURL HTTP-request library
- `MinimalTDG` - The bare-minimum client-server example project that people can start from if they want to make their very own TdG
- `Minixml` - Minixml, because I hadn't learned and appreciated JSON at the time I wrote TdG, so it was all XML.
- `nedmalloc` - Weird theoretically-speedy memory-allocator
- `ServerLib` - Analagous to GameLib, this is the guts of the TdG server code.  If you want to write your own standalone server binary, you'll want to link to ServerLib and instantiate a `TourServer`, then tell it to start listening for incoming connections.
- `TourDeGiroCommon` - This is where all shared game logic lives.  The physics calculations, map calculations, etc are all here.  Be careful modifying this stuff without versioning stuff off, because a game client with changed TourDeGiroCommon physics calculations will behave oddly when connected to a different server.  Most important files: `map.cpp` (map representation and construction from GPX files), `player.cpp` (player physics/drafting), `CommStructs.cpp` (network data population)
- `TourDeGiroData` - This is a badly-named library including all the interfaces and other very-widely-shared definitions used across the app.
- `zlib` - zlib


### How to build
1. In the directory you cloned the repo, do `cmake -G Visual Studio 10` (I'm using cmake 2.8.11.2.  There are probably newer versions.  Hopefully they work).  You may have to install cmake first.
2. Open the generated .SLN
3. MinimalTdG should build in Visual Studio Community 2015 or Visual C++ 2010 Express.  If it doesn't, you'll probably have decent luck commenting requesting a fix.  I HATE open-source projects that don't build easily.  Sometimes just trying to build again can help.

#### If I Had a Million Man-Hours (aka Todos as of November 2017)

0. Golden Cheetah integration
1. Graphics Engine Rewrite
2. User-interface / game setup rewrite.  Now having some modicum of web skills, I'd probably do this with electron nowadays.
3. Unicode support (all the strings are just std::strings)
4. Simple setup for LAN play, rather than depending on online server resources.
5. Network-protocol versioning (we only had one major revision of the network protocol, and accomplished it by just using different TCP ports).
6. Clean up hungarian notation usage/misusage, as well as function naming conventions, etc.  When you're a time-starved one-man shop, it's easy to fall into bad habits, so I did.

#### Things I think are good
1. The networking code is pretty solid.  It doesn't take much bandwidth, the server-side doesn't gobble CPU cycles.  The SimpleClient/SimpleServer templated classes are probably a bit nuts and had to follow, but over the years of TdG, they never needed much maintenance.
2. The interface between game logic and the painting is fairly strong, so it should be easy for someone that just wants to cook up a graphics engine to do so without having to muck about with networking.  Check out ConsolePainter::PaintState in the MinimalTdG project.
3. The StatsStore

### Differences from the released game

1. Since the graphics were universally described as "from the 90s", they've been removed to simplify compiling.  The MinimalTdG project has an example of a console-based "graphics engine" and could easily have a console-based "game setup" stage, but it will be up the community to build something worthy.
2. The ANT+ communication components have been taken out.  My original thrust while OSSing this was initially to get it integrated into Golden Cheetah and thus use their device-communication components, but due to laziness I skipped out on that in favour of just getting the guts onto github sooner rather than later.
3. The map-smoothing code used to depend on some spline code from Ogre3D.  I'm a hurting guy and couldn't replicate it, so I said "linear interpolation it is!", and so the maps will have straight lines between waypoints.
4. For obvious reasons, the server components that talk to tourdegiro.com's MySQL DB have been removed.

## If I want to do something with this code, where do I start?

#### I just want to poke around MinimalTdG and understand what it is doing?
- `ConsolePainter` - This class represents a _very_ simple graphics implementation.  In this case, its "graphics" are a console window.
  - Baby development steps: Make ConsolePainter start and SDL window and draw cyclist's positions and wattages in it.
  - Long-term development steps: Fire up a unity or unreal engine or something and make it beautiful.
- `ConsoleLocalPlayer` - This is mostly a bunch of parameters indicating a user's visual preferences.  It is intended to help start up the painter by giving each player a viewport position (remember, TdG allows for split-screen play), player color, etc.
- `ConsolePlayerSetupData` - Feeds a name and a mass into the actual game client.  Kinda similar to and could probably be merged with `ConsoleLocalPlayer`, really.
- `ConsoleTDGSetupState` - Tells the game client our master account ID, the server IP we want to connect to, and the players that are riding locally.
- `GameEntryPoint` - An example of a minimalist startup for the game.  It makes some fake players, creates and starts a server that loads from a specific map, then connects a game client to that server.
- `GetSettingsFromUI` - A fake function that imagines if we actually had some setup UI here, and fakes like we just got some map settings and things from the user.

#### I want to write a new graphics engine!
Start with the MinimalTdG project, and copy or modify the ConsolePainter class.  In `ConsolePainter::Painter_Init`, you'll want to load your art assets.  In `ConsolePainter::PaintState`, you'll want to paint the current state of the game (as represented in the `TDGFRAMEPARAMS` object).  Note that there is no guarantee that you'll have the same map or same set of players from frame to frame (players might drop out, the server might change the map, etc).

#### I want to create a new game mode with wacky upgrades or different physics!
The frame-by-frame physics (including drafting, aero, etc) occur in `TourDeGiroCommon/Player.cpp`, in the function `RunPhysics`.  Note that this function is shared by the game client and the server.  If you modify it on your client but connect to a server without your modifications, you'll end up with a jumpy rider, because you'll keep getting position updates from the server that differ from and override your game client's renderings.

#### I want to make a LAN Server Host and connect to it!
This would be a pretty quick modification to MinimalTdG.  You'll see that the first half of `GameEntryPoint` is spent setting up a server, and the second half is a client.  All you need to do is make a binary that doesn't set up the client, and then make another client that connects to the IP address of your server.

#### I want to make my server save data to a database/web service to save data and load maps!
There is an interface in `TourDeGiroCommon/StatsStore.h` called StatsStore.  This is the interface the server uses to save and load data from any kind of a back-end.  The MinimalTdG server runs off a NullStatsStore, which basically does as little as possible to let the game start.  The real TdG servers did (still do!) run off a StatsStore implementation that saves to our MySQL DB.

Important StatsStore/DB terms:
- masterId - this is a representation of the account ID of the player.  In Tour de Giro terms, signing up gave you an account ID, under which you could have many rider IDs.
- playerId - this is the ID of the alias they're currently riding under.  Master account "tourdegiro@tourdegiro.com" might have "art", "eric", or "demo rider" as different rider names.  Think of masterID as a household, and playerId as a individual person in that household.
- mapId - Every map has a unique ID.
- raceId - An ID of a single race.  One race can have many race entries.
- raceEntry - An ID of a single rider's race effort.  Tied to a single raceId.  Tied to a single playerId.
- race "replay" - This is how we stored second-by-second wattage/distance/elevation data.  There was actually a prototype online race replay system, but it never got pretty enough to be useful.

#### I want to actually connect to my ANT+ or Bluetooth device!
I may yet bring back TdG's ANT+ connection components.  Until then, it's actually free to get the SDKs you need from thisisant.com.  Once you get ANT+ or Bluetooth data received and decoded, you'll want to:
0. Have a function on your communication module where the user can trigger "Discover an ANT+ PM for player index X"
1. Pass an IPowerSourceReceiver to your ANT+/BLE system
2. Call IPowerSourceReceiver::SetPower / SetCadence / SetHR as your data comes in, and indicate the player index of the data.  TDGGameClient will assign the received power to your rider and communicate that to the server.

- Tip: For a baby step, start by making a timer callback or something that calls TdGGameClient::SetPower and make sure that your rider in MinimalTdG responds appropriately.
- Stupid Tip: I used unsigned shorts to represent power numbers over the network, so try not to exceed 65000W or send negative values.
- Stupid Design Decision Warning: IPowerSourceReceiver communicates errors (like devices going disconnected or needing calibration) by sending negative power values.  This makes things especially fun because of the unsigned short going over the network.  See `POWERRECV_NEEDCALIB` and its friends in `TourDeGiroData/TDGInterface.h`.  Honestly, just redoing the IPowerSourceReceiver setup might not be a bad idea.
- Tip: If you want to redo how TdG receives power data, keep in mind the main thing you have to do is call LOCALPLAYERDATA::AddPower with your new power data.  This currently happens in `TDGGameClient::SetPower`.

#### I want to rewrite the networking protocol!
Given that this is just about the only thing in TdG that almost never needed maintenance, you may as well just write your own game.  Just about every part of TdG would need to change if the networking protocol was rewritten, so it would honestly be easier to just roll your own from scratch with TdG's perhaps as an inspiration.

However, I can help translate some of the SimpleClient/SimpleServer (...not so simple anymore.  They were once...) conventions:

Note that SimpleServer and SimpleClient are both very templated because that was a thing I was interested in when I started writing them.  Sorry :-)

##### SimpleClient Functions
Note that these are actually generic classes, and upon re-reading this code I think it wouldn't be hard to build a different networked game out of the SimpleServer/SimpleClient templates.  However, the descriptions below are TdG-specific.

- `SimpleClient::SimpleClient_BuildDesc` - This builds a description of your current client.  It includes your master account ID (see database section above), how many local players you have, their names, device types, and a map request (if you have one)
- `SimpleClient::SimpleClient_BuildState` - This builds a description of your instantaneous game state.  For the most part, this just includes the last detected wattage from all the local players.  There's a couple flags that can be sent as well if your client wants players to be able to move left/right.  See `ACTION_FLAG_LEFT` and friends in `TourDeGiroCommon/CommStructs.h`.  This is called repeatedly and rapidly during gameplay and is not on the main thread, so you need to be both fast and threadsafe.
- `SimpleClient::SimpleClient_SetStartupInfo` - Called from the connecting thread, this tells your client "here's the map data we just got from the server.  Deal with it".  This would be a good time to start building 3D models of the map, telling your user the map name and whatnot.
- `SimpleClient::Connect()` - This does the actual connecting to a SimpleServer.  It gets the underlying game to build a `ClientDesc` with `SimpleClient_BuildDesc` and an initial state (`ClientState` from `SimpleClient_BuildState`), then fires those off to the server via TCP.  If connecting is successful, it sends an initial `ClientState` over TCP (so the server has SOMETHING to go off of in case it takes a while for the client to send another packet), then loads the `ClientStartupInfo` (aka map data) and sends it to the client via `SimpleClient_SetStartupInfo`.  Once the client has been told, the connect thread fires up two threads: one for UDP transmissions created from `SimpleClient_BuildState`, and one for two-way TCP communication.  These threads run the `_SendThreadProc` and `_RecvThreadProc` functions.
- `SimpleClient::SimpleClient_NotifyGameState` - Called when the TCP receive thread has received a new packet of data from the server.  Each TDGGameState includes a list of players and a list of their new positions, wattages, etc.  So you might get a `TDGGameState` like "playerids {2, 5, 8} now have power {280, 305, 150} and are at positions {1.3km, 1.5km, 0.8km}".  It is up to the client to update its internal state.
- `SimpleClient::SimpleClient_GetSpecialData` - Called from the TCP receive thread to ask if you have any special messages for the server that need to go over TCP.  See `TDGClientToServerSpecial` in TourDeGiroCommon/CommStructs.h to the current usage.  This is not actually _that_ important of a thing to have implemented.
- `SimpleClient::SimpleClient_ShouldReconnect` - If your client suspects that it needs to reconnect for some reason (corrupted data? user paused it? I dunno), return true and the various SimpleClient threads will make that happen.

##### SimpleServer functions
SimpleServer docs may occur at some point in the future if there is demand.