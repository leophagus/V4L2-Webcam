//----------------------------------------------------------------------------
// Simple yuv webcam viewer. V4L2 (yuv) -> SDL. Hit Ctrl-C to quit
// leophagus 2017-02-26 18:12:40  
//
// gcc % -std=gnu99 -lSDL -O3 -o wc
//----------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <linux/videodev2.h>
#include <stdbool.h>

#include <SDL/SDL.h>

/* Webcam image dimensions. */
int pix_w = 640;
int pix_h = 480;
int nBytes; // for SDL_YUY2_OVERLAY

/* webcam device */
const char* p_devName = "/dev/video0";
int g_fd;

/* V4L2 buffers */
void *g_buf;
struct v4l2_buffer g_bufInfo;

/* SDL surface and oerlay */
SDL_Surface *p_screen = NULL;
SDL_Overlay *g_sdlOverlay = NULL;

/* to trap Ctrl-C */
bool volatile goON = true;

bool openDev ()
{
  g_fd = open(p_devName, O_RDWR);
  if (g_fd < 0) {
    perror ("Failed to open device");
    return false;
  }

  struct v4l2_capability cap;
  if (ioctl (g_fd, VIDIOC_QUERYCAP, &cap) < 0) {
    perror ("VIDIOC_QUERYCAP failed");
    return false;
  }

  if (! (cap. capabilities & (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING))) {
    perror ("Caps insufficient");
    return false;
  }

  struct v4l2_format format;
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
  format.fmt.pix.width = pix_w;
  format.fmt.pix.height = pix_h;

  if(ioctl(g_fd, VIDIOC_S_FMT, &format) < 0){
    perror("VIDIOC_S_FMT not supported");
    return false;
  }

  return true;
}

bool setupBufs ()
{
  struct v4l2_requestbuffers reqBufs;
  reqBufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  reqBufs.memory = V4L2_MEMORY_MMAP;
  reqBufs.count = 1;

  if(ioctl(g_fd, VIDIOC_REQBUFS, &reqBufs) < 0){
    perror("VIDIOC_REQBUFS");
    return false;
  }

  memset(&g_bufInfo, 0, sizeof(g_bufInfo));

  g_bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  g_bufInfo.memory = V4L2_MEMORY_MMAP;
  g_bufInfo.index = 0;

  if(ioctl(g_fd, VIDIOC_QUERYBUF, &g_bufInfo) < 0){
    perror("VIDIOC_QUERYBUF");
    return false;
  }

  g_buf = mmap (NULL, g_bufInfo.length, PROT_READ | PROT_WRITE, MAP_SHARED, 
                g_fd, g_bufInfo.m.offset);

  if(g_buf == MAP_FAILED){
    perror("mmap failed");
    return false;
  }
  memset(g_buf, 0, g_bufInfo.length);

  int type = g_bufInfo.type;
  if(ioctl(g_fd, VIDIOC_STREAMON, &type) < 0){
    perror("VIDIOC_STREAMON");
    return false;
  }

  printf ("setupBufs done. Len %d\n", g_bufInfo.length);
  return true;
}

bool grabNext ()
{
  if(ioctl(g_fd, VIDIOC_QBUF, &g_bufInfo) < 0){
    perror("VIDIOC_QBUF");
    return false;
  }

  // The buffer's waiting in the outgoing queue.
  if(ioctl(g_fd, VIDIOC_DQBUF, &g_bufInfo) < 0){
    perror("VIDIOC_QBUF");
    return false;
  }
}

bool streamOff ()
{
  int type = g_bufInfo.type;
  if(ioctl(g_fd, VIDIOC_STREAMOFF, &type) < 0){
    perror("VIDIOC_STREAMOFF");
    return false;
  }

  close (g_fd);
}

bool initSDL ()
{
  if (SDL_Init(SDL_INIT_VIDEO)) {
    perror ("SDL init failed");
    return false;
  }
   
  p_screen = SDL_SetVideoMode (pix_w, pix_h, 0, 0);
  if (!p_screen) {
    perror ("SDL_SetVideoMode failed");
    return false;
  }

  g_sdlOverlay = SDL_CreateYUVOverlay (pix_w, pix_h, SDL_YUY2_OVERLAY, p_screen);
  if (!g_sdlOverlay) {
    perror ("SDL_CreateYUVOverlay failed");
    return false;
  }

  return true;
}

void quitSDL ()
{
  SDL_FreeYUVOverlay (g_sdlOverlay);
  SDL_Quit ();
}

void displaySDL ()
{
  SDL_LockYUVOverlay (g_sdlOverlay);

  // can avoid this if we can mmap the overlay's pixels to v4l
  memcpy (g_sdlOverlay-> pixels [0], g_buf, nBytes); 

  SDL_UnlockYUVOverlay (g_sdlOverlay);

  SDL_Rect rect = {.x = 0, .y = 0, .w = pix_w, .h = pix_h};
  SDL_DisplayYUVOverlay (g_sdlOverlay, &rect);
}

void sigHandler (int foo)
{
  goON = false;
}

int main ()
{
  signal (SIGINT, sigHandler);

  nBytes = pix_w * pix_h * 2;

  if (!openDev ()) {
    return 1;
  }
  if (!setupBufs ()) {
    return 1;
  }
  if (!initSDL ()) {
    return 1;
  }

  while (goON) {
    grabNext ();
    displaySDL ();
    usleep (33000);
  }

  streamOff ();
  quitSDL ();

  return 0;
}
