#include <Application.h>
#include <Window.h>
#include <View.h>
#include <Rect.h>
#include <Bitmap.h>
#include <Clipboard.h>

class IterView : public BView
{
 public:
					IterView(BRect R);
					~IterView();
	void			MyDraw(void);
				
	BBitmap 		*buffer;	
    BView 	 		*bufferView;
    
    unsigned char	*ScrBuff;
    
    thread_id 		my_thread;

protected:
	virtual void	Draw(BRect r);
	virtual void 	MouseDown(BPoint p);
 	virtual void 	MouseUp(BPoint p);
 	virtual void 	MouseMoved(BPoint point, uint32 transit, const BMessage *message);	
};


class TWin : public BWindow
{
 public:
				TWin(int x,int y, int w,int h,window_look look);
				~TWin();
	bool		QuitRequested();
 	virtual void FrameResized(float width, float height);		
	virtual void MessageReceived(BMessage *message);	
	IterView *view;
};


class SeamView : public BView
{
 public:
					SeamView(BRect R);
					~SeamView();
	void			MyDraw(BRect r);				
protected:
	virtual void	Draw(BRect r);
	virtual void 	MouseDown(BPoint p);
 	virtual void 	MouseUp(BPoint p);
 	virtual void 	MouseMoved(BPoint point, uint32 transit, const BMessage *message);	
};

class SeamWin : public BWindow
{
 public:
				SeamWin(int x,int y, int w,int h,window_look look,int id=0,int group=0);
				~SeamWin();
 	virtual void 	FrameResized(float width, float height);
 	virtual void 	FrameMoved(BPoint p);
	virtual void MessageReceived(BMessage *message);	
	virtual	void WindowActivated(bool f);
	bool		QuitRequested();
	SeamView	*view;
	int			id;
	int			group;	
};
