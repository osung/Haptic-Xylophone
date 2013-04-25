#pragma comment( linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"" )

#include <stdlib.h>

#include <iostream>
#include <vector>
#include <queue>
#include "chai3d.h"

#include <FTGL/ftgl.h>

//#define FULL_SCREEN

#define PROXY_RADIUS     0.1
#define MAX_VIB          0.2                  // 0.23
#define FRQ_VIB          25                   // 20
#define MAX_VOLUME       0.2

#define MAX_ITEM         2

#define SPHERE_RADIUS    0.2

#define MAX_VEL          30

#define OBJ_PLATE        0x0001
#define OBJ_STICK        0x0002
#define OBJ_ITEM         0x0003
#define OBJ_WOOD         0x0004
#define OBJ_BUBBLE       0x0008
#define OBJ_RUBBER       0x0010


#define ITEM_STAY_TIME   10             // in second

#define ITEM_MAGNET      0x0101
#define ITEM_SLOW        0x0102
#define ITEM_GENEROUS    0x0103

#define ITEM_NEG_MAGNET  0x0111
#define ITEM_NEG_VISCO   0x0112
#define ITEM_FAST        0x0113

#define EFFECT_DURATION  1000

#define NOTE_PREP_TIME   2.0             // in second

#define EFFECT_NEGMAGNET 1

#define Z_COORD          -9

#define HIT_GOOD_START     0.9
#define HIT_GOOD_END       1.1
#define HIT_GREAT_START    0.95
#define HIT_GREAT_END      1.05
#define HIT_EXCELLENT_START 0.98
#define HIT_EXCELLENT_END   1.02

#define WRONG_HIT_SCORE    -100

using namespace std;

extern void initMusic();
extern void playMusic(int num, float volume);
extern void endMusic();

// initial size (width/height) in pixels of the display window
const int WINDOW_SIZE_WIDTH			= 1280;
const int WINDOW_SIZE_HEIGHT		= 800;


cBitmap *g_logo = NULL;

FTPixmapFont g_font("verdana.ttf");
//FTExtrudeFont t_font("verdanaz.ttf");
FTPolygonFont t_font("verdanaz.ttf");

vector<cColorf> g_colors;

int g_itemNum = 0;
bool g_useItem = false;

int g_effectMode = 0;
double g_effectStartTime = 0;

int g_score = 0;
int g_combo = 0;
int g_cnt[4];  // 0: excellent, 1: great, 2: good, 3: bad

// information about the current haptic device
cHapticDeviceInfo						g_hapticDeviceInfo;

struct wrongHit
{
	int tone;
	double time;
};


typedef struct _judgeText
{
	float position[3];
	float color[3];
	string text;
	float time;
} judgeText;

deque<judgeText *> jtQueue;

int g_totalWrongHits = 0;
int g_totalNotes = 0;
int g_correctHits = 0;

queue<wrongHit> g_wrongHit;

bool g_pan = false;

// a world contains all objects of the virtual environment
cWorld*						g_world;

// a camera renders the world in a window display
cCamera*					g_camera;

cLight *                    g_light;

// width and height of the current window display
int								g_displayWidth   = 0;
int								g_displayHeight  = 0;

// a haptic device handler
cHapticDeviceHandler*					g_hapticDeviceHandler;


// a virtual tool representing the haptic device in the scene
cGeneric3dofPointer*					g_tool;


// status of the main simulation haptic loop
bool								g_simulationRunning = false;
bool								g_simulationFinished = false;

cVector3d							g_prevMousePos;
int									g_cameraMode = 0;

double g_workspaceScaleFactor;
double g_dampingMax;

double								g_updateRateGraphicLoop, g_prevTimeGraphicLoop;
double								g_updateRateHapticLoop, g_prevTimeHapticLoop;

cPrecisionClock						g_precisionClock;
bool                                g_hapticUse = true;

// Update rate
double g_updateRateGraphicElapsedTime = 0.0;
double g_averageUpdateRateGraphicLoop = 1.0;
int	   g_graphicRenderingCounter = 0;

double g_updateRateHapticElapsedTime = 0.0;
double g_averageUpdateRateHapticLoop = 1.0;
int	   g_hapticRenderingCounter = 0;


float rect[8][3] = {{-0.22, -1.55, -1},   {-0.22, -1.55, 1},   {0.22, -1.55, 1},   {0.22, -1.55, -1},
                    {-0.22, -1.6, -1}, {-0.22, -1.6, 1}, {0.22, -1.6, 1}, {0.22, -1.6, -1}};
          

float brect[8][3] = {{-0.2, -1.35, -0.9},   {-0.2, -1.35, 1.03},   {0.22, -1.35, 1.03},   {0.22, -1.35, -0.9},
                    {-0.2, -2.16, -0.9}, {-0.2, -2.16, 1.03}, {0.22, -2.16, 1.03}, {0.22, -2.16, -0.9}};




bool g_gameStart = false;
bool g_gamePrepare = false;
int g_nextNote = 0;
double g_gameStartTime = 0;
float g_tempo = 0.15; //0.125;

judgeText * genJudgeText(int idx, float x, float y, float z, float time);


class cNoteSphere : public cShapeSphere
{
public :
	cNoteSphere(double r):cShapeSphere(r)
	{
		m_tempo = 0;
		m_startTime = 0;
		m_index = 0;

		this->disable();
  	    this->setAsGhost(true);
	};

	virtual ~cNoteSphere() { };

	void start(int index, double time)
	{
		double xpos = index*0.512-4.0;
		this->setPos(xpos, 7, Z_COORD);
		this->computeGlobalPositions();

		m_startPos.set(xpos, 7, Z_COORD);
		m_startTime = time;

		m_start = true;
		m_index = index;
		this->setGoal(xpos, -0.5, Z_COORD+10.2);

		if (index < g_colors.size()) this->m_material.m_diffuse.setMem4(g_colors[index].pColor());
	}
	

	void disable()
	{
		m_start = false;
		this->setPos(0, 30, 0);
		m_goal = false;
		this->computeGlobalPositions();
		m_index = 0;
		m_Hitable = false;
	};


	void setGoal(double x, double y, double z)
	{
		m_goalPos.set(x, y, z);
	};


	cVector3d getGoal()
	{
		return m_goalPos;
	}


	void setTempo(float tempo)
	{
		m_tempo = tempo;
		m_inv_tempo = 1 / m_tempo;
	};


	void move(double currenttime)
	{
		double t = currenttime - m_startTime;

		m_localPos.x = t * m_inv_tempo * (m_goalPos.x - m_startPos.x) + m_startPos.x;
		m_localPos.y = t * m_inv_tempo * (m_goalPos.y - m_startPos.y) + m_startPos.y; 
		m_localPos.z = t * m_inv_tempo * (m_goalPos.z - m_startPos.z) + m_startPos.z;

		this->computeGlobalPositions();

		// check if it is hitable
		if (t >= m_tempo * HIT_GOOD_START && m_Hitable == false)
		{
			m_Hitable = true;
			g_totalNotes++;
		}
		else if (t >= m_tempo * HIT_GOOD_END)
		{
			m_Hitable = false;
			m_goal = true;
		}
	};

	int getScore(double currenttime)
	{
		double t = currenttime - m_startTime;
		cVector3d pos = g_tool->m_proxyPointForceModel->m_contactPoint0->m_globalPos;
		int combo = g_combo > 1 ? g_combo : 1;
		
		if (m_Hitable == false) // if bad
		{
			g_cnt[3]++;
			judgeText *jt = genJudgeText(3, pos.x, pos.y, pos.z, currenttime);
			jtQueue.pop_front();
			jtQueue.push_back(jt);	
			g_combo = 0;
			return -100;
		}
		else if (t >= m_tempo * HIT_EXCELLENT_START && t <= m_tempo * HIT_EXCELLENT_END) // if excellent
		{
			g_cnt[0]++;
			judgeText *jt = genJudgeText(0, pos.x, pos.y, pos.z, currenttime);
			jtQueue.pop_front();
			jtQueue.push_back(jt);
			g_combo++;
			return 200 * g_combo;
		}
		else if (t >= m_tempo * HIT_GREAT_START && t <= m_tempo * HIT_GREAT_END)
		{
			g_cnt[1]++;
			judgeText *jt = genJudgeText(1, pos.x, pos.y, pos.z, currenttime);
			jtQueue.pop_front();
			jtQueue.push_back(jt);
			g_combo++;
			return 100 * g_combo;
		}
		else 
		{
			g_cnt[2]++;
			judgeText *jt = genJudgeText(2, pos.x, pos.y, pos.z, currenttime);
			jtQueue.pop_front();
			jtQueue.push_back(jt);
			g_combo++;
			return 50 * g_combo;
		}
	};
	
	bool checkGoal()
	{
		return m_goal;
	}

	int getIndex()
	{
		return m_index;
	}

	bool m_start;


protected :
	
   // actual time of rhythm in ms
   float m_tempo;
   float m_inv_tempo;

   double m_startTime;
   bool m_goal;
   int m_index;
   bool m_Hitable;

   cVector3d m_goalPos, m_startPos;
};



class cPlateMesh : public cMesh
{
public :
	
	cPlateMesh(cWorld *a_world):cMesh(a_world)
	{ 
	   m_userData = (void *) &m_contact;
	   m_contact = false;	
	   m_sound = -1;
	};
	
	virtual ~cPlateMesh()  {};

	int  m_sound;

protected :

	bool m_contact;

};



struct note {
	int tone;
	int length;
};



// Plate
cPlateMesh*                              g_plate[16];
cMesh*                                   g_border[2];

deque<cNoteSphere*> g_sphere;
queue<cNoteSphere*> g_spares;

vector<note> * g_scores = NULL;


void initGame()
{
	g_score = 0;
	g_combo = 0;
	g_nextNote = 0;

	for (int i = 0; i < 4; ++i)
	{
		g_cnt[i] = 0;
	}
	
	if (g_scores != NULL)
	{
		g_scores->clear();
		delete g_scores;
	}

	while (g_sphere.size() > 0)
	{
		cNoteSphere *sphere = g_sphere.front();
		sphere->disable();
		g_spares.push(sphere);
		g_sphere.pop_front();
	}

	/*
	while(g_wrongHit.size() > 0) g_wrongHit.pop();
	g_totalWrongHits = 0;
	g_totalNotes = 0;*/
}


void RemoveLogo()
{
	if (g_logo == NULL)
	{
		return;
	}
	g_camera->m_front_2Dscene.removeChild(g_logo);

	delete g_logo;
	g_logo = NULL;
}



void RegisterLogo(const char *name)
{
	RemoveLogo();

	// create a 2D bitmap logo
    g_logo = new cBitmap();

    // add logo to the front plane
    g_camera->m_front_2Dscene.addChild(g_logo);

    // load a bitmap image file
    g_logo->m_image.loadFromFile(name);

	// position the logo at the bottom left of the screen (pixel coordinates)
	g_logo->setPos(WINDOW_SIZE_WIDTH - 300, WINDOW_SIZE_HEIGHT - 200, 0);

	// scale the logo along its horizontal and vertical axis
	g_logo->setZoomHV(0.8, 0.8);
}


judgeText *
genJudgeText(int idx, float x, float y, float z, float time)
{
	judgeText *ret = new judgeText;

	ret->position[0] = x*2;
	ret->position[1] = y+1;
	ret->position[2] = z;
	ret->time = time;

	switch (idx)
	{
	case 0 : // excellent
		ret->color[0] = 1.0;   ret->color[1] = 0.0;   ret->color[2] = 0.0;  // red
		ret->text.assign("Excellent");
	break;

	case 1: // great
		ret->color[0] = 1.0;   ret->color[1] = 1.0;   ret->color[2] = 0.0; // yellow
		ret->text.assign("Great");
	break;

	case 2: // good
		ret->color[0] = 0.1;   ret->color[1] = 1.0;   ret->color[2] = 0.2; // green
		ret->text.assign("Good");
	break;

	case 3 : // bad
	default :
		ret->color[0] = 0.2;   ret->color[1] = 0.2;   ret->color[2] = 0.2; // dark grey
		ret->text.assign("Bad");
	break;
	}

	return ret;
}


cMesh *genBorder(float zScale)
{
	cMesh * border = new cMesh(g_world);

	for (int i = 0; i < 8; ++i)
	{
		border->newVertex(brect[i][0], brect[i][1]+1.27, brect[i][2]*zScale+0.5);
	}

	border->newTriangle(0, 1, 2);
	border->newTriangle(2, 3, 0);

	border->newTriangle(4, 5, 6);
	border->newTriangle(6, 7, 4);

	border->newTriangle(0, 3, 7);
	border->newTriangle(3, 7, 4);

	border->newTriangle(3, 2, 6);
	border->newTriangle(3, 6, 7);

	border->newTriangle(2, 1, 5);
	border->newTriangle(6, 2, 5);

	border->newTriangle(1, 0, 4);
	border->newTriangle(1, 4, 5);

	return border;
}




cPlateMesh * genPlate(double zScale)
{
	cPlateMesh * plate = new cPlateMesh(g_world);

	for (int i = 0; i < 8; ++i)
	{
		plate->newVertex(rect[i][0], rect[i][1]+1.27, rect[i][2]*zScale+0.5);
	}

	plate->newTriangle(0, 1, 2);
	plate->newTriangle(2, 3, 0);
	plate->newTriangle(4, 5, 6);
	plate->newTriangle(6, 7, 4);
	plate->newTriangle(0, 3, 7);
	plate->newTriangle(3, 7, 4);
	plate->newTriangle(3, 2, 6);
	plate->newTriangle(3, 6, 7);
	plate->newTriangle(2, 1, 5);
	plate->newTriangle(6, 2, 5);
	plate->newTriangle(1, 0, 4);
	plate->newTriangle(1, 4, 5);

	return plate;
}



cNoteSphere * genSphere(double radius)
{
	double stiffnessMax = g_hapticDeviceInfo.m_maxForceStiffness / g_workspaceScaleFactor;
	double forceMax = g_hapticDeviceInfo.m_maxForce / g_workspaceScaleFactor;

	cNoteSphere *sphere = new cNoteSphere( radius );

	g_world->addChild( sphere );

	sphere->setHapticEnabled(false);

	return sphere;
}



void loadXylophone()
{
	double stiffnessMax = g_hapticDeviceInfo.m_maxForceStiffness / g_workspaceScaleFactor;

	for (int i = 0; i <16; ++i)  // 16
	{
		g_plate[i] = genPlate((15-i)*0.06+1.0);
	    g_plate[i]->setPos(i*0.512-4.0, 0, Z_COORD+10);
		g_plate[i]->m_sound = i;
		g_plate[i]->setTag(OBJ_PLATE);
		g_plate[i]->computeBoundaryBox(true);
		g_plate[i]->createAABBCollisionDetector(PROXY_RADIUS, true, false);
		g_plate[i]->setTransparencyLevel(0);
		g_plate[i]->rotate(cVector3d(1, 0, 0), CHAI_DEG2RAD * 30);
		g_plate[i]->computeGlobalPositions();

	    g_plate[i]->setStiffness(stiffnessMax*0.8);		
	   // g_plate[i]->m_material.setMagnetMaxDistance( 1.0 );
		//g_plate[1]->m_material.setMagnetMaxForce(g_hapticDeviceInfo.m_maxForce * 0.5);
		g_plate[i]->m_material.setVibrationFrequency(FRQ_VIB);
		g_plate[i]->m_material.setVibrationAmplitude(0);

		//g_plate[i]->m_material.setViscosity(g_hapticDeviceInfo.m_maxLinearDamping / g_workspaceScaleFactor);
	
	    g_world->addChild(g_plate[i]);		
/*		
		cGenericEffect *newEffect = new cEffectSurface(g_plate[i]);
		g_plate[i]->addEffect(newEffect);

		cEffectMagnet *newEffect2 = new cEffectMagnet(g_plate[i]);
		g_plate[i]->addEffect((cGenericEffect *) newEffect2);
	
		cGenericEffect* newEffect3 = new cEffectVibration(g_plate[i]);
        g_plate[i]->addEffect(newEffect3);
		*/
		//cGenericEffect * newEffect4 = new cEffectViscosity(me);
		//me->addEffect(newEffect);

		//g_plate[i]->useDisplayList(true);
	}

	g_border[0] = genBorder(1.98);
	g_border[1] = genBorder(0.94);

	for (int i = 0; i < 2; ++i)
	{
        g_border[i]->setTag(OBJ_WOOD);
		g_border[i]->computeBoundaryBox(true);
		g_border[i]->createAABBCollisionDetector(PROXY_RADIUS, true, false);
		//g_border[i]->computeAllNormals();
		g_border[i]->setTransparencyLevel(0);
		g_border[i]->rotate(cVector3d(1, 0, 0), CHAI_DEG2RAD * 30);

	    g_border[i]->setStiffness(stiffnessMax * 0.3);		
		//g_border[i]->m_material.setVibrationFrequency(FRQ_VIB);
		//g_border[i]->m_material.setVibrationAmplitude(0);

	    g_world->addChild(g_border[i]);		
		
		cGenericEffect *newEffect = new cEffectSurface(g_border[i]);
		g_border[i]->addEffect(newEffect);

///		cGenericEffect* newEffect3 = new cEffectVibration(g_border[i]);
//        g_border[i]->addEffect(newEffect3);
	}
	
	g_border[0]->setPos(-1*0.512-4.1, 0, Z_COORD+10);
	g_border[1]->setPos(16*0.512-4.0, 0, Z_COORD+10);
	g_border[0]->computeGlobalPositions();
	g_border[1]->computeGlobalPositions();
	
	
	cMesh *fancy = new cMesh(g_world);
	
    fancy->loadFromFile("xylophone.3ds");
	fancy->setPos(-3, -2.0, Z_COORD+12.0);
	double size = cSub(fancy->getBoundaryMax(), fancy->getBoundaryMin()).length();
	fancy->scale(10 / size); //g_tool->getWorkspaceRadius() / size);
	fancy->rotate(cVector3d(1, 0, 0), CHAI_DEG2RAD * 30);
    fancy->rotate(cVector3d(1, 0, 0), CHAI_DEG2RAD * -90);
	fancy->m_material.m_diffuse.set( 1.0, 0.0, 1.0 );
	fancy->m_material.m_specular.set(1.0, 1.0, 1.0);
	fancy->m_material.setShininess(100);
	fancy->deleteCollisionDetector(true);
	fancy->setAsGhost(true);
	fancy->computeGlobalPositions();
	fancy->useDisplayList(true);
	fancy->setTransparencyLevel(1.0f);

	g_world->addChild(fancy);

	cMesh *stick = new cMesh(g_world);

	stick->loadFromFile("stick.3ds");
	stick->scale(1.6);
	stick->setPos(0.8, -2.7, Z_COORD + 8.8); //-0.2);
	stick->rotate(cVector3d(0, 1, 0), CHAI_DEG2RAD * 180);
	stick->deleteCollisionDetector(true);

	g_tool->m_proxyMesh->addChild(stick); 
}



void addNewNote(int index, double currentTime)
{
   cNoteSphere *sphere;

   if (g_spares.size() > 0)
   {
	   sphere = g_spares.front();
	   g_spares.pop();
   }
   else
   {
      sphere = genSphere(SPHERE_RADIUS);
   }

   sphere->setTempo(NOTE_PREP_TIME);
   sphere->setHapticEnabled(true);
   sphere->start((*g_scores)[g_nextNote].tone, currentTime);
   g_sphere.push_back(sphere);
}



void processEffect()
{
	switch(g_effectMode)
	{
	case ITEM_MAGNET :
		break;

	case ITEM_FAST :
		g_tempo -= 0.5;
		break;

	case ITEM_SLOW :
		g_tempo += 0.5;
		break;

	case ITEM_NEG_MAGNET :
		break;

	case ITEM_NEG_VISCO :
		break;
	}
}


void disableEffect()
{
	switch(g_effectMode)
	{
	case ITEM_MAGNET :
		//g_magnetSphere->disable();
		break;

	case ITEM_FAST :
		g_tempo += 0.5;
		break;

	case ITEM_SLOW :
		g_tempo -= 0.5;
		break;

	case ITEM_NEG_MAGNET :
		break;

	case ITEM_NEG_VISCO :
		break;
	}

	g_effectMode = 0;
}


void hapticRenderingLoop(void)
{
	static int contactID = -1;
	static double prevHitTime = 0;
	static cGenericObject *oldObject = NULL;

	while( g_simulationRunning )
	{
		double currentTime = g_precisionClock.getCurrentTimeSeconds();
		double diffTime = currentTime - prevHitTime;
		static float oldMaxVib = 0;
		
		if (g_gameStart == true)
		{
			// if first play
			if (g_gameStartTime == 0) 
			{
				g_gameStartTime = currentTime;

				addNewNote((*g_scores)[0].tone, currentTime);

				g_nextNote = 0;
			}
			else if (g_nextNote < (*g_scores).size() && currentTime - g_gameStartTime >= (*g_scores)[g_nextNote].length * g_tempo)
			{
				g_nextNote++;

				addNewNote((*g_scores)[g_nextNote].tone, currentTime);
				g_gameStartTime = currentTime;			
			}
			
			// check if the first note is at goal
            if (g_sphere.size() > 0)
			{
			   if (g_sphere.front()->checkGoal() == true)
		       {
					cVector3d pos = g_sphere.front()->getGoal();
					g_sphere.front()->disable();
					g_spares.push(g_sphere.front());
					g_sphere.pop_front();
					g_score -= 200;
					g_combo = 0;
							
					judgeText *jt = genJudgeText(3, pos.x, pos.y, pos.z, currentTime);
					jtQueue.pop_front();
					jtQueue.push_back(jt);	

				   if (g_score < 0) g_score = 0;
		       }
			}
			else 
			{
				if (g_nextNote >= (*g_scores).size()) 
				{
					g_gameStart = false;
					g_gameStartTime = 0;
				}
			}
			
			// movement of note and check if it is at goal
			for (int i = 0; i < g_sphere.size(); ++i)
			{
				g_sphere[i]->move(currentTime);
			}

			if (jtQueue.size() > 0)
			{
				if (jtQueue.front()->time + 0.5 <= currentTime)
				{
					jtQueue.pop_front();
				}
			}
/*
			// handle wrong hits
			while (g_wrongHit.size() > 0)
			{
				// if wrong hit was occured 0.1 sec before
				if (currentTime - g_wrongHit.front().time > 0.1)
				{
					g_plate[g_wrongHit.front().tone]->setStiffness(g_hapticDeviceInfo.m_maxForceStiffness / g_workspaceScaleFactor);
					g_wrongHit.pop();
				}
				else
				{
					break;
				}
			}
			*/
		}

		g_updateRateHapticLoop = currentTime - g_prevTimeHapticLoop;
		g_updateRateHapticElapsedTime += g_updateRateHapticLoop;
		g_prevTimeHapticLoop = currentTime;

        // update position and orientation of tool
		g_tool->updatePose();

		// compute interaction forces
		g_tool->computeInteractionForces();

		//g_negMagnet->checkTimer(currentTime);

		cGenericObject* objectContact = g_tool->m_proxyPointForceModel->m_contactPoint0->m_object;
		if (objectContact != NULL) // if contact
		{           
			if (objectContact->m_tag == OBJ_PLATE)
			{
				cPlateMesh *pm = (cPlateMesh *) objectContact;

			   // if new hit
			   if (diffTime > 0.1 && pm->m_sound != contactID)
			   {
					// if game is started
					if (g_gameStart == true && g_sphere.size() > 0)
					{
						// if wrong hit
						if (g_sphere.front()->getIndex() != pm->m_sound)
						{
							g_score += WRONG_HIT_SCORE;
							g_cnt[3]++;
							if (g_score < 0) g_score = 0;
							g_combo = 0;

							cVector3d pos = g_tool->m_proxyPointForceModel->m_contactPoint0->m_globalPos;
							judgeText *jt = genJudgeText(3, pos.x, pos.y, pos.z, currentTime);
							jtQueue.pop_front();
							jtQueue.push_back(jt);	
						}
						else
						{
							int score = g_sphere.front()->getScore(currentTime);

							g_score += score;
							g_sphere.front()->disable();
							g_sphere.pop_front();
						}
					}

				  float vel = g_tool->m_deviceLocalVel.length();
				  
			      playMusic(pm->m_sound, vel * 0.01f);
				  
				  if (oldObject != NULL)
				  {
					  oldObject->m_material.setVibrationAmplitude(0);
				  }
				  
				  objectContact->m_material.setVibrationAmplitude(MAX_VIB); 

				  //g_tool->m_lastComputedGlobalForce -= g_tool->m_deviceGlobalVel * 10000;
	   
	   			  contactID = pm->m_sound;
				  prevHitTime = currentTime;
				  oldObject = objectContact;
				  oldMaxVib = MAX_VIB * (vel > MAX_VEL ? 1 : vel / MAX_VEL);
			   }
			}
			else if (objectContact->m_tag == OBJ_WOOD)
			{
				cPlateMesh *pm = (cPlateMesh *) objectContact;

			   // if new hit
			   if (diffTime > 0.1 && contactID != OBJ_WOOD)
			   {
				  float vel = g_tool->m_deviceLocalVel.length();
				  
			      playMusic(16, vel * 0.01f);

				  if (oldObject != NULL)
				  {
					  oldObject->m_material.setVibrationAmplitude(0);
				  }
				  
				  objectContact->m_material.setVibrationAmplitude(MAX_VIB); 
				  
				  //g_tool->m_lastComputedGlobalForce -= g_tool->m_deviceGlobalVel * 10000;
	   
	   			  contactID = OBJ_WOOD;
				  prevHitTime = currentTime;
				  oldObject = objectContact;
				  oldMaxVib = MAX_VIB * (vel > MAX_VEL ? 1 : vel / MAX_VEL);
			   }
			} 
		}
		else if (diffTime > 0.1) contactID = -1;
		/*
		if (g_effectMode != 0)
		{
			if (currentTime - g_effectStartTime > EFFECT_DURATION)
			{
				disableEffect();
			}
		}

		// maintain vibrations
		if (oldObject != NULL)
		{
			if (diffTime <= 2)
			{
				oldObject->m_material.setVibrationAmplitude(oldMaxVib - oldMaxVib * diffTime * 0.5); 
			}
			else
			{
				oldObject->m_material.setVibrationAmplitude(0);
				oldObject = NULL;
			}
		}
		*/
		// send forces to device
		if (g_hapticUse == true)
		{
		   g_tool->applyForces();
		}

		if( g_updateRateHapticElapsedTime > 1.0 )
		{
			if( g_hapticRenderingCounter <= 0 )
			{
				g_hapticRenderingCounter = 1;
			}

			g_averageUpdateRateHapticLoop = g_updateRateHapticElapsedTime * 1000000 / g_hapticRenderingCounter;
			g_updateRateHapticElapsedTime = 0.0;
			g_hapticRenderingCounter = 0;
		}

		g_hapticRenderingCounter++;
	}

	g_simulationFinished = true;
}


void renderJudge()
{
    glEnable(GL_COLOR_MATERIAL);
	glMatrixMode(GL_MODELVIEW);

	glPushMatrix();
	glScalef(0.5f, 0.5f, 0.5f);

//	int size = jtQueue.size();

	//for (int i = 0; i < size; ++i)
	if (jtQueue.size() > 0)
	{
		glColor3fv(jtQueue.back()->color);
		glPushMatrix();
			glTranslatef(jtQueue.back()->position[0], jtQueue.back()->position[1], jtQueue.back()->position[2]);
			t_font.Render(jtQueue.back()->text.c_str());
		glPopMatrix();
	}

	glPopMatrix();

	glDisable(GL_COLOR_MATERIAL);
}



void renderText()
{
	char			buf[ 256 ];

	glDisable( GL_LIGHTING );

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(0.0, WINDOW_SIZE_WIDTH, 0.0, WINDOW_SIZE_HEIGHT);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glColor3f(0.8, 0.7, 0.2); // Yellow

	glRasterPos2i(10, 758);

	sprintf(buf, "Score : %d", g_score);
	g_font.Render(buf);

	glColor3f(0.9, 0.2, 0.0); // Red
	glRasterPos2i(10, 712);

	sprintf(buf, "Combo : %d", g_combo);
	g_font.Render(buf);

	const char *title[4] = {"Excellent", "Great", "Good", "Bad"};

	for (int i = 0; i < 4; ++i)
	{
		glColor3f(0.5, 0.5, 0.5); // White
		glRasterPos2i(10, 712-46*(i+1));

		sprintf(buf, "%s : %d", title[i], g_cnt[i]);
		g_font.Render(buf);
	}

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	glEnable( GL_LIGHTING );
}



void visualRenderingLoop(void)
{
	g_camera->renderView( g_displayWidth, g_displayHeight );

	renderText();
	renderJudge();

	glutSwapBuffers();

	double currentTime = g_precisionClock.getCurrentTimeSeconds();
	g_updateRateGraphicLoop = currentTime - g_prevTimeGraphicLoop;
	g_updateRateGraphicElapsedTime += g_updateRateGraphicLoop;
	g_prevTimeGraphicLoop = currentTime;

	if( g_updateRateGraphicElapsedTime > 1.0 )
	{
		if( g_graphicRenderingCounter <= 0 )
		{
			g_graphicRenderingCounter = 1;
		}

		g_averageUpdateRateGraphicLoop = g_updateRateGraphicElapsedTime * 1000.0 / g_graphicRenderingCounter;
		g_updateRateGraphicElapsedTime = 0.0;
		g_graphicRenderingCounter = 0;
	}

	GLenum		err;
	err = glGetError();
	if( err != GL_NO_ERROR ) 
		printf( "Error: %S\n", gluErrorString( err ) );

	if( g_simulationRunning )
	{
		glutPostRedisplay();
	}

	g_graphicRenderingCounter++;

}


// get tone index
int getTone(char tone)
{
	if (tone >= 'c')
	{
		return (int) (tone - 'c') + 1;
	}
	else if (tone >= 'a')
	{
		return (int) (tone - 'a') + 6;
	}
	else if (tone < 'H')
	{
		if (tone > 'B') return (int) (tone - 'C') + 8;
		else return (int) (tone - 'A') + 13;
	}
	else return 15;
}




vector<note> * getScore(const char *filename)
{
   string str;
   char buffer[256];
   vector<note> * score = new vector<note>;

   ifstream ifs (filename, ifstream::in);

   while(ifs.good())
   {
	   ifs.getline(buffer, 256);

	   if (buffer[0] != '#')
	   {
	      str.append(buffer);
	   }
   }

   ifs.close();

   for (int i = 0; i < str.size(); i+=2)
   {
	   note tmp;

	   tmp.tone = getTone(str[i]);
	   tmp.length = str[i+1]-'0';
	   if (tmp.length == 0) tmp.length = 12;
	   else if (tmp.length == 5) tmp.length = 10;

	   score->push_back(tmp);
   }

   return score;
}


void playScore(vector<note> *score)
{
	for (int i = 0; i < score->size(); ++i)
	{
		playMusic((*score)[i].tone, 0.2);
		Sleep((*score)[i].length*50);
	}

}


void keySelect( unsigned char key, int x, int y )
{
	switch( key )
	{
	case 27:   // Esc
//		close();
		exit( 0 );
		break;

	case 32:   // Space bar
		break;


	case '1':

		initGame();
		g_scores = getScore("butterfly.txt");
		RegisterLogo("butterfly.tga");
  		g_gameStart = true;
		break;

	case '2':
		initGame();
		g_scores = getScore("little_star.txt");
		RegisterLogo("little_star.tga");
  		g_gameStart = true;
		break;

	case '3':
		initGame();
		g_scores = getScore("3bears.txt");
		RegisterLogo("3bears.tga");
  		g_gameStart = true;
		break;


	case '4':
		initGame();
		g_scores = getScore("rudolf.txt");
		RegisterLogo("rudolf.tga");
  		g_gameStart = true;
		break;

	case '5':
		initGame();
		g_scores = getScore("cannon.txt");
		RegisterLogo("canon.tga");
  		g_gameStart = true;
		break;

	case '6':
		initGame();
		g_scores = getScore("arirang.txt");
		RemoveLogo();
  		g_gameStart = true;
		break;

	case '7':
		initGame();
		g_scores = getScore("house.txt");
		RemoveLogo();   		
  		g_gameStart = true;
		break;

	case '8':
		initGame();
		g_scores = getScore("rainbow.txt");
		RemoveLogo();
  		g_gameStart = true;
		break;

	case 'h':
	case 'H':
		g_hapticUse = !g_hapticUse;

		break;

	case '-' :
	case '_' :
		g_tempo += 0.02;
		break;

	case '+' :
	case '=' :
		g_tempo -= 0.02;
		break;

	case 'v':
		g_tool->m_material.setViscosity(10);
		break;

	case 'p' :
		g_pan = !g_pan;
		break;
	}

}




void resizeWindow(int width, int height)
{
	g_displayWidth  = width;
	g_displayHeight = height;
	glViewport( 0, 0, g_displayWidth, g_displayHeight );
}



void mouseEventCallback(int button, int state, int x, int y)
{


}




void mouseMotionCallback(int x, int y)
{



}



void initializeCamera()
{
	g_camera = new cCamera( g_world );
	g_world->addChild( g_camera );

	g_camera->setFieldViewAngle( 50);
	g_camera->set( cVector3d( 0.0, 0.0, 10.0 ),
		cVector3d( 0.0, 0, 0.0 ),
		cVector3d( 0.0, 1.0, 0.0 ) );

	g_camera->setClippingPlanes( 0.01, 100.0 );
	g_camera->computeGlobalPositions( true );
}


void initializeLight()
{
	// Create a light source
	g_light = new cLight( g_world );

	g_world->addChild( g_light );
	g_light->setEnabled(true);
	g_light->setPos( cVector3d( 2.0, 0.5, 1.0 ) );  
	g_light->setDir( cVector3d( -2.0, 0.5, 1.0 ) );
	
    float front_emission[4] = { 0.8, 0.1, 0.1, 0.0 };
    float front_ambient[4]  = { 0.8, 0.1, 0.1, 0.0 };
    float front_diffuse[4]  = { 0.95, 0.25, 0.1, 0.0 };
    float front_specular[4] = { 0.8, 0.8, 0.8, 0.0 };
    glMaterialfv(GL_FRONT, GL_EMISSION, front_emission);
    glMaterialfv(GL_FRONT, GL_AMBIENT, front_ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, front_diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, front_specular);
    glMaterialf(GL_FRONT, GL_SHININESS, 25.0);
    glColor4fv(front_diffuse);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);

    glColorMaterial(GL_FRONT, GL_DIFFUSE);
}



void initializeCHAI3D()
{
	//-----------------------------------------------------------------------
	// 3D - SCENEGRAPH
	//-----------------------------------------------------------------------	
	g_world = new cWorld();

	g_world->setBackgroundColor( 0.0, 0.0, 0.0 );
	g_world->setFrameSize( 1.0 );

	//g_Font = cFont::createFont();

	//-----------------------------------------------------------------------
	// HAPTIC DEVICES / TOOLS
	//-----------------------------------------------------------------------

	cGenericHapticDevice*		hapticDevice;
	g_hapticDeviceHandler = new cHapticDeviceHandler();
	g_hapticDeviceHandler->getDevice( hapticDevice, 0 );

	if( hapticDevice )
	{
		g_hapticDeviceInfo = hapticDevice->getSpecifications();
	}

	g_tool = new cGeneric3dofPointer( g_world );
	g_world->addChild( g_tool );

	// connect the haptic device to the tool
	g_tool->setHapticDevice( hapticDevice );
	
	// initialize tool by connecting to haptic device
	g_tool->start();
	
	// map the physical workspace of the haptic device to a larger virtual workspace.
	g_tool->setWorkspaceRadius(8); // 4

	// define a radius for the tool
	g_tool->setRadius( PROXY_RADIUS );

	// hide the device sphere. only show proxy.
	g_tool->m_deviceSphere->setShowEnabled( false );
	g_tool->m_proxySphere->setShowEnabled( false );
	g_tool->m_materialProxy.m_diffuse.set( 1.0, 0.0, 1.0 );
	g_tool->m_deviceSphere->m_material.m_diffuse.set( 0.0, 1.0, 0.0 );

	// set the physical radius of the proxy. for performance reasons, it is
	// sometimes useful to set this value to zero when dealing with
	// complex objects.
	g_tool->m_proxyPointForceModel->setProxyRadius( PROXY_RADIUS );

	// inform the proxy algorithm to only check front sides of triangles
	g_tool->m_proxyPointForceModel->m_collisionSettings.m_checkBothSidesOfTriangles = false;

	// enable if objects in the scene are going to rotate of translate
	// or possibly collide against the tool. If the environment
	// is entirely static, you can set this parameter to "false"
	g_tool->m_proxyPointForceModel->m_useDynamicProxy = false; //false;

	g_precisionClock.start( true );
	g_prevTimeGraphicLoop = g_precisionClock.getCurrentTimeSeconds();
	g_prevTimeHapticLoop = g_precisionClock.getCurrentTimeSeconds();

	g_workspaceScaleFactor = g_tool->getWorkspaceScaleFactor();
	g_dampingMax = g_hapticDeviceInfo.m_maxLinearDamping / g_workspaceScaleFactor;

	//g_prev.zero();

	// set sphere note color
	g_colors.push_back(cColorf(178.0/255.0f, 34.0/255.0f, 34.0/255.0f));
	g_colors.push_back(cColorf(1.0f, 0.0f, 0.0f));
	g_colors.push_back(cColorf(1.0f, 140.0/255.0f, 0.0f));
	g_colors.push_back(cColorf(1.0f, 215.0/255.0f, 0.0f));
	g_colors.push_back(cColorf(127.0/255.0f, 1.0f, 0.0f));

	g_colors.push_back(cColorf(50.0f/255.0f, 205.0f/255.0f, 50.0f/255.0f));
	g_colors.push_back(cColorf(0.0f, 100.0f/255.0f, 0.0f/255.0f));
	g_colors.push_back(cColorf(47.0f/255.0f, 79.0f/255.0f, 79.0f/255.0f));
	g_colors.push_back(cColorf(135.0f/255.0f, 206.0f/255.0f, 235.0f/255.0f));
	g_colors.push_back(cColorf(30.0f/255.0f, 144.0f/255.0f, 1.0f));

	g_colors.push_back(cColorf(0.0f, 0.0f, 1.0f));
	g_colors.push_back(cColorf(0.0f, 0.0f, 139.0f/255.0f));
	g_colors.push_back(cColorf(75.0f/255.0f, 0.0f, 130.0f/255.0f));
    g_colors.push_back(cColorf(147.0f/255.0f, 112.0f/255.0f, 219.0f/255.0f));
	g_colors.push_back(cColorf(139.0f/255.0f, 0.0f, 139.0f/255.0f));
	g_colors.push_back(cColorf(1.0f, 0.0f, 1.0f));

	int nline = 5;
	float lstart[2] = {7, Z_COORD};
	float lend[2]   = {-0.55, Z_COORD+9.8}; //-0.2};

	for (int i = 0; i < nline; ++i)
	{
		cShapeLine *line = new cShapeLine(	cVector3d(-5.0f, 
											(lend[0]-lstart[0])*i/(nline)+lstart[0],
											(lend[1]-lstart[1])*i/(nline)+lstart[1]), 
											cVector3d(5.0f,
											(lend[0]-lstart[0])*i/(nline)+lstart[0],
											(lend[1]-lstart[1])*i/(nline)+lstart[1]));

		line->m_ColorPointA.set(1.0, 1.0, 1.0);
		g_world->addChild(line);
	}

	initializeCamera();
	initializeLight();
	loadXylophone();
	initMusic();

	g_font.FaceSize(30);
	t_font.FaceSize(1);
}


void initializeGLUT( int argc, char** argv )
{
	//-----------------------------------------------------------------------
	// OPEN GL - WINDOW DISPLAY
	//-----------------------------------------------------------------------
	glutInit( &argc, argv );

	int screenW = glutGet( GLUT_SCREEN_WIDTH );
	int screenH = glutGet( GLUT_SCREEN_HEIGHT );
	int windowPosX = (screenW - WINDOW_SIZE_WIDTH ) / 2;
	int windowPosY = (screenH - WINDOW_SIZE_HEIGHT ) / 2;

	glutInitWindowPosition( windowPosX, windowPosY );
	glutInitWindowSize( WINDOW_SIZE_WIDTH, WINDOW_SIZE_HEIGHT );
	glutInitDisplayMode( GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE );
	glutCreateWindow( "Haptic Xylophone");
	glutDisplayFunc( visualRenderingLoop );
	glutKeyboardFunc( keySelect );
	glutReshapeFunc( resizeWindow );
	glutMouseFunc( mouseEventCallback );
	glutMotionFunc( mouseMotionCallback );
	
#ifdef FULL_SCREEN
	glutFullScreen();
#endif
}


void close(void)
{
	endMusic();
}



int main(int argc, char **argv)
{
	initializeCHAI3D();
	initializeGLUT( argc, argv );

	g_simulationRunning = true;

	cThread*		hapticThread = new cThread();
	hapticThread->set( hapticRenderingLoop, CHAI_THREAD_PRIORITY_HAPTICS );

	glutMainLoop();

	close();

	return 0;
}
