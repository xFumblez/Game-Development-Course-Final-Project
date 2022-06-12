// Matthew Taylor

//modified by:
//date:
//
//author: Gordon Griesel
//date: Spring 2022
//purpose: get openGL working on your personal computer
//

#include <iostream>
#include <fstream>

using namespace std;

#include <stdio.h>
#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <cmath>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/glx.h>
#include <pthread.h>
#include "fonts.h"

//MVC architecture
//M - Model
//V - View
//C - Controller

struct Vector {
    float x, y, z;
};

class Image
{
    public:
        int width, height, max;
        char *data;
        Image() { }
        Image (const char *fname)
        {
            char str[1200];
            char newfile[200];
            ifstream fin;
            char *p = strstr((char *)fname, ".ppm");
            if (!p)
            {
                //not a ppm file
                strcpy(newfile, fname);
                newfile[strlen(newfile)-4] = '\0';
                strcat(newfile, ".ppm");
                sprintf(str, "convert %s %s", fname, newfile);
                system(str);
                fin.open(newfile, ios::in|ios::out);
            }
            else
            {
                fin.open(fname);
            }
            char p6[10];
            fin >> p6;
            fin >> width >> height;
            fin >> max;
            data = new char [width * height * 3];
            fin.read(data, 1);
            fin.read(data, width * height * 3);
            fin.close();
        }
} player_walk("/home/stu/mtaylor/4490/proj/spritewalk.ppm"),
  player_idle("/home/stu/mtaylor/4490/proj/idle.ppm"),
  player_jump("/home/stu/mtaylor/4490/proj/jump.ppm"),
  background("/home/stu/mtaylor/4490/proj/background.ppm"),
  main_menu("/home/stu/mtaylor/4490/proj/main_menu_bg.ppm"),
  game_over_screen("/home/stu/mtaylor/4490/proj/game_over.ppm"),
  start_button("/home/stu/mtaylor/4490/proj/start.ppm"),
  rules_button("/home/stu/mtaylor/4490/proj/rules.ppm"),
  retry_button("/home/stu/mtaylor/4490/proj/retry.ppm"),
  selection("/home/stu/mtaylor/4490/proj/select.ppm");


//a game object/model
class Object {
    public:
        float pos[3];      //vector
        float velocity[3]; //vector
        float w, h;
        unsigned int color;
        bool alive_or_dead;

};

class Box {
    public:
    float pos[3];      //vector
    float velocity[3]; //vector
    float width, height;
    float color[3];
    bool alive_or_dead;
    Box () { }
    Box(float r, float g, float b)
    {
        width = 100;
        height = 10;
        pos[0] = rand() % 200;
        pos[1] = rand() % 200;
        velocity[0] = 0;
        velocity[1] = 0;
        velocity[2] = 0;
        color[0] = r;
        color[1] = g;
        color[2] = b;
    }

};

typedef double Flt;

// Player game object
class Player
{
    public:
        Flt pos[3];
        Flt vel[3];
        float last_x_velocity;
        float w, h;
        unsigned int color;
        bool alive_or_dead;

        bool isGrounded;
        bool isJumping;
        float jump_start_pos;
        int max_jump;

        Player()
        {
            w = h = 4.0;
            pos[0] = 0.0f + w;
            pos[1] = 0;
            vel[0] = 0.0f;
            vel[1] = 0.0;
            isJumping = 0;
            isGrounded = 1;
            jump_start_pos = 0.0f;
            max_jump = 130;
        }
        void set_dimensions(int x, int y)
        {
            w = (float)x * 0.05;
            h = 2 * w;
        }
};

enum {
    STATE_TOP,
    STATE_INTRO,
    STATE_INSTRUCTIONS,
    STATE_SHOW_OBJECTIVES,
    STATE_PLAY,
    STATE_GAME_OVER,
    STATE_BOTTOM
};

class Global {
public:
	int xres, yres;
    Object select_hand, start, rules, retry;
    Player player;
    Box platform[10];
    Box goal;
    Box obstacle[5];
    //box components
	int nplatforms = 10;
    int nobstacles = 5;
    float pos[2];
    float select_pos[3];
	float w;
	float dir;
    int inside;
    int state;
    int playtime;
    int starttime;
    int countdown;

    unsigned int mm_background;
    unsigned int select_id;
    unsigned int start_id;
    unsigned int rules_id;
    
    unsigned int game_over;
    unsigned int retry_id;

    unsigned int texid;
    unsigned int playerid_walk;
    unsigned int playerid_idle;
    unsigned int playerid_jump;

    Flt gravity;
    int frameno;

    Global();
} g;

class X11_wrapper {
private:
	Display *dpy;
	Window win;
	GLXContext glc;
public:
	~X11_wrapper();
	X11_wrapper();
	void set_title();
	bool getXPending();
	XEvent getXNextEvent();
	void swapBuffers();
	void reshape_window(int width, int height);
	void check_resize(XEvent *e);
	void check_mouse(XEvent *e);
	int check_keys(XEvent *e);
} x11;

//Function prototypes
void init_opengl(void);
void physics(void);
void render(void);

void *spriteThread(void *arg)
{
    //Setup timers
    //const double oobillion = 1.0 / 1e9;
    struct timespec start, end;
    extern double timeDiff(struct timespec *start, struct timespec *end);
    extern void timeCopy(struct timespec *dest, struct timespec *source);
    //clock_gettime(CLOCK_REALTIME, &smokeStart);

    clock_gettime(CLOCK_REALTIME, &start);
    double diff;
    while(1)
    {
        //if some amount of time has passed, change the frame number.
        clock_gettime(CLOCK_REALTIME, &end);
        diff = timeDiff(&start, &end);
        if (diff >= 0.05)
        {
            //enough time has passed
            ++g.frameno;
            if(g.frameno > 10) //if greater than total frames
                g.frameno = 1;
            timeCopy(&start, &end);
        }
    }

    return (void *)0;
}

int main()
{
    //start thread
    pthread_t th;
    pthread_create(&th, NULL, spriteThread, NULL);

	init_opengl();
    //main game loop
	int done = 0;
	while (!done) {
		//process events...
		while (x11.getXPending()) {
			XEvent e = x11.getXNextEvent();
			x11.check_resize(&e);
			x11.check_mouse(&e);
			done = x11.check_keys(&e);
		}
		if (g.state == STATE_PLAY)
        {
            //check countdown timer
            g.countdown = time(NULL) - g.starttime;
            //if (g.countdown > g.playtime)
                //g.state = STATE_GAME_OVER;
        }
        physics();           //move things
		render();            //draw things
		x11.swapBuffers();   //make video memory visible
		usleep(200);         //pause to let X11 work better
	}
    cleanup_fonts();
	return 0;
}

Global::Global()
{
	xres = 1000;
	yres = 700;

    //dir = 0.00001f;

    //platform setup
    for (int i = 0; i < nplatforms; i++)
    {
        platform[i].width = 50;
        platform[i].height = 15;
        platform[i].velocity[0] = 0;
        platform[i].velocity[1] = 0;
        platform[i].velocity[2] = 0;
        platform[i].color[0] = 0;
        platform[i].color[1] = 1;
        platform[i].color[2] = 0;
    }

    platform[0].pos[0] = 150;
    platform[0].pos[1] = 100;
    platform[1].pos[0] = 300;
    platform[1].pos[1] = 150;
    platform[2].pos[0] = 400;
    platform[2].pos[1] = 150;
    platform[3].pos[0] = 500;
    platform[3].pos[1] = 150;
    platform[4].pos[0] = 500;
    platform[4].pos[1] = 200;
    platform[5].pos[0] = 600;
    platform[5].pos[1] = 200;
    platform[6].pos[0] = 600;
    platform[6].pos[1] = 200;
    platform[7].pos[0] = 650;
    platform[7].pos[1] = 200;
    platform[8].pos[0] = 675;
    platform[8].pos[1] = 200;
    platform[9].pos[0] = 700;
    platform[9].pos[1] = 200;

    for (int i = 0; i < nobstacles; i++)
    {
        obstacle[i].width = 15;
        obstacle[i].height = 15;
        obstacle[i].velocity[0] = 0;
        obstacle[i].velocity[1] = 0;
        obstacle[i].velocity[2] = 0;
        obstacle[i].color[0] = 1;
        obstacle[i].color[1] = 0;
        obstacle[i].color[2] = 0;
    }

    obstacle[0].pos[0] = 200;
    obstacle[0].pos[1] = 150;
    obstacle[1].pos[0] = 200;
    obstacle[1].pos[1] = 150;
    obstacle[2].pos[0] = 350;
    obstacle[2].pos[1] = 200;
    obstacle[3].pos[0] = 350;
    obstacle[3].pos[1] = 200;
    obstacle[4].pos[0] = 575;
    obstacle[4].pos[1] = 250;
    
    goal.width = 15;
    goal.height = 15;
    goal.velocity[0] = 0;
    goal.velocity[1] = 0;
    goal.velocity[2] = 0;
    goal.color[0] = 0;
    goal.color[1] = 0;
    goal.color[2] = 1;
    goal.pos[0] = 850;
    goal.pos[1] = 150;
    
    select_pos[0] = yres/2 + 70;
    select_pos[1] = yres/2;
    select_pos[2] = xres/2 - 80;

    //select_hand
    w = 20.0f;
    g.select_hand.pos[0] = xres/8;
    g.select_hand.pos[1] = yres/2 + 70;
    g.select_hand.w = 20;
    g.select_hand.h = 20;

    //start button
    g.start.pos[0] = xres/8 + 80;
    g.start.pos[1] = yres/2 + 70;
    g.start.w = 70;
    g.start.h = 25;
    
    //rules button
    g.rules.pos[0] = xres/8 + 80;
    g.rules.pos[1] = yres/2;
    g.rules.w = 70;
    g.rules.h = 25; 
    
    //retry button
    g.retry.pos[0] = xres/2;
    g.retry.pos[1] = yres/2;
    g.retry.w = 70;
    g.retry.h = 25;

    //player
    g.player.pos[0] = 0.0f+g.player.w;
    g.player.pos[1] = 0.0f+g.player.h;
	dir = 25.0f;
    //score
    inside = 0;
    state = STATE_INTRO;

    gravity = 25.0;
    frameno = 1;
}

X11_wrapper::~X11_wrapper()
{
	XDestroyWindow(dpy, win);
	XCloseDisplay(dpy);
}

X11_wrapper::X11_wrapper()
{
	GLint att[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
	int w = g.xres, h = g.yres;
	dpy = XOpenDisplay(NULL);
	if (dpy == NULL) {
		cout << "\n\tcannot connect to X server\n" << endl;
		exit(EXIT_FAILURE);
	}
	Window root = DefaultRootWindow(dpy);
	XVisualInfo *vi = glXChooseVisual(dpy, 0, att);
	if (vi == NULL) {
		cout << "\n\tno appropriate visual found\n" << endl;
		exit(EXIT_FAILURE);
	} 
	Colormap cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);
	XSetWindowAttributes swa;
	swa.colormap = cmap;
	swa.event_mask =
		ExposureMask | KeyPressMask | KeyReleaseMask |
		ButtonPress | ButtonReleaseMask |
		PointerMotionMask |
		StructureNotifyMask | SubstructureNotifyMask;
	win = XCreateWindow(dpy, root, 0, 0, w, h, 0, vi->depth,
		InputOutput, vi->visual, CWColormap | CWEventMask, &swa);
	set_title();
	glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
	glXMakeCurrent(dpy, win, glc);
}

void X11_wrapper::set_title()
{
	//Set the window title bar.
	XMapWindow(dpy, win);
	XStoreName(dpy, win, "4490 Lab1");
}

bool X11_wrapper::getXPending()
{
	//See if there are pending events.
	return XPending(dpy);
}

XEvent X11_wrapper::getXNextEvent()
{
	//Get a pending event.
	XEvent e;
	XNextEvent(dpy, &e);
	return e;
}

void X11_wrapper::swapBuffers()
{
	glXSwapBuffers(dpy, win);
}

void X11_wrapper::reshape_window(int width, int height)
{
	//window has been resized.
	g.xres = width;
	g.yres = height;
	//
	glViewport(0, 0, (GLint)width, (GLint)height);
	glMatrixMode(GL_PROJECTION); glLoadIdentity();
	glMatrixMode(GL_MODELVIEW); glLoadIdentity();
	glOrtho(0, g.xres, 0, g.yres, -1, 1);
    g.player.set_dimensions(g.xres, g.yres);
}

void X11_wrapper::check_resize(XEvent *e)
{
	//The ConfigureNotify is sent by the
	//server if the window is resized.
	if (e->type != ConfigureNotify)
		return;
	XConfigureEvent xce = e->xconfigure;
	if (xce.width != g.xres || xce.height != g.yres) {
		//Window size did change.
		reshape_window(xce.width, xce.height);
	}
}
//-----------------------------------------------------------------------------

void X11_wrapper::check_mouse(XEvent *e)
{
	static int savex = 0;
	static int savey = 0;

	//Weed out non-mouse events
	if (e->type != ButtonRelease &&
		e->type != ButtonPress &&
		e->type != MotionNotify) {
		//This is not a mouse event that we care about.
		return;
	}
	//
	if (e->type == ButtonRelease) {
		return;
	}
	if (e->type == ButtonPress) {
		if (e->xbutton.button==1) {
			//Left button was pressed.
			//int y = g.yres - e->xbutton.y;
            //int x = e->xbutton.x;
            if (g.state == STATE_INTRO)
            {

            }

            else if (g.state == STATE_PLAY)
            {

            }
            
            return;
		}
		if (e->xbutton.button==3) {
			//Right button was pressed.

			return;
		}
	}
	if (e->type == MotionNotify) {
		//The mouse moved!
		if (savex != e->xbutton.x || savey != e->xbutton.y) {
			savex = e->xbutton.x;
			savey = e->xbutton.y;
			//Code placed here will execute whenever the mouse moves.


		}
	}
}

int X11_wrapper::check_keys(XEvent *e)
{
	if (e->type != KeyPress && e->type != KeyRelease)
		return 0;
	int key = XLookupKeysym(&e->xkey, 0);
	if (e->type == KeyPress) {
		switch (key) {
            case XK_Up:
                if (g.state == STATE_INTRO)
                {
                    g.select_hand.pos[1] = g.select_pos[0];
                }
                break;
            case XK_Down:
                if (g.state == STATE_INTRO)
                {
                    g.select_hand.pos[1] = g.select_pos[1];
                }
                break;
            case XK_Return:
                if (g.state == STATE_INTRO)
                {
                    if (g.select_hand.pos[1] == g.select_pos[0])
                    {
                        g.state = STATE_PLAY;
                        g.starttime = time(NULL);
                        g.playtime = 10;
                        g.player.pos[0] = 0.0f+g.player.w;
                        g.player.pos[1] = 0.0f+g.player.h;
                        g.player.last_x_velocity = 1.0f;
                    }
                    else if (g.select_hand.pos[1] == g.select_pos[1])
                    {
                        g.state = STATE_INSTRUCTIONS;
                    }
                }
                else if (g.state == STATE_PLAY)
                {
                    g.state = STATE_GAME_OVER;
                }
                else if (g.state == STATE_GAME_OVER)
                {
                    g.state = STATE_INTRO;
                }
                break;
            case XK_Left:
            case XK_a:
                if (g.state == STATE_PLAY)
                {
                    g.player.vel[0] = -25.0f;
                    g.player.last_x_velocity = -1.0f;
                }
                break;
            case XK_Right:
            case XK_d:
                if (g.state == STATE_PLAY)
                {
                    g.player.vel[0] = 25.0f;;
                    g.player.last_x_velocity = 1.0f;
                }
                break;
            case XK_space:
                if (g.state == STATE_PLAY)
                {
                    if (g.player.isGrounded)
                    {
                        g.player.isJumping = 1;
                        g.player.jump_start_pos = g.player.pos[1];
                    }
                }
                break;
            case XK_BackSpace:
                if (g.state == STATE_INSTRUCTIONS)
                {
                    g.state = STATE_INTRO;
                }
                break;
            case XK_1:
				//Key 1 was pressed
				break;
			case XK_Escape:
				//Escape key was pressed
				return 1;
            case XK_s:
                break;
		}
	}
    else if (e->type == KeyRelease)
    {
        switch (key)
        {
            case XK_Left:
            case XK_a:
                g.player.last_x_velocity = -1.0f;
                g.player.vel[0] = 0.0f;
                break;
            case XK_Right:
            case XK_d:
                g.player.last_x_velocity = 1.0f;
                g.player.vel[0] = 0.0f;
                break;
            case XK_space:
                if (g.player.isJumping)
                    g.player.isJumping = 0;
                break;
        }
    }
	return 0;
}

unsigned char *buildAlphaData(Image *img)
{
    //add 4th component to RGB stream...
    int i;
    int a,b,c;
    unsigned char *newdata, *ptr;
    unsigned char *data = (unsigned char *)img->data;
    newdata = (unsigned char *)malloc(img->width * img->height * 4);
    ptr = newdata;
    
    for (i=0; i<img->width * img->height * 3; i+=3) {
        a = *(data+0);
        b = *(data+1);
        c = *(data+2);
        *(ptr+0) = a;
        *(ptr+1) = b;
        *(ptr+2) = c;
        
        *(ptr+3) = (a==0 && b==0 && c==255); // transparency on full blue background
        
        ptr += 4;
        data += 3;
    }
    
    return newdata;
}

void init_opengl(void)
{
	//OpenGL initialization
	glViewport(0, 0, g.xres, g.yres);
	//Initialize matrices
	glMatrixMode(GL_PROJECTION); glLoadIdentity();
	//Set 2D mode (no perspective)
	glOrtho(0, g.xres, 0, g.yres, -1, 1);
	//
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    //Set the screen background color
	glClearColor(0.1, 0.1, 0.1, 1.0);
    //allow 2D textue maps
    glEnable(GL_TEXTURE_2D);
    initialize_fonts();

    // Background
    glGenTextures(1, &g.texid);
    glBindTexture(GL_TEXTURE_2D, g.texid);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, 3, background.width, background.height, 0,
            GL_RGB, GL_UNSIGNED_BYTE, background.data);
    
    
    // Main Menu Background
    glGenTextures(1, &g.mm_background);
    glBindTexture(GL_TEXTURE_2D, g.mm_background);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, 3, main_menu.width, main_menu.height, 0,
            GL_RGB, GL_UNSIGNED_BYTE, main_menu.data);
    
    
    // Game Over Screen

    glGenTextures(1, &g.game_over);
    glBindTexture(GL_TEXTURE_2D, g.game_over);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, 3, game_over_screen.width, game_over_screen.height, 0,
            GL_RGB, GL_UNSIGNED_BYTE, game_over_screen.data);


    // Start Button

    unsigned char *startdata = buildAlphaData(&start_button);
    
    glGenTextures(1, &g.start_id);
    glBindTexture(GL_TEXTURE_2D, g.start_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, 4, start_button.width, start_button.height, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, startdata);
    
    delete [] startdata;

    
    // Rules Button

    unsigned char *rulesdata = buildAlphaData(&rules_button);
    
    glGenTextures(1, &g.rules_id);
    glBindTexture(GL_TEXTURE_2D, g.rules_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, 4, rules_button.width, rules_button.height, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, rulesdata);
    
    delete [] rulesdata;
    
    
    // Retry Button
    
    unsigned char *retrydata = buildAlphaData(&retry_button);
    
    glGenTextures(1, &g.retry_id);
    glBindTexture(GL_TEXTURE_2D, g.retry_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, 4, retry_button.width, retry_button.height, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, retrydata);
    
    delete [] retrydata;
    
    
    // Selection Hand

    unsigned char *selectdata = buildAlphaData(&selection);
    
    glGenTextures(1, &g.select_id);
    glBindTexture(GL_TEXTURE_2D, g.select_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, 4, selection.width, selection.height, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, selectdata);
    
    delete [] selectdata;




    // Player Sprites
    
    unsigned char *data2 = buildAlphaData(&player_walk);
    
    glGenTextures(1, &g.playerid_walk);
    glBindTexture(GL_TEXTURE_2D, g.playerid_walk);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, 4, player_walk.width, player_walk.height, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, data2);
    
    delete [] data2;
    

    unsigned char *data3 = buildAlphaData(&player_idle);
    
    glGenTextures(1, &g.playerid_idle);
    glBindTexture(GL_TEXTURE_2D, g.playerid_idle);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, 4, player_idle.width, player_idle.height, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, data3);
    
    delete [] data3;
    
    unsigned char *data4 = buildAlphaData(&player_jump);
    
    glGenTextures(1, &g.playerid_jump);
    glBindTexture(GL_TEXTURE_2D, g.playerid_jump);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, 4, player_jump.width, player_jump.height, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, data4);
    
    delete [] data4;
    
    g.player.set_dimensions(g.xres, g.yres);

}

void physics()
{
    if (g.state == STATE_PLAY)
    {
        if (g.player.isJumping == 0 && (g.player.pos[1] - g.player.jump_start_pos) == 0.0f)
        {
            g.player.isGrounded = 1;
        }
        
        if (g.player.isJumping == 1)
        {
            g.player.isGrounded = 0;
            g.player.vel[1] = 50.0f;

            if (g.player.pos[1] - g.player.jump_start_pos >= g.player.max_jump)
            {
                g.player.pos[1] = g.player.jump_start_pos + g.player.max_jump;
                g.player.isJumping = 0;
            }
        }
        
        if (g.player.isJumping == 0)
        {
            g.player.vel[1] -= g.gravity;
        }
        
        g.player.pos[0] += g.player.vel[0];
        g.player.pos[1] += g.player.vel[1];

        //boundary test
        if (g.player.pos[0] >= g.xres - g.player.w) {
            g.player.pos[0] = g.xres - g.player.w;
        }
        if (g.player.pos[0] <= 0 + g.player.w) {
            g.player.pos[0] = 0 + g.player.w;
        }
        if (g.player.pos[1] >= g.yres) {
            g.player.pos[1] = g.yres - g.player.h;
            g.player.vel[1] = 0.0;
        }
        if (g.player.pos[1] <= nearbyint(0 + g.player.h)) {
            g.player.pos[1] = 0 + g.player.h;
            g.player.vel[1] = 0.0;
            g.player.isGrounded = 1;
            g.player.isJumping = 0;
            g.player.jump_start_pos = g.player.pos[1];
        }
        
        
        for (int i = 0; i < g.nplatforms; i++)
        {
        
            if (g.player.pos[0] - g.player.w < g.platform[i].pos[0] + g.platform[i].width &&
                g.player.pos[0] + g.player.w > g.platform[i].pos[0] - g.platform[i].width &&
                g.player.pos[1] - g.player.h < g.platform[i].pos[1] + g.platform[i].height &&
                g.player.pos[1] + g.player.h > g.platform[i].pos[1] - g.platform[i].height)
            {
                if (g.player.pos[0] <= g.platform[i].pos[0] - g.platform[i].width - g.player.w)
                {
                    g.player.pos[0] = g.platform[i].pos[0] - g.platform[i].width - g.player.w; 
                }
                if (g.player.pos[0] >= g.platform[i].pos[0] + g.platform[i].width + g.player.w)
                {
                    g.player.pos[0] = g.platform[i].pos[0] + g.platform[i].width + g.player.w; 
                }
                if (g.player.pos[0] - g.player.w < g.platform[i].pos[0] + g.platform[i].width &&
                    g.player.pos[0] + g.player.w > g.platform[i].pos[0] - g.platform[i].width &&
                    g.player.pos[1] - g.player.h <= g.platform[i].pos[1] + g.platform[i].height + g.player.h)
                {
                    g.player.pos[1] = g.platform[i].pos[1] + g.platform[i].height + g.player.h - 10;
                    g.player.isGrounded = 1;
                    g.player.isJumping = 0;
                    g.player.jump_start_pos = g.player.pos[1];
                    g.player.vel[1] = 0.0;
                }
            }

        }

        for (int i = 0; i < g.nobstacles; i++)
        {
        
            if (g.player.pos[0] - g.player.w < g.obstacle[i].pos[0]  &&
                g.player.pos[0] + g.player.w > g.obstacle[i].pos[0]  &&
                g.player.pos[1] - g.player.h < g.obstacle[i].pos[1] + g.obstacle[i].height  &&
                g.player.pos[1] + g.player.h > g.obstacle[i].pos[1] - g.obstacle[i].width)
            {
                g.state = STATE_GAME_OVER;
                g.player.pos[0] = 0.0f+g.player.w;
                g.player.pos[1] = 0.0f+g.player.h;
                g.player.last_x_velocity = 1.0f;
            }
        }
        
        if (g.player.pos[0] - g.player.w < g.goal.pos[0]  &&
            g.player.pos[0] + g.player.w > g.goal.pos[0]  &&
            g.player.pos[1] - g.player.h < g.goal.pos[1] + g.goal.height  &&
            g.player.pos[1] + g.player.h > g.goal.pos[1] - g.goal.width)
        {
            g.state = STATE_GAME_OVER;
            g.player.pos[0] = 0.0f+g.player.w;
            g.player.pos[1] = 0.0f+g.player.h;
            g.player.last_x_velocity = 1.0f;
        }

    }

}

void render()
{
    glClear(GL_COLOR_BUFFER_BIT); 
    
    Rect r;
    
    if (g.state == STATE_INTRO)
    {
        // main menu background
        glColor3ub(255, 255, 255);
        glBindTexture(GL_TEXTURE_2D, g.mm_background);
        glBegin(GL_QUADS);
            glTexCoord2f(0,1); glVertex2i(0,0);
            glTexCoord2f(0,0); glVertex2i(0,g.yres);
            glTexCoord2f(1,0); glVertex2i(g.xres,g.yres);
            glTexCoord2f(1,1); glVertex2i(g.xres,0);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, 0);
        
        
        // Prepare texture coordinates for main menu images
        float tx1 = 0.0f;
        float tx2 = 1.0f;
        float ty1 = 0.0f;
        float ty2 = 1.0f;
        
        
        // Draw Select Hand
        glPushMatrix();
        glColor3ub(255, 255, 255);
        glTranslatef(g.select_hand.pos[0], g.select_hand.pos[1], 0.0f);
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_EQUAL, 0.0f);
        glColor4ub(255,255,255,255);
        
        glBindTexture(GL_TEXTURE_2D, g.select_id);
        glBegin(GL_QUADS);
            glTexCoord2f(tx1, ty2); glVertex2f(-g.select_hand.w, -g.select_hand.h);
            glTexCoord2f(tx1, ty1); glVertex2f(-g.select_hand.w,  g.select_hand.h);
            glTexCoord2f(tx2, ty1); glVertex2f( g.select_hand.w,  g.select_hand.h);
            glTexCoord2f(tx2, ty2); glVertex2f( g.select_hand.w, -g.select_hand.h);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_ALPHA_TEST);
        glPopMatrix();
        
        //show the intro
        r.bot = g.yres - 100;
        r.left = g.xres / 3;
        r.center = 1;
        ggprint40(&r, 0, 0x00ffffff, "Welcome to Not Scott Pilgrim");
        //ggprint16(&r, 20, 0x00ff0000, "Press 's' to start");
        //ggprint16(&r,  0, 0x00ff0000, "Press 'i' for instructions");


        // Draw Start Button
        
        glPushMatrix();
        glColor3ub(255, 255, 255);
        glTranslatef(g.start.pos[0], g.start.pos[1], 0.0f);
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_EQUAL, 0.0f);
        glColor4ub(255,255,255,255);
        
        glBindTexture(GL_TEXTURE_2D, g.start_id);
        glBegin(GL_QUADS);
            glTexCoord2f(tx1, ty2); glVertex2f(-g.start.w, -g.start.h);
            glTexCoord2f(tx1, ty1); glVertex2f(-g.start.w,  g.start.h);
            glTexCoord2f(tx2, ty1); glVertex2f( g.start.w,  g.start.h);
            glTexCoord2f(tx2, ty2); glVertex2f( g.start.w, -g.start.h);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_ALPHA_TEST);
        glPopMatrix();

        // Draw Rules Button
        
        glPushMatrix();
        glColor3ub(255, 255, 255);
        glTranslatef(g.rules.pos[0], g.rules.pos[1], 0.0f);
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_EQUAL, 0.0f);
        glColor4ub(255,255,255,255);
        
        glBindTexture(GL_TEXTURE_2D, g.rules_id);
        glBegin(GL_QUADS);
            glTexCoord2f(tx1, ty2); glVertex2f(-g.rules.w, -g.rules.h);
            glTexCoord2f(tx1, ty1); glVertex2f(-g.rules.w,  g.rules.h);
            glTexCoord2f(tx2, ty1); glVertex2f( g.rules.w,  g.rules.h);
            glTexCoord2f(tx2, ty2); glVertex2f( g.rules.w, -g.rules.h);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_ALPHA_TEST);
        glPopMatrix();
        
        return;
    }

    if (g.state == STATE_INSTRUCTIONS)
    {
        // main menu background
        glColor3ub(255, 255, 255);
        glBindTexture(GL_TEXTURE_2D, g.mm_background);
        glBegin(GL_QUADS);
            glTexCoord2f(0,1); glVertex2i(0,0);
            glTexCoord2f(0,0); glVertex2i(0,g.yres);
            glTexCoord2f(1,0); glVertex2i(g.xres,g.yres);
            glTexCoord2f(1,1); glVertex2i(g.xres,0);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, 0);
        
        //show the instructions
        r.bot = g.yres - 100;
        r.left = g.xres / 3;
        r.center = 1;
        ggprint16(&r, 30, 0x00ff0000, "Use A/D or Left/Right Arrow Keys to Move.");
        ggprint16(&r, 30, 0x00ff0000, "Use Spacebar to Jump, Hold for Higher Jump.");
        ggprint16(&r, 30, 0x00ff0000, "Avoid the obstacles and reach the end.");
        ggprint16(&r, 30, 0x00ff0000, "Press Backspace to return to start screen.");
    }

    if (g.state == STATE_GAME_OVER)
    {
        r.bot = g.yres / 2;
        r.left = g.xres / 2;
        r.center = 1;
        //ggprint8b(&r, 20, 0x00ffffff, "GAME OVER");
        //ggprint8b(&r, 30, 0x00ff0000, "Total Score: %i", g.inside);
        //ggprint8b(&r, 0, 0x0000ff00, "Right-click to play again");
        
        // Prepare texture coordinates for game over images
        float tx1 = 0.0f;
        float tx2 = 1.0f;
        float ty1 = 0.0f;
        float ty2 = 1.0f;
        
        // Draw Game Over Background
        
        glColor3ub(255, 255, 255);
        glBindTexture(GL_TEXTURE_2D, g.game_over);
        glBegin(GL_QUADS);
            glTexCoord2f(0,1); glVertex2i(0,0);
            glTexCoord2f(0,0); glVertex2i(0,g.yres);
            glTexCoord2f(1,0); glVertex2i(g.xres,g.yres);
            glTexCoord2f(1,1); glVertex2i(g.xres,0);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, 0);


        // Draw Select Hand
        glPushMatrix();
        glColor3ub(255, 255, 255);
        glTranslatef(g.select_pos[2], g.select_pos[1], 0.0f);
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_EQUAL, 0.0f);
        glColor4ub(255,255,255,255);
        
        glBindTexture(GL_TEXTURE_2D, g.select_id);
        glBegin(GL_QUADS);
            glTexCoord2f(tx1, ty2); glVertex2f(-g.select_hand.w, -g.select_hand.h);
            glTexCoord2f(tx1, ty1); glVertex2f(-g.select_hand.w,  g.select_hand.h);
            glTexCoord2f(tx2, ty1); glVertex2f( g.select_hand.w,  g.select_hand.h);
            glTexCoord2f(tx2, ty2); glVertex2f( g.select_hand.w, -g.select_hand.h);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_ALPHA_TEST);
        glPopMatrix();

        
        // Draw Retry Button
        glPushMatrix();
        glColor3ub(255, 255, 255);
        glTranslatef(g.retry.pos[0], g.retry.pos[1], 0.0f);
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_EQUAL, 0.0f);
        glColor4ub(255,255,255,255);
        
        glBindTexture(GL_TEXTURE_2D, g.retry_id);
        glBegin(GL_QUADS);
            glTexCoord2f(tx1, ty2); glVertex2f(-g.retry.w, -g.retry.h);
            glTexCoord2f(tx1, ty1); glVertex2f(-g.retry.w,  g.retry.h);
            glTexCoord2f(tx2, ty1); glVertex2f( g.retry.w,  g.retry.h);
            glTexCoord2f(tx2, ty2); glVertex2f( g.retry.w, -g.retry.h);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_ALPHA_TEST);
        glPopMatrix();

        
        return;

    }
    
    if (g.state == STATE_PLAY)
    {
        // background
        glColor3ub(255, 255, 255);
        glBindTexture(GL_TEXTURE_2D, g.texid);
        glBegin(GL_QUADS);
            glTexCoord2f(0,1); glVertex2i(0,0);
            glTexCoord2f(0,0); glVertex2i(0,g.yres);
            glTexCoord2f(1,0); glVertex2i(g.xres,g.yres);
            glTexCoord2f(1,1); glVertex2i(g.xres,0);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, 0);
        
        r.bot = g.yres - 20;
        r.left = 10;
        r.center = 0;
        ggprint8b(&r, 30, 0x00ffffff, "Score: %i", g.inside);
        ggprint8b(&r, 0, 0x00ffff00, "Time: %i", g.playtime - g.countdown);
        
        

	    //Draw box.

        for (int i = 0; i < g.nplatforms; i++)
        {
            glPushMatrix();
            glTranslatef(g.platform[i].pos[0], g.platform[i].pos[1], 0.0f);
            glColor3fv(g.platform[i].color);
            glBegin(GL_QUADS);
                glVertex2f(-g.platform[i].width, -g.platform[i].height);
                glVertex2f(-g.platform[i].width,  g.platform[i].height);
                glVertex2f( g.platform[i].width,  g.platform[i].height);
                glVertex2f( g.platform[i].width, -g.platform[i].height);
            glEnd();
            glPopMatrix();
        } 
        
        for (int i = 0; i < g.nobstacles; i++)
        {
            glPushMatrix();
            glTranslatef(g.obstacle[i].pos[0], g.obstacle[i].pos[1], 0.0f);
            glColor3fv(g.obstacle[i].color);
            glBegin(GL_QUADS);
                glVertex2f(-g.obstacle[i].width, -g.obstacle[i].height);
                glVertex2f(-g.obstacle[i].width,  g.obstacle[i].height);
                glVertex2f( g.obstacle[i].width,  g.obstacle[i].height);
                glVertex2f( g.obstacle[i].width, -g.obstacle[i].height);
            glEnd();
            glPopMatrix();
        }

        glPushMatrix();
        glTranslatef(g.goal.pos[0], g.goal.pos[1], 0.0f);
        glColor3fv(g.goal.color);
        glBegin(GL_QUADS);
            glVertex2f(-g.goal.width, -g.goal.height);
            glVertex2f(-g.goal.width,  g.goal.height);
            glVertex2f( g.goal.width,  g.goal.height);
            glVertex2f( g.goal.width, -g.goal.height);
        glEnd();
        glPopMatrix();
        
        
        // Draw player
        glPushMatrix();
        glColor3ub(255, 255, 255);
        glTranslatef(g.player.pos[0], g.player.pos[1], 0.0f);

        //https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/glAlphaFunc.xml
        glEnable(GL_ALPHA_TEST);
        
        //transparent if alpha value is greater than 0.0
        glAlphaFunc(GL_EQUAL, 0.0f);
        
        //Set 4-channels of color intensity
        glColor4ub(255,255,255,255);
        
        if (abs(g.player.vel[0]) > 0.0f && abs((nearbyint(g.player.vel[1]))) == 0 && g.player.isGrounded)
        {
            glBindTexture(GL_TEXTURE_2D, g.playerid_walk);
        
            //make texture coordinates based on frame number.
            float tx1 = (float)((g.frameno-1) % 8) * 0.125f;
            float tx2 = tx1 + 0.125f;
            float ty1 = 0.0 + (float)((g.frameno-1) / 1) * 1.0f;
            float ty2 = ty1 + 0.5f;
            if (g.player.vel[0] < 0.0)
            {
                float temp_tx = tx2;
                tx2 = tx1;
                tx1 = temp_tx;
            }
            glBegin(GL_QUADS);
                glTexCoord2f(tx1, ty2); glVertex2f(-g.player.w, -g.player.h);
                glTexCoord2f(tx1, ty1); glVertex2f(-g.player.w,  g.player.h);
                glTexCoord2f(tx2, ty1); glVertex2f( g.player.w,  g.player.h);
                glTexCoord2f(tx2, ty2); glVertex2f( g.player.w, -g.player.h);
            glEnd();
        
        }
        
        else if (abs(g.player.vel[0]) == 0.0f && abs((nearbyint(g.player.vel[1]))) == 0 && g.player.isGrounded)
        {
            glBindTexture(GL_TEXTURE_2D, g.playerid_idle);
        
            //make texture coordinates based on frame number.
            float tx1 = 0.0f;
            float tx2 = 1.0f;
            float ty1 = 0.0f;
            float ty2 = 1.0f;
            
            if(g.player.last_x_velocity < 0.0)
            {
                float temp_tx = tx2;
                tx2 = tx1;
                tx1 = temp_tx;
            }
            
            glBegin(GL_QUADS);
                glTexCoord2f(tx1, ty2); glVertex2f(-g.player.w, -g.player.h);
                glTexCoord2f(tx1, ty1); glVertex2f(-g.player.w, g.player.h);
                glTexCoord2f(tx2, ty1); glVertex2f( g.player.w, g.player.h);
                glTexCoord2f(tx2, ty2); glVertex2f( g.player.w, -g.player.h);
            glEnd();

        }

        if (abs(g.player.vel[1]) > 0.0f || (g.player.vel[1] == 0 && !g.player.isGrounded))
        {
            glBindTexture(GL_TEXTURE_2D, g.playerid_jump);
        
            //make texture coordinates based on frame number.
            float tx1 = 0.405f;//(float)((g.frameno-1) % 10) * 0.12f;
            float tx2 = tx1 + 0.1f;
            float ty1 = 0.0f;// + (float)((g.frameno-1) / 1) * 1.0f;
            float ty2 = 1.0f;//ty1 + 1.0f;
            if (g.player.vel[0] < 0.0 || g.player.last_x_velocity < 0.0)
            {
                float temp_tx = tx2;
                tx2 = tx1;
                tx1 = temp_tx;
            }
            glBegin(GL_QUADS);
                glTexCoord2f(tx1, ty2); glVertex2f(-g.player.w+12, -g.player.h);
                glTexCoord2f(tx1, ty1); glVertex2f(-g.player.w+12,  g.player.h);
                glTexCoord2f(tx2, ty1); glVertex2f( g.player.w-12,  g.player.h);
                glTexCoord2f(tx2, ty2); glVertex2f( g.player.w-12, -g.player.h);
            glEnd();
            
        }
        
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_ALPHA_TEST);
        glPopMatrix();    
        
    }
}






