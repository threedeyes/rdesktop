#include <Application.h>
#include <Window.h>
#include <View.h>
#include <Rect.h>
#include <Bitmap.h>
#include <Clipboard.h>
#include <Screen.h>
#include <String.h>

extern "C"
{

#include "../rdesktop.h"
#include "window_haiku.h"

#include "vga.h"
#include "vgakeyboard.h"
#include "vgamouse.h"
#include "vgagl.h"

extern void process_mouse(int);
extern int g_sock;
extern int deactivated;
extern uint32 ext_disc_reason;
extern RD_BOOL rdp_loop(RD_BOOL * deactivated, uint32 * ext_disc_reason);
extern char *g_hostname;
extern char *g_username;
extern int g_width;// = 1024;
extern int g_height;// = 768;
extern int g_server_depth;// = 16;
extern int g_encryption;// = 1;

extern int g_xpos;
extern int g_ypos;
extern int g_pos;

extern RD_BOOL g_sendmotion;
extern RD_BOOL g_fullscreen;
extern RD_BOOL g_grab_keyboard;
extern RD_BOOL g_hide_decorations;
extern RD_BOOL g_pending_resize;

extern char g_title[];
extern int g_server_bpp;
extern int g_win_button_size;


static sem_id block_sem = -1;

/* SeamlessRDP support */
typedef struct _seamless_group
{
	BWindow *wnd;
	unsigned long id;
	unsigned int refcnt;
} seamless_group;

typedef struct _seamless_window
{
//	Window wnd;
	BWindow *wnd;
	unsigned long id;
	unsigned long behind;
	seamless_group *group;
	int xoffset, yoffset;
	int width, height;
	int state;		/* normal/minimized/maximized. */
	unsigned int desktop;
	struct timeval *position_timer;

	RD_BOOL outstanding_position;
	unsigned int outpos_serial;
	int outpos_xoffset, outpos_yoffset;
	int outpos_width, outpos_height;

	struct _seamless_window *next;
} seamless_window;

static seamless_window *g_seamless_windows = NULL;
static unsigned long g_seamless_focused = 0;
static RD_BOOL g_seamless_started = False;	/* Server end is up and running */
static RD_BOOL g_seamless_active = False;	/* We are currently in seamless mode */
static RD_BOOL g_seamless_hidden = False;	/* Desktop is hidden on server */
extern RD_BOOL g_seamless_rdp;

//#include "info.h"
////////////////////////////////////////////////////////////////////////////////
int		WW=0;
int		HH=0;
int	CurCol=0;
int		MouseX=0;
int		MouseY=0;
int		oldMouseX=0;
int		oldMouseY=0;
int		MouseB=0;
int MouseEvent=0;
int KeybEvent=0;
thread_id MainThreadID;
thread_id BAppThreadID;
int		NeedUpdate=1;
unsigned char   *BeBuffer=NULL;
BView	*BeView=NULL;
TWin	*BeWindow=NULL;
BBitmap *BeBitmap=NULL;
bool	BEWIN = true;



struct col{
unsigned short r,g,b;
};

col palette[256];

void (*__svgalib_keyboard_eventhandler) (int, int);

static const uint8 be_to_scan_codes[] = {
SCANCODE_ESCAPE,   SCANCODE_F1, SCANCODE_F2, SCANCODE_F3, SCANCODE_F4, SCANCODE_F5,
SCANCODE_F6, SCANCODE_F7, SCANCODE_F8, SCANCODE_F9, SCANCODE_F10, SCANCODE_F11, SCANCODE_F12,SCANCODE_PRINTSCREEN,SCANCODE_SCROLLLOCK,0,
//////////
SCANCODE_GRAVE,SCANCODE_1,SCANCODE_2,SCANCODE_3,SCANCODE_4,SCANCODE_5,SCANCODE_6,SCANCODE_7,SCANCODE_8,
SCANCODE_9,SCANCODE_0,SCANCODE_MINUS,SCANCODE_EQUAL,SCANCODE_BACKSPACE,SCANCODE_INSERT,
SCANCODE_HOME,SCANCODE_PAGEUP,SCANCODE_NUMLOCK,SCANCODE_KEYPADDIVIDE,SCANCODE_KEYPADMULTIPLY,SCANCODE_KEYPADMINUS,
//////////
SCANCODE_TAB,SCANCODE_Q,SCANCODE_W,SCANCODE_E,SCANCODE_R,SCANCODE_T,SCANCODE_Y,SCANCODE_U,SCANCODE_I,
SCANCODE_O,SCANCODE_P,SCANCODE_BRACKET_LEFT,SCANCODE_BRACKET_RIGHT,SCANCODE_BACKSLASH,SCANCODE_REMOVE,SCANCODE_END,SCANCODE_PAGEDOWN,
SCANCODE_KEYPAD7,SCANCODE_KEYPAD8,SCANCODE_KEYPAD9,SCANCODE_KEYPADPLUS,
//////////
SCANCODE_CAPSLOCK,SCANCODE_A,SCANCODE_S,SCANCODE_D,SCANCODE_F,SCANCODE_G,SCANCODE_H,SCANCODE_J,SCANCODE_K,SCANCODE_L,SCANCODE_SEMICOLON,SCANCODE_APOSTROPHE,SCANCODE_ENTER,
SCANCODE_KEYPAD4,SCANCODE_KEYPAD5,SCANCODE_KEYPAD6,
//////////
SCANCODE_LEFTSHIFT,SCANCODE_Z,SCANCODE_X,SCANCODE_C,SCANCODE_V,SCANCODE_B,SCANCODE_N,SCANCODE_M,SCANCODE_COMMA,SCANCODE_PERIOD,SCANCODE_SLASH,
SCANCODE_RIGHTSHIFT,SCANCODE_CURSORBLOCKUP,SCANCODE_KEYPAD1,SCANCODE_KEYPAD2,SCANCODE_KEYPAD3,SCANCODE_KEYPADENTER,
//////////
SCANCODE_LEFTCONTROL,SCANCODE_LEFTALT,SCANCODE_SPACE,SCANCODE_RIGHTALT,SCANCODE_RIGHTCONTROL,SCANCODE_CURSORBLOCKLEFT,SCANCODE_CURSORBLOCKDOWN,SCANCODE_CURSORBLOCKRIGHT,SCANCODE_KEYPAD0,SCANCODE_KEYPADPERIOD,
SCANCODE_LEFTWIN,SCANCODE_RIGHTWIN,127,0
};

////////////////////////////////////////////////////////////////////////////////

static seamless_window *
sw_get_window_by_id(unsigned long id)
{
	seamless_window *sw;
	for (sw = g_seamless_windows; sw; sw = sw->next)
	{
		if (sw->id == id)
			return sw;
	}
	return NULL;
}


static seamless_window *
sw_get_window_by_wnd(BWindow *wnd)
{
	seamless_window *sw;
	for (sw = g_seamless_windows; sw; sw = sw->next)
	{
		if (sw->wnd == wnd)
			return sw;
	}
	return NULL;
}

static void
sw_remove_window(seamless_window * win)
{
	seamless_window *sw, **prevnext = &g_seamless_windows;
	for (sw = g_seamless_windows; sw; sw = sw->next)
	{
		if (sw == win)
		{
			*prevnext = sw->next;
			sw->group->refcnt--;
			if (sw->group->refcnt == 0)
			{
				if(sw->group->wnd!=NULL)
					{
						sw->group->wnd->Hide();
						sw->group->wnd->PostMessage('QUIT');
					}
				xfree(sw->group);
			}
			xfree(sw->position_timer);
			xfree(sw);
			return;
		}
		prevnext = &sw->next;
	}
	return;
}


/* Move all windows except wnd to new desktop */
static void
sw_all_to_desktop(BWindow *wnd, unsigned int desktop)
{
	seamless_window *sw;
	for (sw = g_seamless_windows; sw; sw = sw->next)
	{
		if (sw->wnd == wnd)
			continue;
		if (sw->desktop != desktop)
		{
//			ewmh_move_to_desktop(sw->wnd, desktop);
//			sw->desktop = desktop;
		}
	}
}

/* Send our position */
static void
sw_update_position(seamless_window * sw)
{
	int x, y, w, h;
	unsigned int serial;
	
	x=(int)sw->wnd->Frame().left;
	y=(int)sw->wnd->Frame().top;
	w=(int)sw->wnd->Bounds().Width();
	h=(int)sw->wnd->Bounds().Height();
	
	serial = seamless_send_position(sw->id, x, y, w, h, 0);

	sw->outstanding_position = True;
	sw->outpos_serial = serial;

	sw->outpos_xoffset = x;
	sw->outpos_yoffset = y;
	sw->outpos_width = w;
	sw->outpos_height = h;
}

/* Check if it's time to send our position */
static void
sw_check_timers()
{
	seamless_window *sw;
	struct timeval now;

	gettimeofday(&now, NULL);
	for (sw = g_seamless_windows; sw; sw = sw->next)
	{
		if (timerisset(sw->position_timer) && timercmp(sw->position_timer, &now, <))
		{
			timerclear(sw->position_timer);
			sw_update_position(sw);
		}
	}
}

static void
sw_restack_window(seamless_window * sw, unsigned long behind)
{
	seamless_window *sw_above;

	/* Remove window from stack */
	for (sw_above = g_seamless_windows; sw_above; sw_above = sw_above->next)
	{
		if (sw_above->behind == sw->id)
			break;
	}

	if (sw_above)
		sw_above->behind = sw->behind;

	/* And then add it at the new position */
	for (sw_above = g_seamless_windows; sw_above; sw_above = sw_above->next)
	{
		if (sw_above->behind == behind)
			break;
	}

	if (sw_above)
		sw_above->behind = sw->id;

	sw->behind = behind;
}


static seamless_group *
sw_find_group(unsigned long id, RD_BOOL dont_create)
{
	seamless_window *sw;
	seamless_group *sg;

	for (sw = g_seamless_windows; sw; sw = sw->next)
	{
		if (sw->group->id == id)
			return sw->group;
	}

	if (dont_create)
		return NULL;

	sg = (seamless_group*)malloc(sizeof(seamless_group));

	sg->wnd = NULL;

	sg->id = id;
	sg->refcnt = 0;

	return sg;
}

/////////////////////////////////////////////////////////////////////////////////


int32 lister(void *data)
{
  IterView *obj=(IterView*)data;
  seamless_window *sw;
  struct timeval now;
  
	for(;;)
	{
		if(NeedUpdate==1)
		 {
		  NeedUpdate=0;

		  if(!g_seamless_rdp)
		  		obj->MyDraw();	
		  else {	
					for (sw = g_seamless_windows; sw; sw = sw->next)
					{
						//gettimeofday(&now, NULL);		  	  		  		  	  
						//if (!timerisset(sw->position_timer))
				 		SeamWin *win = (SeamWin*)sw->wnd;
						if(win->Frame().left==sw->xoffset && win->Frame().top==sw->yoffset)
						{
							win->view->MyDraw(win->Bounds());
						}
					}					
		  }
		  
		 }
	  if (g_seamless_active)
			sw_check_timers();		  
		snooze(1000000/18);
	}
}





SeamView::SeamView(BRect R) : BView(R, "seamview", B_FOLLOW_ALL, B_WILL_DRAW)
{
	SetViewColor(B_TRANSPARENT_COLOR);
	SetDrawingMode(B_OP_COPY); 	
}

SeamView::~SeamView()
{
	
}


void SeamView::MouseMoved(BPoint p, uint32 transit,const BMessage *message)
{
	int _MouseX=(int)(p.x+Window()->Frame().left);
	int _MouseY=(int)(p.y+Window()->Frame().top);
		
	MouseX=_MouseX;
	MouseY=_MouseY;
	MouseEvent=1;
	process_mouse(0);
}

void SeamView::MouseDown(BPoint p)
{
	uint32 buttons = Window()->CurrentMessage()->FindInt32("buttons");
	int butt=0;
	
	if (buttons & B_PRIMARY_MOUSE_BUTTON)
			butt |= 4;
	if (buttons & B_SECONDARY_MOUSE_BUTTON)
			butt |= 1;
	if (buttons & B_TERTIARY_MOUSE_BUTTON)
			butt |= 2;

			
	MouseB=butt;	
	process_mouse(0);

//	Window()->Activate(true);
	SetMouseEventMask(B_POINTER_EVENTS,0);
}

void SeamView::MouseUp(BPoint p)
{
	MouseB=0;
	process_mouse(0);

}

void
SeamView::Draw(BRect r)
{
 MyDraw(r);
}

void
SeamView::MyDraw(BRect r)
{
  bool s=LockLooper();
  if(s==true)
	{
	 BRect from=r;	 
	 from.OffsetBy(Window()->Frame().left,Window()->Frame().top);
 	 DrawBitmap(BeBitmap,from,r);
	 UnlockLooper();
	}	

}

SeamWin::SeamWin(int x,int y, int w,int h,window_look look,int _id,int _group)
	: BWindow(BRect(x, y, x+w-1, y+h-1), g_title, look,B_NORMAL_WINDOW_FEEL, B_WILL_ACCEPT_FIRST_CLICK|B_OUTLINE_RESIZE)
{
	id=_id;
	group=_group;
	
	view = new SeamView(Bounds());
	
	AddChild(view);
	be_clipboard->StartWatching(this);	
}

SeamWin::~SeamWin()
{
//	RemoveChild(view);
//	delete view;
 	be_clipboard->StopWatching(this);
}


void SeamWin::MessageReceived(BMessage *message)
{
	switch(message->what) {
		case 'QUIT':
		{
			Quit();
			break;
		}	
		case B_UNMAPPED_KEY_DOWN:
		case B_KEY_DOWN:
		{
			const char *bytes;
			int32 key;
			message->FindInt32("key",&key);
			if (key < sizeof(be_to_scan_codes))
			{
			 // printf("B_KEY_DOWN %d\n",key);
			  key = be_to_scan_codes[(uint8)key-1];
			  __svgalib_keyboard_eventhandler(key, KEY_EVENTPRESS);
			}
			break;
		}
		case B_UNMAPPED_KEY_UP:
		case B_KEY_UP:
		{
			const char *bytes;
			int32 key;
			message->FindInt32("key",&key);
			if (key < sizeof(be_to_scan_codes))
			{
			  key = be_to_scan_codes[(uint8)key-1];			
		 	  __svgalib_keyboard_eventhandler(key, KEY_EVENTRELEASE);
		 	}
			 break;
		} 
		case B_MOUSE_WHEEL_CHANGED:
			{
			 float shift=0;
			 if(message->FindFloat("be:wheel_delta_y",&shift)==B_OK)
			 {
		        if(shift<0)
		        {
		         rdp_send_input(0, RDP_INPUT_MOUSE, MOUSE_FLAG_DOWN | MOUSE_FLAG_BUTTON4, MouseX,MouseY);
		        }
				if(shift>0)
				{
		         rdp_send_input(0, RDP_INPUT_MOUSE, MOUSE_FLAG_DOWN | MOUSE_FLAG_BUTTON5, MouseX,MouseY);
		        }
			 }			 
			 break;
			}		
		case B_CLIPBOARD_CHANGED:
			{
			 ui_clip_request_data(CF_TEXT);
			 break;
			}
		case 'SCCH':
			{
			 snooze(200000);
			 cliprdr_send_data_request(CF_TEXT);
			 break;
			}
		case B_WINDOW_ACTIVATED:
		case B_WORKSPACE_ACTIVATED:
			{
			 NeedUpdate = 1;
//			 release_sem(loop_sem); 		  
			 break;
			}
		default:
			BWindow::MessageReceived(message);
			break;		
	}
}

void 
SeamWin::WindowActivated(bool f)
{
	printf("WindowActivated - %d,%d\n",id,f);
	seamless_window *sw;
	sw = sw_get_window_by_id(id);
	if (!sw)return;

/*	if(f==1)
	{	
		seamless_send_state(sw->id, SEAMLESSRDP_NORMAL, 0);		
		if(Look()==B_TITLED_WINDOW_LOOK)
		{
			MouseX=sw->xoffset+sw->width/2;
			MouseY=sw->yoffset+10;
			MouseB=4;			
			process_mouse(0);	
			MouseB=0;			
			process_mouse(0);	
		}
	}
	else
	{
		//seamless_send_state(sw->id, SEAMLESSRDP_NORMAL, 0);			
	}*/
}

void 
SeamWin::FrameResized(float width, float height)
{
	//seamless_send_position(id, Frame().left, Frame().top, width, height, 0);
	seamless_window *sw;
	
	if (!g_seamless_active)
		return;

	sw = sw_get_window_by_id(id);
	if (!sw)
		return;

	gettimeofday(sw->position_timer, NULL);
	if (sw->position_timer->tv_usec + SEAMLESSRDP_POSITION_TIMER >= 1000000) {
		sw->position_timer->tv_usec += SEAMLESSRDP_POSITION_TIMER - 1000000;
		sw->position_timer->tv_sec += 1;
	} else {
		sw->position_timer->tv_usec += SEAMLESSRDP_POSITION_TIMER;
	}	
}

void
SeamWin::FrameMoved(BPoint p)
{
	seamless_window *sw;
	
	if (!g_seamless_active)
		return;

	sw = sw_get_window_by_id(id);
	if (!sw)
		return;

	gettimeofday(sw->position_timer, NULL);
	if (sw->position_timer->tv_usec + SEAMLESSRDP_POSITION_TIMER >= 1000000) {
		sw->position_timer->tv_usec += SEAMLESSRDP_POSITION_TIMER - 1000000;
		sw->position_timer->tv_sec += 1;
	} else {
		sw->position_timer->tv_usec += SEAMLESSRDP_POSITION_TIMER;
	}

	//sw_handle_restack(sw);
}




bool
SeamWin::QuitRequested()
{    
	return true;
}

TWin::TWin(int x,int y, int w,int h,window_look look)
	: BWindow(BRect(x, y, x+w-1, y+h-1), g_title, look,B_NORMAL_WINDOW_FEEL, 0)//B_NOT_RESIZABLE|B_NOT_ZOOMABLE
{
	WW=w;
	HH=h;
	view = new IterView(BRect(0,0,w-1,h-1));
	AddChild(view);
	view->SetViewColor(0,0,0);
	be_clipboard->StartWatching(this);
	block_sem = create_sem(1, "blocker");
}

TWin::~TWin()
{
 	be_clipboard->StopWatching(this);
}

void TWin::MessageReceived(BMessage *message)
{
	switch(message->what) {
		case B_UNMAPPED_KEY_DOWN:
		case B_KEY_DOWN:
		{
			const char *bytes;
			int32 key;
			message->FindInt32("key",&key);
			if (key < sizeof(be_to_scan_codes))
			{
			 // printf("B_KEY_DOWN %d\n",key);
			  key = be_to_scan_codes[(uint8)key-1];
			  __svgalib_keyboard_eventhandler(key, KEY_EVENTPRESS);
			}
			break;
		}
		case B_UNMAPPED_KEY_UP:
		case B_KEY_UP:
		{
			const char *bytes;
			int32 key;
			message->FindInt32("key",&key);
			if (key < sizeof(be_to_scan_codes))
			{
			  key = be_to_scan_codes[(uint8)key-1];			
		 	  __svgalib_keyboard_eventhandler(key, KEY_EVENTRELEASE);
		 	}
			 break;
		} 
		case B_MOUSE_WHEEL_CHANGED:
			{
			 float shift=0;
			 if(message->FindFloat("be:wheel_delta_y",&shift)==B_OK)
			 {
		        if(shift<0)
		        {
		         rdp_send_input(0, RDP_INPUT_MOUSE, MOUSE_FLAG_DOWN | MOUSE_FLAG_BUTTON4, MouseX,MouseY);
		        }
				if(shift>0)
				{
		         rdp_send_input(0, RDP_INPUT_MOUSE, MOUSE_FLAG_DOWN | MOUSE_FLAG_BUTTON5, MouseX,MouseY);
		        }
			 }			 
			 break;
			}		
		case B_CLIPBOARD_CHANGED:
			{
			 ui_clip_request_data(CF_TEXT);
			 break;
			}
		case 'SCCH':
			{
			 snooze(200000);
			 cliprdr_send_data_request(CF_TEXT);
			 break;
			}
		case B_WINDOW_ACTIVATED:
		case B_WORKSPACE_ACTIVATED:
			{
			 NeedUpdate=1;
			 break;
			}
		default:
			BWindow::MessageReceived(message);
			break;		
	}
}

void 
TWin::FrameResized(float width, float height)
{
	NeedUpdate=1;
//	g_pending_resize = True;
//	snooze(10000);
//	g_height = height;
//	g_width = width;
}



bool
TWin::QuitRequested()
{    
   	kill_thread(find_thread("rdpgui"));
	be_app->PostMessage(B_QUIT_REQUESTED);
	exit(0);
	return true;
}

IterView::IterView(BRect R) : BView(R, "iterview", B_FOLLOW_ALL, B_WILL_DRAW)
{
	SetViewColor(255,255,255);
	bufferView=new BView(R,"bufferview",B_FOLLOW_ALL_SIDES,B_WILL_DRAW);
    buffer=new BBitmap(R,B_RGB16,true);

	buffer->AddChild(bufferView);
	
	ScrBuff=(unsigned char*)buffer->Bits();
	BeBuffer=ScrBuff;
	BeView=bufferView;
	BeBitmap = buffer;
		
 	my_thread = spawn_thread(lister,"rdpgui",1,(void*)this);
	resume_thread(my_thread);
	
	SetDrawingMode(B_OP_COPY); 	
}

IterView::~IterView()
{
	
}


void IterView::MouseMoved(BPoint p, uint32 transit,const BMessage *message)
{
	MouseX=(int)p.x * buffer->Bounds().Width() / Bounds().Width();
	MouseY=(int)p.y * buffer->Bounds().Height() / Bounds().Height();
	
	//if(transit==B_ENTERED_VIEW)be_app->HideCursor();
	//if((transit==B_EXITED_VIEW || transit==B_OUTSIDE_VIEW))be_app->ShowCursor();
	process_mouse(0);
	MouseEvent=1;
}

void IterView::MouseDown(BPoint p)
{
	uint32 buttons = Window()->CurrentMessage()->FindInt32("buttons");
	int butt=0;
	
	if (buttons & B_PRIMARY_MOUSE_BUTTON)
			butt |= 4;
	if (buttons & B_SECONDARY_MOUSE_BUTTON)
			butt |= 1;
	if (buttons & B_TERTIARY_MOUSE_BUTTON)
			butt |= 2;
			
	MouseB=butt;
	
	process_mouse(0);

}

void IterView::MouseUp(BPoint p)
{
	uint32 buttons = Window()->CurrentMessage()->FindInt32("buttons");
	int butt=0;

	if ((buttons ^ MouseB) & B_PRIMARY_MOUSE_BUTTON)
			butt |= 4;
		if ((buttons ^ MouseB) & B_SECONDARY_MOUSE_BUTTON)
			butt |= 1;
		if ((buttons ^ MouseB) & B_TERTIARY_MOUSE_BUTTON)
			butt |= 2;
	MouseB=0;
	process_mouse(0);
}

void
IterView::Draw(BRect r)
{
 MyDraw();
}

void
IterView::MyDraw(void)
{
  bool s=LockLooper();
  if(s==true)
	{
 	 bufferView->LockLooper();
 	 DrawBitmap(buffer, buffer->Bounds(), Bounds(), B_FILTER_BITMAP_BILINEAR);
	 bufferView->UnlockLooper();
	 UnlockLooper();
	}	

}


///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////


void vga_resize_window(int w,int h)
{
	//BeWindow->ResizeTo(w,h);
}

int beapp_init(void)
 {
  BApplication *app;
  if(be_app==NULL)app=new BApplication("application/x-vnd.Be-RDesktop");
  }

 int vga_init(void)
 {
  xclip_init();
  return 0;
 }
 
 int vga_getpixel(int x, int y)
 {
  // printf("vga_getpixel (%d,%d)\n",x,y);
   return 0;
 }
 
 int vga_setcolor(int color)
 {
  CurCol=color;
  return 0;
 }
 
 int vga_drawpixel(int x, int y)
 {
//  BeBuffer[y*WW+x]=CurCol;
  NeedUpdate=1;
  return 0;
 }
 
 
int vga_accel(unsigned operation,...)
{
    return 0;
}
 
 int vga_drawscansegment(unsigned char *colors, int x, int y, int length)
 {  
    if(x<0 || y<0)return 0;
    if(g_server_bpp==16)memcpy((unsigned char*)BeBuffer+(y*WW*2)+(x*2),colors,length);
    if(g_server_bpp==8)
    {
     unsigned char* p=(unsigned char*)BeBuffer+(y*WW*2)+(x*2);
     unsigned short *s=(unsigned short*)p;
     for(int i=0;i<length;i++)
      {
       col c=palette[ colors[i]];
       s[i]=( (c.b>>1) & 31) | ((c.g&63)<<5) | (((c.r>>1)&31)<<11);
      }
    }
    NeedUpdate=1;
    return 0;
 }

 
 int vga_getscansegment(unsigned char *colors, int x, int y, int length)
 {
    return 0;
 }
 
 int mouse_getx(void)
 {
    return MouseX;
 }

 int mouse_gety(void)
 {
    return MouseY;
 }
 
 int vga_getscreen_width(void)
 {
  BScreen *Screen=new BScreen(B_MAIN_SCREEN_ID);
  int w=(int)(Screen->Frame().Width()+1);
  delete Screen;	   
  return w;
 }

 int vga_getscreen_height(void)
 {
  BScreen *Screen=new BScreen(B_MAIN_SCREEN_ID);
  int w=(int)(Screen->Frame().Height()+1);
  delete Screen;	   
  return w;
 }
 
 int vga_setmode_be(int xdim,int ydim,int colors,int full)
 {
  if(xdim==0 || ydim==0 || colors==0)
   {   	
    NeedUpdate=0;
    snooze(100000);
    kill_thread(find_thread("rdpgui"));
    if(BeWindow != NULL) {
    	BeWindow->Lock();
    	BeWindow->Quit();
    	BeWindow = NULL;
    }
	return 0;
   }
 
  if(BeWindow!=NULL)
   {
    BeWindow->Lock();
    BeWindow->Quit();
   }

   BeWindow=new TWin(100,100,xdim,ydim,B_TITLED_WINDOW_LOOK);
   
   if(full)
    {
	   BScreen *Screen=new BScreen(B_MAIN_SCREEN_ID);
   	   BeWindow->LockLooper();
	   BPoint viewRect=BeWindow->view->ConvertToScreen(BPoint(0,0));	
	   BPoint winRect=BPoint(BeWindow->Frame().left,BeWindow->Frame().top);			   
	   BeWindow->MoveTo(winRect-viewRect);
 	   BeWindow->UnlockLooper(); 
	   delete Screen;	   
    }    	   
  if(!g_seamless_rdp)
   	BeWindow->Show();

  return 0;
 }

 
 int vga_hasmode(int mode)
 {
   return true;
 }
 
 void vga_setmousesupport(int s)
 {
 }
 
 void mouse_setposition(int x, int y)
 {
 }

 int keyboard_init(void)
 {
  return 0;
 }

 void keyboard_close(void)
 {
 }

 void keyboard_seteventhandler(void (*handler) (int, int))
 { 
   __svgalib_keyboard_eventhandler = handler;
 }
 
 void keyboard_translatekeys(int mode)
 {
 }
 
 int vga_ext_set(unsigned what,...)
{
  return 0;
}

 int mouse_getbutton()
 {
   return MouseB;
 }
 
 int vga_waitevent(int which, fd_set * in, fd_set * out, fd_set * except,
		  struct timeval *timeout)
 {
  return 0;
 }
 
 int vga_setpalvec(int start, int num, int *pal)
 {
  int i;

  for (i = start; i < start + num; ++i)
  {
	palette[i].r=pal[0] , palette[i].g=pal[1] , palette[i].b=pal[2] ;
	pal += 3;
  }
  return num;
 }
 

/*****************************************************************************/

/*****************************************************************************/
int ui_select(int in)
{
  g_sock = in; 

  return 1;
}



void
ui_seamless_begin(RD_BOOL hidden)
{
	if (!g_seamless_rdp)
		return;

	if (g_seamless_started)
		return;

	g_seamless_started = True;
	g_seamless_hidden = hidden;

	if (!hidden)
		ui_seamless_toggle();
}


void
ui_seamless_hide_desktop()
{
	if (!g_seamless_rdp)
		return;

	if (!g_seamless_started)
		return;

	if (g_seamless_active)
		ui_seamless_toggle();

	g_seamless_hidden = True;
}


void
ui_seamless_unhide_desktop()
{
	if (!g_seamless_rdp)
		return;

	if (!g_seamless_started)
		return;

	g_seamless_hidden = False;

	ui_seamless_toggle();
}


void
ui_seamless_toggle()
{
	if (!g_seamless_rdp)
		return;

	if (!g_seamless_started)
		return;

	if (g_seamless_hidden)
		return;

	if (g_seamless_active)
	{
		/* Deactivate */
		while (g_seamless_windows)
		{
//			XDestroyWindow(g_display, g_seamless_windows->wnd);
//			sw_remove_window(g_seamless_windows);
		}
//		XMapWindow(g_display, g_wnd);
	}
	else
	{
		/* Activate */
//		XUnmapWindow(g_display, g_wnd);
		seamless_send_sync();
	}

	g_seamless_active = !g_seamless_active;
}


void
ui_seamless_create_window(unsigned long id, unsigned long group, unsigned long parent,
			  unsigned long flags)
{
	SeamWin *wnd=NULL;
	seamless_window *sw, *sw_parent;

	if (!g_seamless_active)
		return;

	printf("ui_seamless_create_window id=%d, grp=%d, parent=%d, flags=0x%lx\n",id, group, parent, flags);

	// Ignore CREATEs for existing windows 
	sw = sw_get_window_by_id(id);
	if (sw)
		return;
		
	wnd = new SeamWin(-100,-100,80,20, B_NO_BORDER_WINDOW_LOOK, id, group);
	
	// Parent-less transient windows 
	if (parent == 0xFFFFFFFF)
	{
		wnd->SetLook(B_NO_BORDER_WINDOW_LOOK);
	}
	// Normal transient windows 
	else if (parent != 0x00000000)
	{
		sw_parent = sw_get_window_by_id(parent);
		if (sw_parent)
				wnd->SetLook(B_NO_BORDER_WINDOW_LOOK);
		else
			warning("ui_seamless_create_window: No parent window 0x%lx\n", parent);
	}

	if (flags & SEAMLESSRDP_CREATE_MODAL)
	{
		wnd->SetLook(B_MODAL_WINDOW_LOOK);
		wnd->SetFeel(B_MODAL_APP_WINDOW_FEEL);
		wnd->SetFlags(B_NOT_RESIZABLE|B_NOT_MINIMIZABLE|B_NOT_ZOOMABLE);
	}
	
	wnd->Show();

	sw = (seamless_window*)malloc(sizeof(seamless_window));
	sw->wnd = wnd;
	sw->id = id;
	sw->behind = 0;
	sw->group = sw_find_group(group, False);
	sw->group->refcnt++;
	sw->xoffset = 0;
	sw->yoffset = 0;
	sw->width = 0;
	sw->height = 0;
	sw->state = SEAMLESSRDP_NOTYETMAPPED;
	sw->desktop = 0;
	sw->position_timer = (timeval*)malloc(sizeof(struct timeval));
	timerclear(sw->position_timer);

	sw->outstanding_position = False;
	sw->outpos_serial = 0;
	sw->outpos_xoffset = sw->outpos_yoffset = 0;
	sw->outpos_width = sw->outpos_height = 0;

	sw->next = g_seamless_windows;
	g_seamless_windows = sw;
}


void
ui_seamless_destroy_window(unsigned long id, unsigned long flags)
{
	seamless_window *sw;

	if (!g_seamless_active)
		return;

	printf("ui_seamless_destroy_window - %d beg\n",id);

	sw = sw_get_window_by_id(id);
	if (!sw)
	{
		warning("ui_seamless_destroy_window: No information for window 0x%lx\n", id);
		return;
	}

	sw->wnd->Hide();
	sw->wnd->PostMessage('QUIT');
	sw_remove_window(sw);
}


void
ui_seamless_destroy_group(unsigned long id, unsigned long flags)
{
	seamless_window *sw, *sw_next;

	if (!g_seamless_active)
		return;

	printf("ui_seamless_destroy_group group=%d, flags=0x%lx\n",id, flags);

	for (sw = g_seamless_windows; sw; sw = sw_next)
	{
		sw_next = sw->next;

		if (sw->group->id == id)
		{
			sw->wnd->Hide();
			sw->wnd->PostMessage('QUIT');
			sw_remove_window(sw);
		}
	}	
}


void
ui_seamless_move_window(unsigned long id, int x, int y, int width, int height, unsigned long flags)
{
	seamless_window *sw;

	if (!g_seamless_active)
		return;

	sw = sw_get_window_by_id(id);
	if (!sw)
	{
		warning("ui_seamless_move_window: No information for window 0x%lx\n", id);
		return;
	}

	/* We ignore server updates until it has handled our request. */
	if (sw->outstanding_position)
		return;

	if (!width || !height)
		/* X11 windows must be at least 1x1 */
		return;

	sw->xoffset = x;
	sw->yoffset = y;
	sw->width = width;
	sw->height = height;

	/* If we move the window in a maximized state, then KDE won't
	   accept restoration */
	switch (sw->state)
	{
		case SEAMLESSRDP_MINIMIZED:
		case SEAMLESSRDP_MAXIMIZED:
			return;
	}
	
	sw->wnd->MoveTo(x,y);
	sw->wnd->ResizeTo(width,height);
}


void
ui_seamless_restack_window(unsigned long id, unsigned long behind, unsigned long flags)
{
	seamless_window *sw;

	if (!g_seamless_active)
		return;

	sw = sw_get_window_by_id(id);
	if (!sw)
	{
		warning("ui_seamless_restack_window: No information for window 0x%lx\n", id);
		return;
	}

	if (behind)
	{
		seamless_window *sw_behind;
		BWindow *wnds[2];

		sw_behind = sw_get_window_by_id(behind);
		if (!sw_behind)
		{
			warning("ui_seamless_restack_window: No information for window 0x%lx\n",
				behind);
			return;
		}

		wnds[1] = sw_behind->wnd;
		wnds[0] = sw->wnd;

//		sw->wnd->Activate(true);
	}
	else
	{
//		sw->wnd->Activate(false);
	}

	sw_restack_window(sw, behind);
}


void
ui_seamless_settitle(unsigned long id, const char *title, unsigned long flags)
{
	seamless_window *sw;

	if (!g_seamless_active)
		return;

	sw = sw_get_window_by_id(id);
	if (!sw)
	{
		warning("ui_seamless_settitle: No information for window 0x%lx\n", id);
		return;
	}
	
	sw->wnd->SetTitle(title);
}


void
ui_seamless_setstate(unsigned long id, unsigned int state, unsigned long flags)
{
	seamless_window *sw;

	if (!g_seamless_active)
		return;

	sw = sw_get_window_by_id(id);
	if (!sw)
	{
		warning("ui_seamless_setstate: No information for window 0x%lx\n", id);
		return;
	}

	switch (state)
	{
		case SEAMLESSRDP_NORMAL:
		case SEAMLESSRDP_MAXIMIZED:
			sw->wnd->Activate(true);
			break;
		case SEAMLESSRDP_MINIMIZED:
			sw->wnd->Minimize(true);
			break;
		default:
			warning("SeamlessRDP: Invalid state %d\n", state);
			break;
	}
	
	sw->state = state;
}


void
ui_seamless_syncbegin(unsigned long flags)
{
	if (!g_seamless_active)
		return;
		
	printf("ui_seamless_syncbegin\n");

	// Destroy all seamless windows 
	while (g_seamless_windows)
	{
		g_seamless_windows->wnd->Hide();
		g_seamless_windows->wnd->PostMessage('QUIT');
		sw_remove_window(g_seamless_windows);
	}
}


void
ui_seamless_ack(unsigned int serial)
{
	seamless_window *sw;
	for (sw = g_seamless_windows; sw; sw = sw->next)
	{
		if (sw->outstanding_position && (sw->outpos_serial == serial))
		{
			sw->xoffset = sw->outpos_xoffset;
			sw->yoffset = sw->outpos_yoffset;
			sw->width = sw->outpos_width;
			sw->height = sw->outpos_height;
			sw->outstanding_position = False;

			//((SeamWin*)(sw->wnd))->view->MyDraw(BRect(0, 0, sw->width, sw->height));
			break;
		}
	}
}
 

void
ui_seamless_seticon(unsigned long id, const char *format, int width, int height, int chunk,
		    const char *data, int chunk_len)
{
}


void
ui_seamless_delicon(unsigned long id, const char *format, int width, int height)
{

}


}
