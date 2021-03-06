#include "Stdafx.h"
#include "FootBallPitch.h"
#include "FootBallTeam.h"
#include "Goal.h"
#include "TimeCount.h"
#include "SpotCalculator.h"
#include "Steering.h"
#include "Entity/BasePlayer.h"
#include "Entity/FootBall.h"
#include "Entity/GoalKeeper.h"
#include "Entity/FootBaller.h"
#include "Entity/EntityManager.h"
#include "StateAi/StateMachine.h"
#include "Messageing/MessageDispatcher.h"
#include "../Public/Singleton.h"
#include "../Render/VGdi.h"
#include "../Render/Vector2D.h"
#include "../Render/MathGeo.h"

const int NumRegionsHorizontal = 12; 
const int NumRegionsVertical   = 4;

FootBallPitch::FootBallPitch(int cx, int cy):m_cxClient(cx),
	m_cyClient(cy),
	m_bPaused(false),
	m_bGoalKeeperHasBall(false),
	m_Regions(NumRegionsHorizontal*NumRegionsVertical),
	m_bGameOn(true)
{
	m_pPlayingArea = new Region(60, 30, cx-60, cy-30);

	CreateRegions(PlayingArea()->Width() / (double)NumRegionsHorizontal,
		PlayingArea()->Height() / (double)NumRegionsVertical);

	m_pRedGoal  = new Goal(Vector2D( m_pPlayingArea->Left(), (cy-GetInstObj(CGameSetup).GoalWidth)/2),
		Vector2D(m_pPlayingArea->Left(), cy - (cy-GetInstObj(CGameSetup).GoalWidth)/2),
		Vector2D(1,0));


	m_pBlueGoal = new Goal( Vector2D( m_pPlayingArea->Right(), (cy-GetInstObj(CGameSetup).GoalWidth)/2),
		Vector2D(m_pPlayingArea->Right(), cy - (cy-GetInstObj(CGameSetup).GoalWidth)/2),
		Vector2D(-1,0));

	m_pBall = new FootBall(Vector2D((double)m_cxClient/2.0, (double)m_cyClient/2.0),
		GetInstObj(CGameSetup).BallSize,
		GetInstObj(CGameSetup).BallMass,
		m_vecWalls);

	m_pRedTeam  = new FootBallTeam(m_pRedGoal, m_pBlueGoal, this, FootBallTeam::red);
	m_pBlueTeam = new FootBallTeam(m_pBlueGoal, m_pRedGoal, this, FootBallTeam::blue);

	m_pRedTeam->SetOpponents(m_pBlueTeam);
	m_pBlueTeam->SetOpponents(m_pRedTeam); 

	Vector2D TopLeft(m_pPlayingArea->Left(), m_pPlayingArea->Top());                                        
	Vector2D TopRight(m_pPlayingArea->Right(), m_pPlayingArea->Top());
	Vector2D BottomRight(m_pPlayingArea->Right(), m_pPlayingArea->Bottom());
	Vector2D BottomLeft(m_pPlayingArea->Left(), m_pPlayingArea->Bottom());

	m_vecWalls.push_back(Wall2D(BottomLeft, m_pRedGoal->RightPost()));
	m_vecWalls.push_back(Wall2D(m_pRedGoal->LeftPost(), TopLeft));
	m_vecWalls.push_back(Wall2D(TopLeft, TopRight));
	m_vecWalls.push_back(Wall2D(TopRight, m_pBlueGoal->LeftPost()));
	m_vecWalls.push_back(Wall2D(m_pBlueGoal->RightPost(), BottomRight));
	m_vecWalls.push_back(Wall2D(BottomRight, BottomLeft));

}

FootBallPitch::~FootBallPitch()
{
	SAFE_DELETE( m_pBall );

	SAFE_DELETE( m_pRedTeam );
	SAFE_DELETE( m_pBlueTeam);

	SAFE_DELETE( m_pRedGoal );
	SAFE_DELETE( m_pBlueGoal);

	SAFE_DELETE( m_pPlayingArea );

	for (unsigned int i=0; i<m_Regions.size(); ++i)
	{
		SAFE_DELETE( m_Regions[i] );
	}
}

void FootBallPitch::Update()
{
	if (m_bPaused) return;

	static int tick = 0;

	m_pBall->Update();

	m_pRedTeam->Update();
	m_pBlueTeam->Update();

	if (m_pBlueGoal->Scored(m_pBall) || m_pRedGoal->Scored(m_pBall))
	{
		m_bGameOn = false;

		m_pBall->PlaceAtPosition(Vector2D((double)m_cxClient/2.0, (double)m_cyClient/2.0));

		m_pRedTeam->GetFSM()->ChangeState(&GetInstObj(PrepareForKickOff));
		m_pBlueTeam->GetFSM()->ChangeState(&GetInstObj(PrepareForKickOff));
	}
}

void FootBallPitch::CreateRegions(double width, double height)
{  
	int idx = m_Regions.size()-1;

	for (int col=0; col<NumRegionsHorizontal; ++col)
	{
		for (int row=0; row<NumRegionsVertical; ++row)
		{
			m_Regions[idx--] = new Region(PlayingArea()->Left()+col*width,
				PlayingArea()->Top()+row*height,
				PlayingArea()->Left()+(col+1)*width,
				PlayingArea()->Top()+(row+1)*height,
				idx);
		}
	}
}


bool FootBallPitch::Render()
{
	//render regions
	if (GetInstObj(CGameSetup).bRegions)
	{   
		for (unsigned int r=0; r<m_Regions.size(); ++r)
		{
			m_Regions[r]->Render(true);
		}
	}

	//GetInstObj(CGDI).HollowBrush();
	GetInstObj(CGDI).RedPen();
	GetInstObj(CGDI).HatchRBrush();
	GetInstObj(CGDI).Rect(m_pPlayingArea->Left()-40, (m_cyClient-GetInstObj(CGameSetup).GoalWidth)/2, m_pPlayingArea->Left(), m_cyClient - (m_cyClient-GetInstObj(CGameSetup).GoalWidth)/2);
	GetInstObj(CGDI).Rect(m_pPlayingArea->Left()-39, (m_cyClient-GetInstObj(CGameSetup).GoalWidth)/2+1, m_pPlayingArea->Left()-1, m_cyClient - (m_cyClient-GetInstObj(CGameSetup).GoalWidth)/2-1);	
	GetInstObj(CGDI).HatchXBrush();
	GetInstObj(CGDI).Rect(m_pPlayingArea->Left(), (m_cyClient-GetInstObj(CGameSetup).GoalWidth)/2-100, m_pPlayingArea->Left()+100, m_cyClient - (m_cyClient-GetInstObj(CGameSetup).GoalWidth)/2+100);
	GetInstObj(CGDI).Rect(m_pPlayingArea->Left()+1, (m_cyClient-GetInstObj(CGameSetup).GoalWidth)/2-99, m_pPlayingArea->Left()+99, m_cyClient - (m_cyClient-GetInstObj(CGameSetup).GoalWidth)/2+99);


	GetInstObj(CGDI).BluePen();
	GetInstObj(CGDI).HatchLBrush();
	GetInstObj(CGDI).Rect(m_pPlayingArea->Right(), (m_cyClient-GetInstObj(CGameSetup).GoalWidth)/2, m_pPlayingArea->Right()+40, m_cyClient - (m_cyClient-GetInstObj(CGameSetup).GoalWidth)/2);
	GetInstObj(CGDI).Rect(m_pPlayingArea->Right()+1, (m_cyClient-GetInstObj(CGameSetup).GoalWidth)/2+1, m_pPlayingArea->Right()+39, m_cyClient - (m_cyClient-GetInstObj(CGameSetup).GoalWidth)/2-1);

	GetInstObj(CGDI).HatchXBrush();
	GetInstObj(CGDI).Rect(m_pPlayingArea->Right()-100, (m_cyClient-GetInstObj(CGameSetup).GoalWidth)/2-100, m_pPlayingArea->Right(), m_cyClient - (m_cyClient-GetInstObj(CGameSetup).GoalWidth)/2+100);
	GetInstObj(CGDI).Rect(m_pPlayingArea->Right()-99, (m_cyClient-GetInstObj(CGameSetup).GoalWidth)/2-99, m_pPlayingArea->Right()-1, m_cyClient - (m_cyClient-GetInstObj(CGameSetup).GoalWidth)/2+99);

	//render the pitch markings
	GetInstObj(CGDI).ClearBrush();

	GetInstObj(CGDI).WhitePen();
	GetInstObj(CGDI).Circle(m_pPlayingArea->Center(), m_pPlayingArea->Width() * 0.125);
	GetInstObj(CGDI).Line(m_pPlayingArea->Center().x, m_pPlayingArea->Top(), m_pPlayingArea->Center().x, m_pPlayingArea->Bottom());
	GetInstObj(CGDI).WhiteBrush();
	GetInstObj(CGDI).Circle(m_pPlayingArea->Center(), 3.0);


	//the ball
	GetInstObj(CGDI).WhitePen();
	GetInstObj(CGDI).WhiteBrush();
	m_pBall->Render();

	//Render the teams
	m_pRedTeam->Render();
	m_pBlueTeam->Render(); 

	//render the walls
	GetInstObj(CGDI).WhitePen();
	for (unsigned int w=0; w<m_vecWalls.size(); ++w)
	{
		m_vecWalls[w].Render();
	}

	//show the score
	GetInstObj(CGDI).TextColor(CGDI::red);
	GetInstObj(CGDI).TextAtPos((m_cxClient/2)-80, 10, "红队得分: " + ttos(m_pBlueGoal->NumGoalsScored()));

	GetInstObj(CGDI).TextColor(CGDI::blue);
	GetInstObj(CGDI).TextAtPos((m_cxClient/2)+50, 10, "蓝队得分: " + ttos(m_pRedGoal->NumGoalsScored()));

	return true;  
}