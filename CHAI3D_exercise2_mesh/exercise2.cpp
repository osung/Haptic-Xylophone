#pragma comment( linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"" )

#include "chai3d.h"
#include "ply.h"

//-----------------------------------------------------------------------------------------
// DECLARED CONSTANTS
//-----------------------------------------------------------------------------------------
//#define			FULL_SCREEN

// initial size (width/height) in pixels of the display window
const int WINDOW_SIZE_WIDTH			= 1024;
const int WINDOW_SIZE_HEIGHT		= 768;

//-----------------------------------------------------------------------------------------
// DECLARED VARIABLES
//-----------------------------------------------------------------------------------------

// a world contains all objects of the virtual environment
cWorld*						g_world;

// a camera renders the world in a window display
cCamera*					g_camera;

// camera position and orientation is spherical coordinates
double cameraAngleH;
double cameraAngleV;
double cameraDistance;
cVector3d cameraPosition;
bool flagCameraInMotion = false;

// mouse position and button 
int mouseX, mouseY;
int mouseButton;

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
double									g_proxyRadius = 1.0;

// Font
cFont*								g_Font;

// Mesh
cMesh*								g_mesh;							

// status of the main simulation haptic loop
bool								g_simulationRunning = false;
bool								g_simulationFinished = false;

cVector3d							g_prevMousePos;
int									g_cameraMode = 0;

double								g_updateRateGraphicLoop, g_prevTimeGraphicLoop;
double								g_updateRateHapticLoop, g_prevTimeHapticLoop;

cPrecisionClock						g_precisionClock;

// Update rate
double g_updateRateGraphicElapsedTime = 0.0;
double g_averageUpdateRateGraphicLoop = 1.0;
int	   g_graphicRenderingCounter = 0;

double g_updateRateHapticElapsedTime = 0.0;
double g_averageUpdateRateHapticLoop = 1.0;
int	   g_hapticRenderingCounter = 0;


typedef struct Vertex {
  float x,y,z;             /* the usual 3-space position of a vertex */
} Vertex;

typedef struct Face {
  //unsigned char intensity; /* this user attaches intensity to faces */
  unsigned char nverts;    /* number of vertex indices in list */
  int *verts;              /* vertex index list */
} Face;


/* information needed to describe the user's data to the PLY routines */

char *elem_names[] = { /* list of the kinds of elements in the user's object */
  "vertex", "face"
};

PlyProperty vert_props[] = { /* list of property information for a vertex */
  {"x", PLY_FLOAT, PLY_FLOAT, offsetof(Vertex,x), 0, 0, 0, 0},
  {"y", PLY_FLOAT, PLY_FLOAT, offsetof(Vertex,y), 0, 0, 0, 0},
  {"z", PLY_FLOAT, PLY_FLOAT, offsetof(Vertex,z), 0, 0, 0, 0},
};

PlyProperty face_props[] = { /* list of property information for a vertex */
  //{"intensity", PLY_UCHAR, PLY_UCHAR, offsetof(Face,intensity), 0, 0, 0, 0},
  {"vertex_indices", PLY_INT, PLY_INT, offsetof(Face,verts),
   1, PLY_UCHAR, PLY_UCHAR, offsetof(Face,nverts)},
};



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

// update position of virtual camera
void updateCameraPosition();



/******************************************************************************
Read in a PLY file.
******************************************************************************/

cMesh* ply_read(char *filename)
{
  int i,j,k;
  PlyFile *ply;
  int nelems;
  char **elist;
  int file_type;
  float version;
  int nprops;
  int num_elems;
  PlyProperty **plist;
  Vertex **vlist;
  Face **flist;
  char *elem_name;
  int num_comments;
  char **comments;
  int num_obj_info;
  char **obj_info;

  cMesh *mesh = new cMesh(g_world);

  /* open a PLY file for reading */
  ply = ply_open_for_reading(filename, &nelems, &elist, &file_type, &version);

  /* print what we found out about the file */
  printf ("version %f\n", version);
  printf ("type %d\n", file_type);

  /* go through each kind of element that we learned is in the file */
  /* and read them */
  // read vertices
  for (i = 0; i < nelems; i++) {

    /* get the description of the first element */
    elem_name = elist[i];
    plist = ply_get_element_description (ply, elem_name, &num_elems, &nprops);

    /* print the name of the element, for debugging */
    printf ("element %s %d\n", elem_name, num_elems);

    /* if we're on vertex elements, read them in */
    if (equal_strings ("vertex", elem_name)) {

      /* create a vertex list to hold all the vertices */
      vlist = (Vertex **) malloc (sizeof (Vertex *) * num_elems);

      /* set up for getting vertex elements */

      ply_get_property (ply, elem_name, &vert_props[0]);
      ply_get_property (ply, elem_name, &vert_props[1]);
      ply_get_property (ply, elem_name, &vert_props[2]);

      /* grab all the vertex elements */
      for (j = 0; j < num_elems; j++) {

        /* grab and element from the file */
        vlist[j] = (Vertex *) malloc (sizeof (Vertex));
        ply_get_element (ply, (void *) vlist[j]);

        /* print out vertex x,y,z for debugging */
        //printf ("vertex: %g %g %g\n", vlist[j]->x, vlist[j]->y, vlist[j]->z);
		mesh->newVertex(vlist[j]->x, vlist[j]->y, vlist[j]->z);
      }
    }

    /* if we're on face elements, read them in */
    if (equal_strings ("face", elem_name)) {

      /* create a list to hold all the face elements */
      flist = (Face **) malloc (sizeof (Face *) * num_elems);

      /* set up for getting face elements */

      ply_get_property (ply, elem_name, &face_props[0]);

      /* grab all the face elements */
      for (j = 0; j < num_elems; j++) {

        /* grab and element from the file */
        flist[j] = (Face *) malloc (sizeof (Face));
        ply_get_element (ply, (void *) flist[j]);

        /* print out face info, for debugging */
        //printf ("face: %d, list = ", flist[j]->intensity);
        //for (k = 0; k < flist[j]->nverts; k++)
			mesh->newTriangle(flist[j]->verts[0], flist[j]->verts[1], flist[j]->verts[2]);
          //printf ("%d ", flist[j]->verts[k]);
        printf ("\n");
      }
    }
    
    /* print out the properties we got, for debugging */
    for (j = 0; j < nprops; j++)
      printf ("property %s\n", plist[j]->name);
  }

  /* grab and print out the comments in the file */
  comments = ply_get_comments (ply, &num_comments);
  for (i = 0; i < num_comments; i++)
    printf ("comment = '%s'\n", comments[i]);

  /* grab and print out the object information */
  obj_info = ply_get_obj_info (ply, &num_obj_info);
  for (i = 0; i < num_obj_info; i++)
    printf ("obj_info = '%s'\n", obj_info[i]);

  /* close the PLY file */
  ply_close (ply);

  return mesh;
}




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
	g_tool->setWorkspaceRadius(10.0);

	// define a radius for the tool
	g_tool->setRadius( g_proxyRadius );

	// hide the device sphere. only show proxy.
	g_tool->m_deviceSphere->setShowEnabled(true);
	g_tool->m_proxySphere->setShowEnabled( true );
	g_tool->m_materialProxy.m_diffuse.set( 1.0, 0.0, 1.0 );
	g_tool->m_deviceSphere->m_material.m_diffuse.set( 0.0, 1.0, 0.0 );
	//g_tool->getProxy()->m_forceShadingAngleThreshold = 1;
	//g_tool->getProxy()->m_useForceShading = false;

	// set the physical radius of the proxy. for performance reasons, it is
	// sometimes useful to set this value to zero when dealing with
	// complex objects.s
	g_tool->m_proxyPointForceModel->setProxyRadius(0 );

	// inform the proxy algorithm to only check front sides of triangles
	g_tool->m_proxyPointForceModel->m_collisionSettings.m_checkBothSidesOfTriangles = true;

	// enable if objects in the scene are going to rotate of translate
	// or possibly collide against the tool. If the environment
	// is entirely static, you can set this parameter to "false"
	g_tool->m_proxyPointForceModel->m_useDynamicProxy = false;

	g_precisionClock.start( true );
	g_prevTimeGraphicLoop = g_precisionClock.getCurrentTimeSeconds();
	g_prevTimeHapticLoop = g_precisionClock.getCurrentTimeSeconds();

	initializeCamera();
	initializeLight();
	load3DModels();
}

void initializeCamera()
{
	g_camera = new cCamera( g_world );
	g_world->addChild( g_camera );

	g_camera->setFieldViewAngle( 45 );
	g_camera->set( cVector3d( 0.0, 0.0, 40.0 ),
		cVector3d( 0.0, 0.0, 0.0 ),
		cVector3d( 0.0, 1.0, 0.0 ) );

	g_camera->setClippingPlanes( 0.01, 100.0 );
	g_camera->computeGlobalPositions( true );

	// position and oriente the camera
    // define a default position of the camera (described in spherical coordinates)
    cameraAngleH = 0;
    cameraAngleV = -45;
    cameraDistance = 30;

    //updateCameraPosition();
}

void initializeLight()
{
	// Create a light source
	g_light = new cLight( g_world );

	g_world->addChild( g_light );
	g_light->setEnabled(true);
	g_light->setPos( cVector3d( 0.0, 10.0, 1.0 ) );  
	g_light->setDir( cVector3d( 0.0, -10.0, 1.0 ) );
}

void load3DModels()
{

	// create a virtual mesh
	cMesh *g_mesh = new cMesh( g_world );
	

	// load an object file
	bool fileload = g_mesh->loadFromFile( "bunny.obj" );
	if( !fileload )
	{
		printf( "Error - 3D Model failed to load correctly");
		close();
		return;
	}
	
/*	
	g_mesh = ply_read("iso_010");
	g_mesh->computeAllNormals(true);
	g_mesh->useDisplayList(true);
	*/

	g_world->addChild( g_mesh );



    // get dimensions of object
	double size = cSub( g_mesh->getBoundaryMax(), g_mesh->getBoundaryMin() ).length();

	// resize object to screen
	if( size > 0 )
	{
		g_mesh->scale( 2.0 * g_tool->getWorkspaceRadius() / size );
	//	g_mesh->scale(g_tool->getWorkspaceRadius() / size );
    }

	g_mesh->setPos( 0.0, 0.0, 0.0 );
	g_mesh->setWireMode( false );
//	g_mesh->rotate( cVector3d( 0, 1, 0 ), CHAI_DEG2RAD * 90 );
//	g_mesh->rotate( cVector3d( 0, 0, 1 ), CHAI_DEG2RAD * 90 );
	g_mesh->computeGlobalPositions();
//	g_mesh->createTriangleNeighborList(true);

	// compute a boundary box
	g_mesh->computeBoundaryBox( true );


    // compute collision detection algorithm
	g_mesh->createAABBCollisionDetector( 1.00 * g_proxyRadius, true, false );

	// read the scale factor between the physical workspace of the haptic
	// device and the virtual workspace defined for the tool
	double workspaceScaleFactor = g_tool->getWorkspaceScaleFactor();

	// define a maximum stiffness that can be handled by the current
	// haptic device. The value is scaled to take into account the
	// workspace scale factor
	double stiffnessMax = g_hapticDeviceInfo.m_maxForceStiffness / workspaceScaleFactor;

	// define a default stiffness for the object
	g_mesh->setStiffness( stiffnessMax * 0.5 );
	//g_mesh->setStiffness(0 );

	// define some haptic friction properties
	//g_mesh->setFriction( 0.2, 0.2, true );

	//g_mesh->setCollisionDetectorProperties( 10, cColorf( 1.0, 0.0, 0.0 ), true);
	//g_mesh->setShowCollisionTree( true, true );
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



void updateCameraPosition()
{
    // check values
    if (cameraDistance < 0.1) { cameraDistance = 0.1; }
    if (cameraAngleV > 89) { cameraAngleV = 89; }
    if (cameraAngleV < 10) { cameraAngleV = 10; }

    // compute position of camera in space
    cVector3d pos = cAdd(
                        cameraPosition,
                        cVector3d(
                            cameraDistance * cCosDeg(cameraAngleH) * cCosDeg(cameraAngleV),
                            cameraDistance * cSinDeg(cameraAngleH) * cCosDeg(cameraAngleV),
                            cameraDistance * cSinDeg(cameraAngleV)
                        )
                    );

    // compute lookat position
    cVector3d lookat = cameraPosition;

    // define role orientation of camera
    cVector3d up(0.0, 1.0, 0.0);

    // set new position to camera
    g_camera->set(pos, lookat, up);

    // recompute global positions
    g_world->computeGlobalPositions(true);
}



void mouseEventCallback( int button, int state, int x, int y )
{
    // mouse button down
    if (state == GLUT_DOWN)
    {
        flagCameraInMotion = true;
        mouseX = x;
        mouseY = y;
        mouseButton = button;
    }

    // mouse button up
    else if (state == GLUT_UP)
    {
        flagCameraInMotion = false;
    }

}

void mouseMotionCallback( int x, int y )
{
   if (flagCameraInMotion)
    {
        if (mouseButton == GLUT_RIGHT_BUTTON)
        {
            cameraDistance = cameraDistance - 0.01 * (y - mouseY);
        }

        else if (mouseButton == GLUT_LEFT_BUTTON)
        {
            cameraAngleH = cameraAngleH - (x - mouseX);
            cameraAngleV = cameraAngleV - (y - mouseY);
        }
    }

    updateCameraPosition();

    mouseX = x;
    mouseY = y;

}

void close( void )
{

}