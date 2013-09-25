#include <Clipboard.h>
#include <Window.h>
#include <Message.h>
#include <UTF8.h>
#include "window_haiku.h"

extern "C"
{
#include "../rdesktop.h"

extern TWin	*BeWindow;

#define NUM_TARGETS 6

#ifdef USE_UNICODE_CLIPBOARD
#define RDP_CF_TEXT CF_UNICODETEXT
#else
#define RDP_CF_TEXT CF_TEXT
#endif

static int have_primary = 0;
static int rdesktop_is_selection_owner = 0;


BClipboard *Clip;


/* Replace CR-LF to LF (well, rather removing all CR:s) This is done
   in-place. The length is updated. Handles embedded nulls */
static void
crlf2lf(uint8 * data, uint32 * length)
{
	uint8 *dst, *src;
	src = dst = data;
	while (src < data + *length)
	{
		if (*src != '\x0d')
			*dst++ = *src;
		src++;
	}
	*length = dst - data;
}

/* Translate LF to CR-LF. To do this, we must allocate more memory.  
   The length is updated. */
static uint8 *
lf2crlf(uint8 * data, uint32 * length)
{
	uint8 *result, *p, *o;

	/* Worst case: Every char is LF */
	result = (uint8*)xmalloc(*length * 2);

	p = data;
	o = result;

	while (p < data + *length)
	{
		if (*p == '\x0a')
			*o++ = '\x0d';
		*o++ = *p++;
	}
	*length = o - result;

	/* Convenience */
	*o++ = '\0';

	return result;
}

void
ui_clip_format_announce(uint8 * data, uint32 length)
{
  	
  if(BeWindow!=NULL)
   {
    BeWindow->PostMessage('SCCH');
   }
}


void
ui_clip_handle_data(uint8 * data, uint32 length)
{
	  const char *text;
	  int32 textLen;
	  BMessage *clip = (BMessage *)NULL;
	  
	  
	  
	  if(be_clipboard->Lock())
	   {
	    be_clipboard->Clear();
	    if(clip=be_clipboard->Data())
	     {
			if(length>0)
			 {
				char *ToUtf8=(char*)xmalloc((length*4)+4);
				if(ToUtf8!=NULL)
				 {
					uint32 len=length;
					crlf2lf(data,&len);
					
					int32 DestLen=len*4;
					int32 State=0;
		     		int32 SourceLen=len;
			  		convert_to_utf8(B_MS_WINDOWS_1251_CONVERSION, (const char*)data, &SourceLen, ToUtf8, &DestLen, &State);
			  		ToUtf8[DestLen]=0;
					
					//printf("ui_clip_handle_data: len=%d str=%s\n",DestLen, ToUtf8);
					clip->AddData("text/plain", B_MIME_TYPE,ToUtf8,DestLen);				
					
	      			xfree(ToUtf8);
			      	be_clipboard->Commit();
   		    	 }
   		     }
	     }
   		be_clipboard->Unlock();
	   }	 
}

void
ui_clip_request_failed()
{
	
}

void
ui_clip_set_mode(const char *optarg)
{
	
}

void
ui_clip_request_data(uint32 format)
{
	
	if(format==1)
	 {
	  const char *text;
	  ssize_t textLen;
	  BMessage *clip = (BMessage *)NULL;
	  if(be_clipboard->Lock())
	   {
	    if(clip = be_clipboard->Data())
	     {
	      	clip->FindData("text/plain", B_MIME_TYPE,(const void**)&text,&textLen);
			if(textLen>0)
			 {
				char *ToAnsi=(char*)xmalloc(textLen+4);
				if(ToAnsi!=NULL)
				 {
					int32 DestLen=textLen;
					int32 State=0;
		     		int32 SourceLen=textLen;

			  		convert_from_utf8(B_MS_WINDOWS_1251_CONVERSION, text, &SourceLen, ToAnsi, &DestLen, &State);
			  		ToAnsi[DestLen]=0;
		  			      		
		      		uint8 *translated_data;
					uint32 length = DestLen;

					translated_data = lf2crlf((uint8*)ToAnsi, &length);
					cliprdr_send_data(translated_data, length);
					xfree(translated_data);	
					
	      			cliprdr_send_simple_native_format_announce(RDP_CF_TEXT);
	      			xfree(ToAnsi);
   		    	 }
   		     }
	     }
  		be_clipboard->Unlock();
	   }
	 }
	else
	{
	 cliprdr_send_data(NULL, 0);
	}
}

void
ui_clip_sync(void)
{
	
	cliprdr_send_simple_native_format_announce(RDP_CF_TEXT);	
}



void
xclip_init(void)
{
	
	if (!cliprdr_init())
		return;
}
}
