#if defined(_MSC_VER)
//disable _CRT_SECURE_NO_WARNINGS message to build this in VC++
#pragma warning(disable:4996)
#endif

#include "Scene.h"
#ifdef ARM9
#include "loader.h"
#include "GXPayload.h" //required to flush the GX<->DMA<->FIFO circuit on real hardware
#endif
#ifndef _MSC_VER
					// //
#define ARM9 1		// Enable only if not real GCC + NDS environment
#undef _MSC_VER		// //
#undef WIN32		// //
#endif

int widthScene;	/// the width of the window
int heightScene;	/// the height of the window

// light 0 colours

//https://www.glprogramming.com/red/chapter05.html
//The GL_DIFFUSE parameter probably most closely correlates with what you naturally think of as "the color of a light." 
//It defines the RGBA color of the diffuse light that a particular light source adds to a scene. By default, GL_DIFFUSE is (1.0, 1.0, 1.0, 1.0) for GL_LIGHT0, 
//which produces a bright, white light as shown in the left side of "Plate 13" in Appendix I. 
//The default value for any other light (GL_LIGHT1, ... , GL_LIGHT7) is (0.0, 0.0, 0.0, 0.0).
GLfloat light_diffuse0Scene[4]	= {0.9f, 0.9f, 0.4f, 1.01f}; //WIN32

GLfloat light_ambient0Scene[4]	= {0.1f, 0.1f, 0.1f, 1.0f}; //WIN32
GLfloat light_specular0Scene[4]	= {0.6f, 0.6f, 0.6f, 1.0f}; //WIN32
GLfloat light_position0Scene[4]	= {-1.0f, -1.0f, 1.0f, 0.0f}; //WIN32

// light 1 colours
GLfloat light_ambient1Scene[4]	= {0.1f, 0.1f, 0.1f, 1.0f};
GLfloat light_diffuse1Scene[4]	= {0.45f, 0.45f, 0.45f, 1.0f};
GLfloat light_specular1Scene[4]	= {0.5f, 0.5f, 0.5f, 1.0f};
GLfloat light_position1Scene[4]	= {-2.0f, -5.0f, -5.0f, -1.0f};

//material
GLfloat mat_ambient[]    = { 8.0f, 8.0f, 8.0f, 0.0f }; 
GLfloat mat_diffuse[]    = { 16.0f, 16.0f, 16.0f, 0.0f }; 
GLfloat mat_specular[]   = { 8.0f, 8.0f, 8.0f, 0.0f }; 
GLfloat mat_emission[]   = { 5.0f, 5.0f, 5.0f, 0.0f }; 
GLfloat high_shininess[] = { 128.0f };

/// Resets the camera position to default position and tilt
void initializeCamera(struct Camera * Inst){
	TWLPrintf("-- Creating camera\n");
	Inst->verticalTilt = -30.0f;
	Inst->horizontalAngle = 35.0f;
#ifdef WIN32
	Inst->distance = -90.0f;
#endif

#ifdef ARM9
	Inst->distance = 0.0f;
#endif
}

/// Positions the camera at the required place and rotation
/// Zoom and spin is done by translate/rotate
#if (defined(__GNUC__) && !defined(__clang__))
__attribute__((optimize("Ofast")))
#endif
#if (!defined(__GNUC__) && defined(__clang__))
__attribute__ ((optnone))
#endif
void position(struct Camera * Inst){
	glTranslatef(0.0f, 0.0f, Inst->distance);
	glRotatef(Inst->verticalTilt, 1.0f, 0.0f, 0.0f);
	glRotatef(Inst->horizontalAngle, 0.0f, 1.0f, 0.0f);

//DS GX: Set extra camera parameters
#ifdef ARM9	
	//any floating point gl call is being converted to fixed prior to being implemented
	gluPerspective(-45, 256.0 / 192.0, 0.1, 250);


	gluLookAt(	1.0, -Inst->distance, -45.0f + Inst->horizontalAngle,		//camera possition 
				1.0, 1.0, 1.0,		//look at
				1.0, 1.0, 45.0		//up
	);		
#endif
}

/// Decrements the distance to origin (zoom in)
#if (defined(__GNUC__) && !defined(__clang__))
__attribute__((optimize("Ofast")))
#endif
#if (!defined(__GNUC__) && defined(__clang__))
__attribute__ ((optnone))
#endif
void dec(struct Camera * Inst){
	#if defined(controlCamera)
	Inst->distance--;
	#endif

	#if !defined(controlCamera)
		xrot+=6;
	#endif
}

/// Incrementes the distance to origin (zoom out)
#if (defined(__GNUC__) && !defined(__clang__))
__attribute__((optimize("Ofast")))
#endif
#if (!defined(__GNUC__) && defined(__clang__))
__attribute__ ((optnone))
#endif
void inc(struct Camera * Inst){
	#if defined(controlCamera)
	Inst->distance++;
	#endif

	#if !defined(controlCamera)
		xrot-=6;
	#endif
}

/// Adjusts the camera rotation around the Y axis
#if (defined(__GNUC__) && !defined(__clang__))
__attribute__((optimize("Ofast")))
#endif
#if (!defined(__GNUC__) && defined(__clang__))
__attribute__ ((optnone))
#endif
void clockwise(struct Camera * Inst){
	#if defined(controlCamera)
	Inst->horizontalAngle++;
	#endif

	#if !defined(controlCamera)
		yrot+=6;
	#endif
}

/// Adjusts the camera rotation around the Y axis
#if (defined(__GNUC__) && !defined(__clang__))
__attribute__((optimize("Ofast")))
#endif
#if (!defined(__GNUC__) && defined(__clang__))
__attribute__ ((optnone))
#endif
void anticlockwise(struct Camera * Inst){
#if defined(controlCamera)
	Inst->horizontalAngle--;
#endif

#if !defined(controlCamera)
	yrot-=6;
#endif
}

/// Adjusts the camera rotation around the X axis
/// the angle is locked if it gets above 0 degrees
#if (defined(__GNUC__) && !defined(__clang__))
__attribute__((optimize("Ofast")))
#endif
#if (!defined(__GNUC__) && defined(__clang__))
__attribute__ ((optnone))
#endif
void tiltup(struct Camera * Inst){
	if (Inst->verticalTilt < 0)
		Inst->verticalTilt++;
}

/// Adjusts the camera rotation around the X axis
/// The angle is locked if it gets greate than 90 degrees
#if (defined(__GNUC__) && !defined(__clang__))
__attribute__((optimize("Ofast")))
#endif
#if (!defined(__GNUC__) && defined(__clang__))
__attribute__ ((optnone))
#endif
void tiltdown(struct Camera * Inst){
	if (Inst->verticalTilt > -90)
		Inst->verticalTilt--;
}

/// Default Constructor. Initialises defaults.
#if (defined(__GNUC__) && !defined(__clang__))
__attribute__((optimize("Ofast")))
#endif
#if (!defined(__GNUC__) && defined(__clang__))
__attribute__ ((optnone))
#endif
void initializeScene(struct Scene * Inst){
	TWLPrintf("-- Creating scene\n");

	// set up our directional overhead lights
	Inst->light0On = false;
	Inst->light1On = false;
	
	Inst->fogMode = false;
	Inst->wireMode = false;		/// wireframe mode on / off
	Inst->flatShading = false;	/// flat shading on / off
	initializeCamera(&Inst->camera); //construct camera
}

#ifdef ARM9
static bool renderCube = false;
#endif
#ifdef WIN32
static bool renderCube;
#endif

#if (defined(__GNUC__) && !defined(__clang__))
__attribute__((optimize("Ofast")))
#endif
#if (!defined(__GNUC__) && defined(__clang__))
__attribute__ ((optnone))
#endif
void render3DUpperScreen(){
	//Update camera for NintendoDS Upper 3D Screen:
	renderCube = false;
}

#if (defined(__GNUC__) && !defined(__clang__))
__attribute__((optimize("Ofast")))
#endif
#if (!defined(__GNUC__) && defined(__clang__))
__attribute__ ((optnone))
#endif
void render3DBottomScreen(){
	//Update camera for NintendoDS Bottom 3D Screen
	renderCube = true;
}

/// Renders a single frame of the scene
#if (defined(__GNUC__) && !defined(__clang__))
__attribute__((optimize("Ofast")))
#endif
#if (!defined(__GNUC__) && defined(__clang__))
__attribute__ ((optnone))
#endif
void drawScene(){
	struct Scene * Inst = &scene;

	#ifdef ARM9
	//NDS: Dual 3D Render implementation. Must be called right before a new 3D scene is drawn
	if(Inst->TGDSProjectDual3DEnabled == true){
		TGDS_ProcessDual(render3DUpperScreen, render3DBottomScreen);
	}
	#endif

	#ifdef WIN32
	// Clear The Scene And The Depth Buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	
	#endif
	
	#ifdef ARM9
	glMatrixMode(GL_TEXTURE); //GX 3D hardware needs this to enable texturing on a frame basis
	#endif
	
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	
	//position camera
	position(&Inst->camera); 
	
	// draw element(s) in the scene + light source(s)
	#ifdef ARM9
		glMaterialShinnyness();		
		updateGXLights(); //Update GX 3D light scene!
		glColor3f(1.0, 1.0, 1.0); //clear last scene color/light vectors
	#endif
	
	if(renderCube == false){
		#ifdef WIN32
		drawSphereCustom(32, 32, 32);
		#endif

		#ifdef ARM9
		drawSphereCustom(1, 8, 8);
		#endif
	}
	else{
		int yloop = 0;
		int xloop= 0;

		float rotateX = 0.0;
		float rotateY = 0.0;

#ifdef ARM9
		scanKeys();
		int pen_delta[2];
		bool isTSCActive = get_pen_delta( &pen_delta[0], &pen_delta[1] );
		if( isTSCActive == false ){
			rotateX = 0.0;
			rotateY = 0.0;
		}
		else{
			rotateX = pen_delta[0];
			rotateY = pen_delta[1];
			if(yrot > 0){
				yrot-=rotateY;
			}
			else{
				yrot+=rotateY;
			}
		
			if(xrot > 0){
				xrot-=rotateX;
			}
			else{
				xrot+=rotateX;
			}
		}
#endif
		//DS doesn't support filtering.
		#ifdef WIN32
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		#endif
		
#ifdef ARM9
	//Clear The Screen And The Depth Buffer
	glReset(); //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	todo: implement https://stackoverflow.com/questions/28137027/why-do-i-need-glcleargl-depth-buffer-bit through cuboid tests and rendering into OpenGL
	//Instead, we forcefully reset so always points to a first default projection matrix, then matrix node relative to a first default modelview 
	
	//Set initial view to look at
	gluPerspective(18, 256.0 / 192.0, 0.1, 40);

	//Camera perspective from there
	gluLookAt(	0.0, 0.0, 4.0,		//camera possition 
				0.0, 0.0, 0.0,		//look at
				0.0, 1.0, 0.0		//up
	);
#endif

		for (yloop=1;yloop<6;yloop++)
		{
			for (xloop=0;xloop<yloop;xloop++)
			{
				// enable texturing
				glEnable(GL_TEXTURE_2D);
	
				glBindTexture(
			#ifdef WIN32		
					GL_TEXTURE_2D, texture[0]
			#endif
			#ifdef ARM9
				0, textureSizePixelCoords[Texture_MetalCubeID].textureIndex
			#endif	
				);

				glLoadIdentity();							// Reset The View
				glTranslatef(1.4f+(((float)xloop)*2.8f)-(((float)yloop)*1.4f),((6.0f-((float)yloop))*2.4f)-7.0f, -20.0f);
				glRotatef(45.0f-(2.0f*yloop)+xrot,1.0f,0.0f,0.0f);
				glRotatef(45.0f+yrot,0.0f,1.0f,0.0f);

				glLightfv(GL_LIGHT0, GL_AMBIENT,  light_ambient0Scene);
				glLightfv(GL_LIGHT0, GL_DIFFUSE,  light_diffuse0Scene);
				glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular0Scene);
				glLightfv(GL_LIGHT0, GL_POSITION, light_position0Scene);
	
				glLightfv(GL_LIGHT1, GL_AMBIENT, light_ambient1Scene);
				glLightfv(GL_LIGHT1, GL_DIFFUSE, light_diffuse1Scene);
				glLightfv(GL_LIGHT1, GL_SPECULAR, light_specular1Scene);
				glLightfv(GL_LIGHT1, GL_POSITION, light_position1Scene);

				#ifdef ARM9
				glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT,   mat_ambient); 
				glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE,   mat_diffuse);
				glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR,  mat_specular);
				glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION,  mat_emission);
				glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, high_shininess);
				#endif
				
				glColor3fv(boxcol[yloop-1]);
				glCallList(box);
				glColor3fv(topcol[yloop-1]);
				glCallList(top);
			}
		}
	}
	
	#ifdef WIN32
	glutSwapBuffers();
	#endif

	#ifdef ARM9
    glFlush();
	handleARM9SVC();	/* Do not remove, handles TGDS services */
    IRQVBlankWait();
    #endif
}

void glut2SolidCube0_06f() {
#ifdef ARM9
	updateGXLights(); //Update GX 3D light scene!
#endif
	glCallList(DLSOLIDCUBE0_06F);
}

/// Sets up the OpenGL state machine environment
/// All hints, culling, fog, light models, depth testing, polygon model
/// and blending are set up here
#ifdef ARM9
#if (defined(__GNUC__) && !defined(__clang__))
__attribute__((optimize("O0")))
#endif
#if (!defined(__GNUC__) && defined(__clang__))
__attribute__((optnone))
#endif
#endif
int InitGL(int argc, char *argv[]){
	TWLPrintf("-- Setting up OpenGL context\n");
#ifdef _MSC_VER
	// initialise glut
	glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutCreateWindow("TGDS Project through OpenGL (GLUT)");
	glutFullScreen();
#endif

#ifdef WIN32
	// blue green background colour
	glClearColor(0.0, 0.5, 0.55
#ifdef _MSC_VER
		, 1.0
#endif	
	);
	glShadeModel(GL_SMOOTH);

	// depth testing used on with less than testing
#ifdef _MSC_VER
	glDepthFunc(GL_LESS);
	glEnable(GL_DEPTH_TEST);
#endif

	// setup  fog, but disable for now
#ifdef _MSC_VER
	glDisable(GL_FOG);
	glFogi(GL_FOG_MODE, GL_EXP);
	{
		GLfloat fogColor[4] = {0.0f, 0.5f, 0.55f, 1.0f};
		glFogfv(GL_FOG_COLOR, fogColor);
	}
	glFogf(GL_FOG_DENSITY, 0.0075);
	
	// enable normalising of normals after scaling
	glEnable(GL_NORMALIZE);
#endif
	// setup lighting, but disable for now
	glDisable(GL_LIGHTING);
#ifdef _MSC_VER
	{
		GLfloat ambient[] = {0.1f, 0.1f, 0.1f, 1.0};
		glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
	}
	glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);

	/*
	* The actual lights are defined in the Scene class
	*/

	// set up line antialiasing
	glLineWidth(1.0f);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#endif
	// setup backface culling
	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);
#endif

#ifdef ARM9
	int TGDSOpenGLDisplayListWorkBufferSize = (256*1024);
	struct Scene * Inst = &scene;
	glInit(TGDSOpenGLDisplayListWorkBufferSize); //NDSDLUtils: Initializes a new videoGL context
	
	glClearColor(255,255,255);		// White Background
	glClearDepth(0x7FFF);		// Depth Buffer Setup
	glEnable(GL_ANTIALIAS|GL_TEXTURE_2D|GL_BLEND); // Enable Texture Mapping + light #0 enabled per scene

	glDisable(GL_LIGHT0|GL_LIGHT1|GL_LIGHT2|GL_LIGHT3);

	if(Inst->TGDSProjectDual3DEnabled == false){
		setTGDSARM9PrintfCallback((printfARM9LibUtils_fn)&TGDSDefaultPrintf2DConsole); //Redirect to default TGDS printf Console implementation
		menuShow();
	}
	REG_IE |= IRQ_VBLANK;
	glReset(); //Depend on GX stack to render scene
	glClearColor(0,35,195);		// blue green background colour

	/* TGDS 1.65 OpenGL 1.1 Initialization */
	ReSizeGLScene(255, 191);
	glMaterialShinnyness();
#endif

	glDisable(GL_CULL_FACE); 
	glCullFace (GL_NONE);
	glColorMaterial(GL_FRONT, GL_DIFFUSE); 
	
	glEnable(GL_COLOR_MATERIAL);	//allow to mix both glColor3f + light sources (glVertex + glNormal3f)

	renderCube = true;

	setupTGDSProjectOpenGLDisplayLists();
	return 0;
}


GLfloat boxcol[5][3]=
{
	{1.0f,0.0f,0.0f},{1.0f,0.5f,0.0f},{1.0f,1.0f,0.0f},{0.0f,1.0f,0.0f},{0.0f,1.0f,1.0f}
};

GLfloat topcol[5][3]=
{
	{.5f,0.0f,0.0f},{0.5f,0.25f,0.0f},{0.5f,0.5f,0.0f},{0.0f,0.5f,0.0f},{0.0f,0.5f,0.5f}
};

GLuint	box=-1;				// Storage For The Box Display List
GLuint	top=-1;				// Storage For The Top Display List

GLfloat	xrot=0.0;				// Rotates Cube On The X Axis
GLfloat	yrot=0.0;				// Rotates Cube On The Y Axis

#ifdef WIN32
GLuint	texture[1];			// Storage For 1 Texture
#endif

#ifdef ARM9
#if (defined(__GNUC__) && !defined(__clang__))
__attribute__((optimize("O0")))
#endif
#if (!defined(__GNUC__) && defined(__clang__))
__attribute__((optnone))
#endif
#endif
GLvoid BuildLists()
{
	box=glGenLists(2);									// Generate 2 Different Lists
	glNewList(box,GL_COMPILE);							// Start With The Box List
		glBegin(GL_QUADS);
			// Bottom Face
			glNormal3f( 0.0f,-1.0f, 0.0f);
			glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f, -1.0f, -1.0f);
			glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f, -1.0f, -1.0f);
			glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f, -1.0f,  1.0f);
			glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f, -1.0f,  1.0f);
			// Front Face
			glNormal3f( 0.0f, 0.0f, 1.0f);
			glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f, -1.0f,  1.0f);
			glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f, -1.0f,  1.0f);
			glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f,  1.0f,  1.0f);
			glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f,  1.0f,  1.0f);
			// Back Face
			glNormal3f( 0.0f, 0.0f,-1.0f);
			glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f, -1.0f, -1.0f);
			glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f,  1.0f, -1.0f);
			glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f,  1.0f, -1.0f);
			glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f, -1.0f, -1.0f);
			// Right face
			glNormal3f( 1.0f, 0.0f, 0.0f);
			glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f, -1.0f, -1.0f);
			glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f,  1.0f, -1.0f);
			glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f,  1.0f,  1.0f);
			glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f, -1.0f,  1.0f);
			// Left Face
			glNormal3f(-1.0f, 0.0f, 0.0f);
			glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f, -1.0f, -1.0f);
			glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f, -1.0f,  1.0f);
			glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f,  1.0f,  1.0f);
			glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f,  1.0f, -1.0f);
		glEnd();
	glEndList();
	top=box+1;											// Storage For "Top" Is "Box" Plus One
	glNewList(top,GL_COMPILE);							// Now The "Top" Display List
		glBegin(GL_QUADS);
			// Top Face
			glNormal3f( 0.0f, 1.0f, 0.0f);
			glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f,  1.0f, -1.0f);
			glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f,  1.0f,  1.0f);
			glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f,  1.0f,  1.0f);
			glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f,  1.0f, -1.0f);
		glEnd();
	glEndList();

	glEnable(GL_TEXTURE_2D); // Enable Texture Mapping 
	glEnable(GL_BLEND);

}

GLint DLCIRCLELIGHTSRC=-1;

#ifdef ARM9
#if (defined(__GNUC__) && !defined(__clang__))
__attribute__((optimize("O0")))
#endif
#if (!defined(__GNUC__) && defined(__clang__))
__attribute__((optnone))
#endif
#endif
void setupTGDSProjectOpenGLDisplayLists(){
	DLSOLIDCUBE0_06F = (GLint)glGenLists(1);

	//glut2SolidCube(); -> NDS GX Implementation
	glNewList(DLSOLIDCUBE0_06F, GL_COMPILE);
	{
		float size = 0.06f;
		GLfloat n[6][3] =
		{
			{-1.0f, 0.0f, 0.0f},
			{0.0f, 1.0f, 0.0f},
			{1.0f, 0.0f, 0.0f},
			{0.0f, -1.0f, 0.0f},
			{0.0f, 0.0f, 1.0f},
			{0.0f, 0.0f, -1.0f}
		};
		GLint faces[6][4] =
		{
			{0, 1, 2, 3},
			{3, 2, 6, 7},
			{7, 6, 5, 4},
			{4, 5, 1, 0},
			{5, 6, 2, 1},
			{7, 4, 0, 3}
		};
		GLfloat v[8][3];
		GLint i;

		v[0][0] = v[1][0] = v[2][0] = v[3][0] = -size / 2;
		v[4][0] = v[5][0] = v[6][0] = v[7][0] = size / 2;
		v[0][1] = v[1][1] = v[4][1] = v[5][1] = -size / 2;
		v[2][1] = v[3][1] = v[6][1] = v[7][1] = size / 2;
		v[0][2] = v[3][2] = v[4][2] = v[7][2] = -size / 2;
		v[1][2] = v[2][2] = v[5][2] = v[6][2] = size / 2;

		glScalef(32.0f, 32.0f, 32.0f);
		for (i = 5; i >= 0; i--)
		{
			glBegin(GL_QUADS);
			glNormal3fv(&n[i][0]);
			glTexCoord2f(0, 0);
			glVertex3fv(&v[faces[i][0]][0]);
			glTexCoord2f(1, 0);
			glVertex3fv(&v[faces[i][1]][0]);
			glTexCoord2f(1, 1);
			glVertex3fv(&v[faces[i][2]][0]);
			glTexCoord2f(0, 1);
			glVertex3fv(&v[faces[i][3]][0]);
			glEnd();
		}
	}
	glEndList();
	
	DLCIRCLELIGHTSRC = (GLint)glGenLists(1);
	//drawSphere(); -> NDS GX Implementation
	glNewList(DLCIRCLELIGHTSRC, GL_COMPILE); //recompile a light-based sphere as OpenGL DisplayList for rendering on upper screen later
	{
		float r=1; 
		int lats=8; 
		int longs=8;
		int i, j;
		for (i = 0; i <= lats; i++) {
			float lat0 = PI * (-0.5 + (float)(i - 1) / lats);
			float z0 = sin((float)lat0);
			float zr0 = cos((float)lat0);

			float lat1 = PI * (-0.5 + (float)i / lats);
			float z1 = sin((float)lat1);
			float zr1 = cos((float)lat1);
			glBegin(GL_TRIANGLE_STRIP);
			for (j = 0; j <= longs; j++) {
				float lng = 2 * PI * (float)(j - 1) / longs;
				float x = cos(lng);
				float y = sin(lng);
				glNormal3f(x * zr0, y * zr0, z0);
				glVertex3f(r * x * zr0, r * y * zr0, r * z0);
				glNormal3f(x * zr1, y * zr1, z1);
				glVertex3f(r * x * zr1, r * y * zr1, r * z1);
			}
			glEnd();
		}
	}
	glEndList();
	

	#ifdef _MSC_VER
	glGenTextures(1, texture);
    glBindTexture(GL_TEXTURE_2D, texture[0]);
    load_image("../src/resources/Cube.png");
	#endif

#ifdef ARM9
	//#1: Load a texture and map each one to a texture slot
	u32 arrayOfTextures[1];
	arrayOfTextures[0] = (u32)&Texture_Cube_metal;  
	int texturesInSlot = LoadLotsOfGLTextures((u32*)&arrayOfTextures, (sizeof(arrayOfTextures)/sizeof(u32)) ); //Implements both glBindTexture and glTexImage2D 
	int i = 0;
	for(i = 0; i < texturesInSlot; i++){
		printf("Tex. index: %d: Tex. name[%d]", i, getTextureNameFromIndex(i));
	}
	printf("Free Mem: %d KB", ((int)TGDSARM9MallocFreeMemory()/1024));
	glCallListGX((u32*)&GXPayload); //Run this payload once to force cache flushes on DMA GXFIFO
#endif

	BuildLists();
}

//glutSolidSphere(radius, 16, 16);  -> NDS GX Replacement
void drawSphereCustom(float r, int lats, int longs) {
	int i, j;
	for (i = 0; i <= lats; i++) {
		float lat0 = PI * (-0.5 + (float)(i - 1) / lats);
		float z0 = sin((float)lat0);
		float zr0 = cos((float)lat0);

		float lat1 = PI * (-0.5 + (float)i / lats);
		float z1 = sin((float)lat1);
		float zr1 = cos((float)lat1);
		glBegin(GL_TRIANGLE_STRIP);
		for (j = 0; j <= longs; j++) {
			float lng = 2 * PI * (float)(j - 1) / longs;
			float x = cos(lng);
			float y = sin(lng);
			glNormal3f(x * zr0, y * zr0, z0);
			glVertex3f(r * x * zr0, r * y * zr0, r * z0);
			glNormal3f(x * zr1, y * zr1, z1);
			glVertex3f(r * x * zr1, r * y * zr1, r * z1);
		}
		glEnd();
	}
}

#ifdef ARM9
#if (defined(__GNUC__) && !defined(__clang__))
__attribute__((optimize("O0")))
#endif
#if (!defined(__GNUC__) && defined(__clang__))
__attribute__((optnone))
#endif
#endif
GLvoid ReSizeGLScene(GLsizei width, GLsizei height)		// resizes the window (GLUT & TGDS GL)
{
	if (height==0)										// Prevent A Divide By Zero By
	{
		height=1;										// Making Height Equal One
	}

	glViewport(0,0,width,height);						// Reset The Current Viewport

	glMatrixMode(GL_PROJECTION);						// Select The Projection Matrix
	glLoadIdentity();									// Reset The Projection Matrix

	// Calculate The Aspect Ratio Of The Window
	gluPerspective(45.0f,(GLfloat)width/(GLfloat)height,0.1f,100.0f);

	glMatrixMode(GL_MODELVIEW);							// Select The Modelview Matrix
	glLoadIdentity();									// Reset The Modelview Matrix
}

int startTGDSProject(int argc, char *argv[])
{
	time_t time1 = time(NULL);
	TWLPrintf("-- Program starting: %d\n", (unsigned int)time1);
	srand(time1);
	InitGL(argc, argv);

	// register our call-back functions
	TWLPrintf("-- Registering callbacks\n");

//VS2012
#ifdef _MSC_VER
	glutDisplayFunc(drawScene);
	glutReshapeFunc(ReSizeGLScene);
	glutKeyboardFunc(keyboardInput);
	glutSpecialFunc(keyboardInputSpecial);
#endif

	// create the scene and set perspective projection as default
	initializeScene(&scene);
	
	// 'fake' keys being pressed to enable the state to
	// setup lighting and fog
	keyboardInput((unsigned char)'L', 0, 0);
	keyboardInput((unsigned char)'0', 0, 0);
	keyboardInput((unsigned char)'1', 0, 0);
	keyboardInput((unsigned char)'2', 0, 0);
	keyboardInput((unsigned char)'F', 0, 0);
	
	// start the timer and enter the mail GLUT loop
#ifdef _MSC_VER
	glutTimerFunc(50, animateScene, 0);
	glutMainLoop();
#endif

#if defined(ARM9)
	BgMusicOff();
	BgMusic("0:/bgm.ima");
	startTimerCounter(tUnitsMilliseconds, 1);
    glMaterialShinnyness();
	glReset(); //Depend on GX stack to render scene
	while(1==1){
		//Handle Input & game logic
		scanKeys();
		u32 keys = keysHeld()&(KEY_UP|KEY_DOWN|KEY_LEFT|KEY_RIGHT|KEY_SELECT|KEY_START);
		keyboardInputSpecial(keys, 0, 0);
		
		//Go back to TGDS-multiboot
		if(keysDown() & KEY_L){	
			haltARM7(); //required
			char thisArgv[3][MAX_TGDSFILENAME_LENGTH];
			memset(thisArgv, 0, sizeof(thisArgv));
			strcpy(&thisArgv[0][0], "");	//Arg0:	This Binary loaded
			strcpy(&thisArgv[1][0], "");	//Arg1:	NDS Binary to chainload through TGDS-MB
			strcpy(&thisArgv[2][0], "");	//Arg2: NDS Binary loaded from TGDS-MB	
			char * bootldr = NULL;
			if(__dsimode == true){
				bootldr = "0:/ToolchainGenericDS-multiboot.srl";
			}
			else{
				bootldr = "0:/ToolchainGenericDS-multiboot.nds";
			}
			u32 * payload = getTGDSMBV3ARM7Bootloader();
			if(TGDSMultibootRunNDSPayload(bootldr, (u8*)payload, 0, (char*)&thisArgv) == false){ //should never reach here, nor even return true. Should fail it returns false
				
			}
			while(keysDown() & KEY_L){
				scanKeys();
			}
		}
		
		if(keysDown() & KEY_R){	
			GUI.GBAMacroMode = !GUI.GBAMacroMode; //swap LCD
			TGDSLCDSwap();
			
			while(keysDown() & KEY_R){
				scanKeys();
			}
		}
		
		//sound (ARM7)
		
		//Render
		drawScene();
	}
#endif
	return 0;
}