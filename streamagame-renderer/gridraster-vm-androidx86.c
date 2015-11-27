#include "SDL.h"
#include "SDL_syswm.h"
#include "SDL_main.h"

#ifdef __linux__
#include <X11/Xlib.h>
#endif

#include <streamagame-renderer/socket.h>

#ifdef WIN32
  #include <windows.h>
  #include <tchar.h>
#else

  #include <pthread.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <android/utils/panic.h>
#include <android/utils/path.h>
#include <android/utils/bufprint.h>
#include <android/android.h>
#include <streamagame-renderer/opengles_gridraster.h>

typedef int bool;
#define true 1
#define false 0

#define TCP_PORT_INPUT 22469
#define TCP_PORT_SENSOR 22471 // VM side port
#define TCP_PORT_LISTEN_TO_ADDR 22471 // host side port

/* Strings describing the host system's OpenGL implementation */
char gl_vendor[ANDROID_GLSTRING_BUF_SIZE];
char gl_renderer[ANDROID_GLSTRING_BUF_SIZE];
char gl_version[ANDROID_GLSTRING_BUF_SIZE];


/* Required by android/utils/debug.h */
extern int android_verbose;


#define USAGE " width height\n"


// Main thread needs to wait on the sdl thread to make the SL thred handle global
#ifdef WIN32
  HANDLE hThread_sdl;

#else
  pthread_t tid_sdl;

#endif

static int g_width, g_height;
static SDL_Surface *g_window_surface = NULL;
static void *g_window_id = NULL;
static float g_rot = 0.0;

static float g_ratio = 1.0;

#define USER_EVENT_ROTATION 1
#define USER_EVENT_NEWCLIENT 2

// extern void multitouch_init();

static SDL_Surface *openSDLWindow(int width, int height, int density)
{
  SDL_Surface *screen;

  if (SDL_Init(SDL_INIT_VIDEO))
    {
      fprintf(stderr, "Can't init SDL system: %s\n", SDL_GetError());
      exit(1);
    }
  char *title = malloc(512 * sizeof(*title));
  if (!title)
    {
      fprintf(stderr, "Out of memory\n");
      exit(1);
  }
  memset(title, 0, 512 * sizeof(*title));

  snprintf(title, 512 * sizeof(*title), "AndroVM Player (%i * %i, %i DPI)", width, height, density);
  SDL_WM_SetCaption(title, 0);

  screen = SDL_SetVideoMode((int)(width*g_ratio), (int)(height*g_ratio), 16, SDL_HWSURFACE);

  if (!screen)
    {
      fprintf(stderr, "Can't create window\n");
      exit(1);
    }
  atexit(SDL_Quit);
  return (screen);
}

void join_sdl_thread(){

#ifdef WIN32
    WaitForSingleObject(hThread_sdl, INFINITE);
#else
    pthread_join(tid_sdl, NULL);
#endif

}

static void *getSDLWindowId(void)
{
  SDL_SysWMinfo wminfo;
  void *winhandle;

  memset(&wminfo, 0, sizeof(wminfo));
  SDL_GetWMInfo(&wminfo);
#ifdef WIN32
  winhandle = (void *)wminfo.window;
#elif defined(__APPLE__)
  winhandle = (void *)wminfo.nsWindowPtr;
#else
  winhandle = (void *)wminfo.info.x11.window;
#endif
  return (winhandle);
}

static void convert_mouse_pos(int *p_x, int *p_y) {
	int tmp;

	*p_x = *p_x / g_ratio;
	*p_y = *p_y / g_ratio;
	switch ((int)g_rot) {
		case 90:
			 tmp = *p_x;
			 *p_x = g_width - *p_y;
			 *p_y = tmp;
			 break;

		case 180:
			 *p_x = g_width - *p_x;
			 break;

		case 270:
			 tmp = *p_x;
			 *p_x = *p_y;
			 *p_y = g_height - tmp;
			 break;
	}
}

SOCKET openControlConnection()
{
	printf("Opening control connection \n");

	SOCKET ssocket = open_server("22469");

	if (ssocket == INVALID_SOCKET)
	{
		printf("Unable to open socket. Aborting ...\n");
		exit(1);
	}

	SOCKET s;
	do {
		s = connect_client(ssocket);

		if (s == INVALID_SOCKET) {
			printf(".");
			sleep(1);
		}
	} while (s == INVALID_SOCKET);

	closesocket(ssocket);

	printf("\nConnection established. (Socket: %d)\n", s);

	return s;
}

void closeControlConnection(SOCKET socket)
{
	if (!socket)
		return;

	printf("Closing control connection ... ");
	closesocket(socket);
	printf("done. \n");
}

#ifdef WIN32
static void input_thread()
#else
static void *input_thread(void *arg)
#endif
{
	// Establish control connection
	SOCKET controlSocket = openControlConnection();

	if (controlSocket != INVALID_SOCKET && controlSocket != 0)
	{
		// Tell the event_loop (thread) about the socket
		SDL_Event connectedEvent;
		connectedEvent.type = SDL_USEREVENT;
		connectedEvent.user.code = USER_EVENT_NEWCLIENT;
		connectedEvent.user.data1 = (void*)controlSocket;
		int ret;
		do {
			printf("Configuring the transmission of events to the remote server... \n");
			ret = SDL_PushEvent(&connectedEvent);
			sleep(1);
		} while (ret != 0);
	}

	return NULL;
}

void eventLoop(int w, int h) {
	SDL_Event event;
	char mbuff[256];
	int nx, ny;
	SOCKET client_sock = 0;

	// This is triggered if the control conntection needs to reconnect.
	bool reconnect = true;

	while(true) {

		if(reconnect)
		{
			closeControlConnection(client_sock);
			client_sock = 0;
			reconnect = false;

#ifdef WIN32
			hThread_sdl = CreateThread(NULL, 0, input_thread, 0, 0, NULL);
			if (hThread_sdl == NULL) {
				fprintf(stderr, "Unable to start input thread, exiting...\n");
				exit(1);
			}
#else
			if (pthread_create(&tid_sdl, NULL, input_thread, NULL) != 0) {
				fprintf(stderr, "Unable to start input thread, exiting...\n");
				exit(1);
			}
#endif
		}

		while (!reconnect && SDL_WaitEvent(&event)) {
		    int ret;

			switch (event.type) {
				case SDL_USEREVENT:
					if (event.user.code == USER_EVENT_NEWCLIENT) {
				    	printf("Got NEWCLIENT uservent with socket=%d\n", (int)event.user.data1);
						if (client_sock)
						{
							printf("Closing old client socket connection...\n");
							closeControlConnection(client_sock);
						}
						client_sock = (SOCKET)event.user.data1;
						sprintf(mbuff, "CONFIG:%d:%d\n", w, h);
						send(client_sock, mbuff, strlen(mbuff), 0);
					}
					break;

				case SDL_QUIT:
					printf("Got SDL_QUIT\n");
					// stop_vm();
					exit(0);
					break;

				case SDL_MOUSEMOTION:
					// printf("Got SDL_MOUSEMOTION with x=%d,y=%d,state=%d\n", event.motion.x, event.motion.y,event.motion.state);
					nx = event.motion.x;
					ny = event.motion.y;
					convert_mouse_pos(&nx, &ny);
					if (client_sock) {
						sprintf(mbuff, "MOUSE:%d:%d\n", nx, ny);
						ret = send(client_sock, mbuff, strlen(mbuff), 0);
						if (ret == SOCKET_ERROR) {
							fprintf(stderr, "Error sending MOUSE command\n");

							// The connection is apparently broken.
							reconnect = true;
							break;
						}
					}
					break;

				case SDL_MOUSEBUTTONDOWN:
				case SDL_MOUSEBUTTONUP:
					printf("Got SDL_MOUSEBUTTON with state=%d button=%d x=%d y=%d\n", event.button.state, event.button.button, event.button.x, event.button.y);
					nx = event.motion.x;
					ny = event.motion.y;
					convert_mouse_pos(&nx, &ny);
					if (client_sock) {
						switch (event.button.button)
						{
							case SDL_BUTTON_LEFT:
							    if (event.button.state == SDL_PRESSED)
							    {
									sprintf(mbuff, "MSBPR:%d:%d\n", nx, ny);
							    }
							    else {
									sprintf(mbuff, "MSBRL:%d:%d\n", nx, ny);
							    }
							    break;

								default:
								    *mbuff = 0x00;
								    break;
						}
						if (*mbuff) {
						    ret = send(client_sock, mbuff, strlen(mbuff), 0);
						    if (ret == SOCKET_ERROR) {
							fprintf(stderr, "Error sending MOUSEBUTTON command\n");

							// The connection is apparently broken.
							reconnect = true;
							break;
						}
					}
					break;
				}
			}
		}
	}
}

#ifdef WIN32
static void winexit(void)
{
  WSACleanup();
}
#endif

int setupStreamagameRenderer(int _width, int _height)
{
  int width = _width;
  int height = _height;
  int dpi = 320;
  void *window_id = 0;

  /* Define ANDROID_EMULATOR_DEBUG to 1 in your environment if you want to
   * see the debug messages from this launcher program.
   */
  const char* debug = getenv("ANDROID_EMULATOR_DEBUG");
  if (debug != NULL && *debug && *debug != '0')
      android_verbose = 1;

#ifdef WIN32
  WSADATA wsaData;

  // Init windows sockets
  if (WSAStartup(MAKEWORD(2,0), &wsaData)) {
    fprintf(stderr, "Unable to initialize Winsock");
    return(1);
  }

  atexit(&winexit);
#endif

  g_width = width;
  g_height = height;

  
  printf("width %d height %d\n", g_width, g_height);

#ifdef __linux__
    // some OpenGL implementations may call X functions
    // it is safer to synchronize all X calls made by all the
    // rendering threads. (although the calls we do are locked
    // in the FrameBuffer singleton object).
    // Disabled XInitThreads since with this, we are getting a crash.
    // Weird part is that we don't get the crash in standalone renderer.
    //XInitThreads();
#endif

  g_window_surface = openSDLWindow(width, height, dpi);

  g_window_id = window_id = getSDLWindowId();
  printf("Window ID: %p\n",window_id);

    {
        if (android_initOpenglesEmulation() == 0){
           printf("android_initOpenglesEmulation  successfull.\n");
	   if(android_startOpenglesRenderer(width, height) == 0){
               printf("android_startOpenglesRenderer  successfull.\n");
            	android_getOpenglesHardwareStrings(
                    gl_vendor, sizeof(android_gl_vendor),
                    gl_renderer, sizeof(android_gl_renderer),
                    gl_version, sizeof(android_gl_version));
            	printf("initialized OpenglES emulation successfully.\n");
            	printf("OpenGL vendor: %s\n", gl_vendor);
            	printf("OpenGL renderer: %s\n", gl_renderer);
            	printf("OpenGL version: %s\n", gl_version);
	   } else {

            	printf("android_startOpenglesRenderer  failure.\n");
            	exit(1);
           }
        }
	else {
            printf("android_initOpenglesEmulation  failure.\n");
            exit(1);
        }
    }

  if (android_showOpenglesWindow(window_id, 0, 0, (int)(width*g_ratio), (int)(height*g_ratio), 0.0) !=0 ) {
    fprintf(stderr, "Unable to setup SubWindow\n");
    exit(1);
  }

  printf("[OpenGL init OK]\n");



  return (0);
}

void standalone_gridraster_vm_thread(int width, int height)
{

	g_width = width;
	g_height = height;

	setupStreamagameRenderer(g_width, g_height);

    eventLoop(g_width, g_height);
}
