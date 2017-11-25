
#include "stdafx.h"
#include "Player.h"
#include "Map.h"

// sees what pTrailing is going to decide to do.  Should match iExpectedRetForTrailing
void DoMoveDirTest(const IConstPlayerPtrConst pLeading, const IConstPlayerPtrConst pTrailing, int iExpectedRetForTrailing)
{
  EXPECT_T(pLeading->GetDistance().flDistance >= pTrailing->GetDistance().flDistance); // sanity check: it's required that pLeading is ahead (at least in terms of in-lap position)

  vector<IConstPlayerPtrConst> lstDraft;
  lstDraft.push_back(pLeading);
  lstDraft.push_back(pTrailing);

  DraftMap mapDraft;
  ::BuildDraftingMap(lstDraft,mapDraft);

  const int iMoveDirResult = Player::GetMoveDir(1,lstDraft,mapDraft);
  EXPECT_T(iExpectedRetForTrailing == iMoveDirResult);
}


void DoMoveDirTests(boost::shared_ptr<const IMap> spMap)
{
  PlayerDraftCompare<void*> pdc(NULL);
  const DWORD tmNow = ArtGetTime();
  IPlayerPtr spLeading(new Player(tmNow,0,-1,"Leading",spMap,0,AISELECTION()));
  IPlayerPtr spTrailing(new Player(tmNow,0,-1,"Trailing",spMap,0,AISELECTION()));
  IPlayerPtr spOther(new Player(tmNow,0,-1,"Other",spMap,0,AISELECTION()));
  
  DraftMap mapDraft;

  Player* pLeading = (Player*)spLeading.get();
  Player* pTrailing = (Player*)spTrailing.get();
  Player* pOther = (Player*)spOther.get();
  
  pOther->m_flLane = 0;
  pOther->m_dist.flDistance = 0;

  vector<IConstPlayerPtrConst> lstDraft;
  lstDraft.push_back(spLeading);
  lstDraft.push_back(spTrailing);
  lstDraft.push_back(spOther);

  // Test #1: the trailing rider is "trapped" to the outside of the leading rider, but their collision boxes overlap.
  // Expected: The code should ignore the leading rider for blocking, and the trailing rider should move sideways to get into clear air
  pLeading->m_flLane = -3.42;
  pLeading->m_dist.flDistance = 2060.6492;
  pTrailing->m_flLane = -3.50;
  pTrailing->m_dist.flDistance = 2060.6458;
  sort(lstDraft.begin(),lstDraft.end(),pdc);
  EXPECT_T(Player::GetMoveDir(1,lstDraft,mapDraft) == Player::MOVEDIR_RIGHT);

  // Test #2a: Trailing rider is under 1 cyclist-length and slightly left (< 1.0 cyclist-width) of leading rider in middle of road.
  // expected: should move left
  pLeading->m_flLane = 0;
  pLeading->m_dist.flDistance = 2060.6492;
  pTrailing->m_flLane = -CYCLIST_WIDTH/4;
  pTrailing->m_dist.flDistance = 2060.6458;
  DoMoveDirTest(spLeading,spTrailing,Player::MOVEDIR_LEFT);
  pTrailing->m_flLane = CYCLIST_WIDTH/4;
  sort(lstDraft.begin(),lstDraft.end(),pdc);
  EXPECT_T(Player::GetMoveDir(1,lstDraft,mapDraft) == Player::MOVEDIR_RIGHT); // test #2b: mirror image of previous.  Rider should want to move opposite direction


  // Test #3: trailing rider is directly behind leading rider
  // expected: shouldn't want to move, since they're in a good drafting spot
  pLeading->m_flLane = 0;
  pLeading->m_dist.flDistance = 2000;
  pTrailing->m_flLane = 0;
  pTrailing->m_dist.flDistance = 2000 - CYCLIST_LENGTH - 0.1;
  sort(lstDraft.begin(),lstDraft.end(),pdc);
  EXPECT_T(Player::GetMoveDir(1,lstDraft,mapDraft) == Player::MOVEDIR_NOMOVE);

  // Test #4a: trailing rider is behind leading rider, but offset by 2m to the left
  // expected: rider should try to move right to get behind leading rider
  pLeading->m_flLane = 0;
  pLeading->m_dist.flDistance = 2000;
  pTrailing->m_flLane = -2;
  pTrailing->m_dist.flDistance = 2000 - CYCLIST_LENGTH - 0.1;
  sort(lstDraft.begin(),lstDraft.end(),pdc);
  EXPECT_T(Player::GetMoveDir(1,lstDraft,mapDraft) == Player::MOVEDIR_RIGHT);
  pTrailing->m_flLane = 2;
  sort(lstDraft.begin(),lstDraft.end(),pdc);
  EXPECT_T(Player::GetMoveDir(1,lstDraft,mapDraft) == Player::MOVEDIR_LEFT); // test 4b: mirror image

  // Test #5: trailing rider is overlapped with leading rider, offset by 2m to the left
  // expected: rider should do nothing, since there is nothing to do
  pLeading->m_flLane = 0;
  pLeading->m_dist.flDistance = 2000;
  pTrailing->m_flLane = -2;
  pTrailing->m_dist.flDistance = 2000 - CYCLIST_LENGTH/2;
  sort(lstDraft.begin(),lstDraft.end(),pdc);
  EXPECT_T(Player::GetMoveDir(1,lstDraft,mapDraft) == Player::MOVEDIR_NOMOVE);
  pTrailing->m_flLane = 2;
  sort(lstDraft.begin(),lstDraft.end(),pdc);
  EXPECT_T(Player::GetMoveDir(1,lstDraft,mapDraft) == Player::MOVEDIR_NOMOVE); // test 5b: mirror image

  // Test #6: trailing rider is well behind leading rider, offset by 2m to the left
  // expected: rider should do nothing, since he's too far back to care
  pLeading->m_flLane = 0;
  pLeading->m_dist.flDistance = 2000;
  pTrailing->m_flLane = -2;
  pTrailing->m_dist.flDistance = 2000 - 15;
  sort(lstDraft.begin(),lstDraft.end(),pdc);
  EXPECT_T(Player::GetMoveDir(1,lstDraft,mapDraft) == Player::MOVEDIR_NOMOVE);
  pTrailing->m_flLane = 2;
  sort(lstDraft.begin(),lstDraft.end(),pdc);
  EXPECT_T(Player::GetMoveDir(1,lstDraft,mapDraft) == Player::MOVEDIR_NOMOVE); // test 6b: mirror image

  // Test #7: rider is behind and to left of leading rider, but there's a third rider who is drafting the leader
  // exepected: rider should do nothing, since as of May 26 2014, we don't want aggressive lane-changes across people
  pLeading->m_flLane = 0;
  pLeading->m_dist.flDistance = 2000;
  pTrailing->m_flLane = -2;
  pTrailing->m_dist.flDistance = 2000 - CYCLIST_LENGTH*1.5f;
  pOther->m_flLane = 0;
  pOther->m_dist.flDistance = 2000 - CYCLIST_LENGTH*1.1f; // other rider is getting a better draft, and isn't a full cyclist length ahead of first rider
  sort(lstDraft.begin(),lstDraft.end(),pdc);
  EXPECT_T(Player::GetMoveDir(2,lstDraft,mapDraft) == Player::MOVEDIR_NOMOVE); // we're expecting a stable situation
  EXPECT_T(Player::GetMoveDir(1,lstDraft,mapDraft) == Player::MOVEDIR_NOMOVE);
  EXPECT_T(Player::GetMoveDir(0,lstDraft,mapDraft) == Player::MOVEDIR_NOMOVE);

  // test 7.1: move the blocker from test 7 to the side so that the leader's draft area is exposed
  // expect: both pOther and pTrailing should try to move to the center
  pOther->m_flLane = 2; // move the blocking guy to the side, see what happens
  EXPECT_T(Player::GetMoveDir(2,lstDraft,mapDraft) == Player::MOVEDIR_RIGHT); // now that the space is open, both players should try to get to the middle
  EXPECT_T(Player::GetMoveDir(1,lstDraft,mapDraft) == Player::MOVEDIR_LEFT); // now that the space is open, both players should try to get to the middle
  EXPECT_T(Player::GetMoveDir(0,lstDraft,mapDraft) == Player::MOVEDIR_NOMOVE); // leader should still stay still


  // Test #8: rider is behind and to left of rider ahead, who is also behind and to left of leader.  All 3 riders are overlapped (so there is no space to lane-change)
  // expected: rider should do nothing, since as of may 26 2014, we don't want aggressive lane-changes across people
  pLeading->m_flLane = 0;
  pLeading->m_dist.flDistance = 2000;
  pTrailing->m_flLane = CYCLIST_WIDTH*1.1;
  pTrailing->m_dist.flDistance = 2000 - CYCLIST_LENGTH*0.9f;
  pOther->m_flLane = CYCLIST_WIDTH*2.2;
  pOther->m_dist.flDistance = 2000 - CYCLIST_LENGTH*1.8f; 
  sort(lstDraft.begin(),lstDraft.end(),pdc);
  EXPECT_T(Player::GetMoveDir(2,lstDraft,mapDraft) == Player::MOVEDIR_NOMOVE); // we're expecting a stable situation - there's no space for any of the riders to move
  EXPECT_T(Player::GetMoveDir(1,lstDraft,mapDraft) == Player::MOVEDIR_NOMOVE);
  EXPECT_T(Player::GetMoveDir(0,lstDraft,mapDraft) == Player::MOVEDIR_NOMOVE);

  // test #9: same echelon formation as test 8, but each rider is fully behind the one ahead, and thus can move to get behind them
  pLeading->m_flLane = 0;
  pLeading->m_dist.flDistance = 2000;
  pTrailing->m_flLane = CYCLIST_WIDTH*1.1;
  pTrailing->m_dist.flDistance = 2000 - CYCLIST_LENGTH*1.1f;
  pOther->m_flLane = CYCLIST_WIDTH*2.2;
  pOther->m_dist.flDistance = 2000 - CYCLIST_LENGTH*2.2f; 
  sort(lstDraft.begin(),lstDraft.end(),pdc);
  EXPECT_T(Player::GetMoveDir(2,lstDraft,mapDraft) == Player::MOVEDIR_LEFT); // we're expecting a stable situation - there's no space for any of the riders to move
  EXPECT_T(Player::GetMoveDir(1,lstDraft,mapDraft) == Player::MOVEDIR_LEFT);
  EXPECT_T(Player::GetMoveDir(0,lstDraft,mapDraft) == Player::MOVEDIR_NOMOVE); // leader still doesn't move


  
  { // test #10: a bunch of riders on top of each other.  They should attempt to move out of the way
    const char* rgNames[] = {"Savey96","Surgey90","Helpy99","Dopey98","Hillman99","Surgey92"};
    const float rgLanes[] = {-1.13,-1.09,-1.061,-1.062,-1.05,-1};
    const float rgDist[] = {76.7,76.97,77.06,76.94,76.97,76.99};
    DraftMap mapDraft;
    vector<IConstPlayerPtrConst> lstDraft;
    for(int x = 0;x < 6; x++)
    {
      IPlayerPtr sp1(new Player(tmNow,0,-1,rgNames[x],spMap,0,AISELECTION()));
      Player* p1 = (Player*)sp1.get();
      p1->m_flLane = rgLanes[x];
      p1->m_dist.flDistance = rgDist[x];
      lstDraft.push_back(sp1);
    }
    
    sort(lstDraft.begin(),lstDraft.end(),pdc);
    EXPECT_T(Player::GetMoveDir(5,lstDraft,mapDraft) == Player::MOVEDIR_LEFT); // savey96, being the leftmost and lastmost rider, should try to move left
  }
}

void Player::DoTests()
{
  boost::shared_ptr<const IMap> spMap(new Map());
  Map* pMap = (Map*)spMap.get();
  pMap->LoadFromSine(10, 2, -1);
  DoMoveDirTests(spMap);
}