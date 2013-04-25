#pragma comment( linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"" )

#include "chai3d.h"

//-----------------------------------------------------------------------------------------
// DECLARED CONSTANTS
//-----------------------------------------------------------------------------------------
//#define			FULL_SCREEN

// initial size (width/height) in pixels of the display window
const int WINDOW_SIZE_WIDTH			= 1000;
const int WINDOW_SIZE_HEIGHT		= 1000;

//-----------------------------------------------------------------------------------------
// DECLARED VARIABLES
//-----------------------------------------------------------------------------------------

// a world contains all objects of the virtual environment
cWorld*						g_world;

// a camera renders the world in a window display
cCamera*					g_camera;

// light sources to illuminate the objects in the virtual scene
cLight*							g_light;

// width and height of the current window display
int								g_displayWidth   = 0;
int								g_displayHeight  = 0;

// a haptic device handler
cHapticDeviceHandler*					g_hapticDeviceHandler;

// information about the current haptic device
cHapticDeviceInfo						g_hapticDeviceInfo;

// a virtual tool representing the haptic device in the scene
cGeneric3dofPointer*					g_tool;

// radius of the tool proxy
double									g_proxyRadius = 0.05;

// Font
cFont*								g_Font;

// Mesh
cShapeSphere*						g_shapeSphere;

// status of the main simulation haptic loop
bool								g_simulationRunning = false;
bool								g_simulationFinished = false;

cVector3d							g_prevMousePos;
int									g_cameraMode = 0;

double								g_updateRateGraphicLoop, g_prevTimeGraphicLoop;
double								g_updateRateHapticLoop, g_prevTimeHapticLoop;

double								g_dampingMax;
double								g_workspaceScaleFactor;

cPrecisionClock						g_precisionClock;

// Update rate
double g_updateRateGraphicElapsedTime = 0.0;
double g_averageUpdateRateGraphicLoop = 1.0;
int	   g_graphicRenderingCounter = 0;

double g_updateRateHapticElapsedTime = 0.0;
double g_averageUpdateRateHapticLoop = 1.0;
int	   g_hapticRenderingCounter = 0;

//-----------------------------------------------------------------------------------------
// DECLARED FUNCTIONS
//-----------------------------------------------------------------------------------------

// callback when the window display is resized
void resizeWindow( int width, int height );

// callback when a keyboard key is pressed
void keySelect( unsigned char key, int x, int y );

// callback when the mouse moves within the window while one or more mouse buttons are pressed
void mouseMotionCallback( int currentMousePosX, int currentMousePosY );

// callback when a user presses and releases mouse buttons in the window
void mouseEventCallback( int button, int state, int x, int y );

// function called before existing the application
void close( void );

// main graphics callback
void graphicRenderingLoop( void );

// main haptics loop
void hapticRenderingLoop( void );

// initialize CHAI3D library
void initializeCHAI3D( void );
void initializeLight();

// initialize a OpenGL utility library
void initializeGLUT( int argc, char** argv );

void initializeCamera();

void load3DModels();

void renderText();

int main( int argc, char** argv )
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

void initializeCHAI3D()
{
	//-----------------------------------------------------------------------
	// 3D - SCENEGRAPH
	//-----------------------------------------------------------------------	
	g_world = new cWorld();

	g_world->setBackgroundColor( 0.0, 0.0, 0.0 );
	g_world->setFrameSize( 1.0 );

	g_Font = cFont::createFont();

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
	g_tool->setWorkspaceRadius(1.0);

	// define a radius for the tool
	g_tool->setRadius( g_proxyRadius );

	// hide the device sphere. only show proxy.
	g_tool->m_deviceSphere->setShowEnabled(true);
	g_tool->m_proxySphere->setShowEnabled( true );
	g_tool->m_materialProxy.m_diffuse.set( 1.0, 0.0, 1.0 );
	g_tool->m_deviceSphere->m_material.m_diffuse.set( 0.0, 1.0, 0.0 );

	// set the physical radius of the proxy. for performance reasons, it is
	// sometimes useful to set this value to zero when dealing with
	// complex objects.
	g_tool->m_proxyPointForceModel->setProxyRadius( g_proxyRadius );

	// inform the proxy algorithm to only check front sides of triangles
	g_tool->m_proxyPointForceModel->m_collisionSettings.m_checkBothSidesOfTriangles = false;

	// enable if objects in the scene are going to rotate of translate
	// or possibly collide against the tool. If the environment
	// is entirely static, you can set this parameter to "false"
	g_tool->m_proxyPointForceModel->m_useDynamicProxy = false;

	g_precisionClock.start( true );
	g_prevTimeGraphicLoop = g_precisionClock.getCurrentTimeSeconds();
	g_prevTimeHapticLoop = g_precisionClock.getCurrentTimeSeconds();

	g_workspaceScaleFactor = g_tool->getWorkspaceScaleFactor();

	g_dampingMax = g_hapticDeviceInfo.m_maxLinearDamping / g_workspaceScaleFactor;

	initializeCamera();
	initializeLight();
	load3DModels();
}

void initializeCamera()
{
	g_camera = new cCamera( g_world );
	g_world->addChild( g_camera );

	g_camera->setFieldViewAngle( 45 );
	g_camera->set( cVector3d( 0.0, 0.0, 4.0 ),
		cVector3d( 0.0, 0.0, 0.0 ),
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
}

void load3DModels()
{
	g_shapeSphere = new cShapeSphere(0.6 );
	

	double stiffnessMax = g_hapticDeviceInfo.m_maxForceStiffness / g_workspaceScaleFactor;
	double forceMax = g_hapticDeviceInfo.m_maxForce / g_workspaceScaleFactor;

	g_shapeSphere->setPos( 0, 0, 0 );

	g_shapeSphere->setStiffness( stiffnessMax*0.5 );

//	g_shapeSphere->m_material.setVibrationFrequency(15);
//	g_shapeSphere->m_material.setVibrationAmplitude( forceMax * 0.5);
	g_shapeSphere->m_material.setMagnetMaxDistance( 1 );
	g_shapeSphere->m_material.setMagnetMaxForce(forceMax);

	// set haptic properties
//    g_shapeSphere->m_material.setViscosity(g_dampingMax);

	g_world->addChild( g_shapeSphere );

	cGenericEffect* newEffect = new cEffectSurface( g_shapeSphere );
	g_shapeSphere->addEffect( newEffect );

	cGenericEffect* newEffect2 = new cEffectMagnet( g_shapeSphere );
	g_shapeSphere->addEffect( newEffect2 );
	
	
//	cGenericEffect* newEffect3 = new cEffectVibration( g_shapeSphere );
 //   g_shapeSphere->addEffect(newEffect3);

//	cGenericEffect * newEffect4 = new cEffectViscosity(g_shapeSphere);
	//g_shapeSphere->addEffect(newEffect);
	

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
	glutCreateWindow( "Programming exercise #1");
	glutDisplayFunc( graphicRenderingLoop );
	glutKeyboardFunc( keySelect );
	glutReshapeFunc( resizeWindow );
	glutMouseFunc( mouseEventCallback );
	glutMotionFunc( mouseMotionCallback );

#ifdef FULL_SCREEN
	glutFullScreen();
#endif
}

void renderText()
{
	char			buf[ 256 ];

	glDisable( GL_LIGHTING );
	g_Font->setPointSize( 20 );

	glMatrixMode( GL_MODELVIEW );
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode( GL_PROJECTION );
	glPushMatrix();
	glLoadIdentity();

	glTranslatef( 0, 0, -1 );
	glColor3f( 0, 1, 0 );

	glRasterPos2f( -0.99, 0.95 );
	sprintf( buf, "Graphic rendering: %.2f Hz (%.2f msec)", 1000.0 / g_averageUpdateRateGraphicLoop, g_averageUpdateRateGraphicLoop );
	g_Font->renderString( buf );

	glRasterPos2f( -0.99, 0.85 );
	sprintf( buf, "Haptic rendering: %.2f kHz (%.2f microsec)", 1000.0 / (g_averageUpdateRateHapticLoop), g_averageUpdateRateHapticLoop );
	g_Font->renderString( buf );

	glPopMatrix();
	glMatrixMode( GL_MODELVIEW );
	glPopMatrix();

	glEnable( GL_LIGHTING );
}

void graphicRenderingLoop( void )
{
	g_camera->renderView( g_displayWidth, g_displayHeight );

	renderText();

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

void hapticRenderingLoop( void )
{
	while( g_simulationRunning )
	{
		double currentTime = g_precisionClock.getCurrentTimeSeconds();
		g_updateRateHapticLoop = currentTime - g_prevTimeHapticLoop;
		g_updateRateHapticElapsedTime += g_updateRateHapticLoop;
		g_prevTimeHapticLoop = currentTime;

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

        // update position and orientation of tool
		g_tool->updatePose();

		// compute interaction forces
		g_tool->computeInteractionForces();

		// send forces to device
		g_tool->applyForces();

		g_hapticRenderingCounter++;
	}

	g_simulationFinished = true;
}

void resizeWindow( int width, int height )
{
	g_displayWidth  = width;
	g_displayHeight = height;
	glViewport( 0, 0, g_displayWidth, g_displayHeight );
}

void keySelect( unsigned char key, int x, int y )
{
	switch( key )
	{
	case 27:   // Esc
		close();
		exit( 0 );
		break;

	case 32:   // Space bar
		break;

	case '1':
		break;

	case '2':
		break;

	case '3':
		break;

	case '4':
		break;
	}

}

void mouseEventCallback( int button, int state, int x, int y )
{
	if( button ==  GLUT_LEFT_BUTTON || button == GLUT_RIGHT_BUTTON || button == GLUT_MIDDLE_BUTTON )
	{
		if( state == GLUT_DOWN )
		{
			g_prevMousePos.set( x, y, 0 );
			//			g_tool->setForcesOFF();

		}	
		else if( state == GLUT_UP )
		{
			//			g_tool->setForcesON();
		}
	}
}

void mouseMotionCallback( int currentMousePosX, int currentMousePosY )
{
	cVector3d			 currentMousePos;
	bool				 leftButton, rightButton, middleButton;

	currentMousePos.set( currentMousePosX, currentMousePosY, 0 );

	// Check left mouse button
	if( ::GetAsyncKeyState( VK_LBUTTON ) & 0x8000 ) leftButton = true;
	else leftButton = false;

	// Check right mouse button
	if( ::GetAsyncKeyState( VK_RBUTTON ) & 0x80000 ) rightButton = true;
	else rightButton = false;

	if( ::GetAsyncKeyState( VK_MBUTTON ) & 0x80000 ) middleButton = true;
	else middleButton = false;

	// Rotate
	if( leftButton == true && rightButton == false )
	{
	}

	// Zoom
	if( leftButton == false && rightButton == true )
	{

	}

	// Pan
	if( middleButton == true )
	{
	}

	g_prevMousePos = currentMousePos;
}

void close( void )
{

}