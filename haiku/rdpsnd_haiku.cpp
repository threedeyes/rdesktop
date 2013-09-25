#include <SoundPlayer.h>
#include <Application.h>
#include <media/MediaDefs.h>
#include <MediaRoster.h>
#include <GameSoundDefs.h>
#include <MediaDefs.h>
#include <MediaFormats.h>
#include <Screen.h>
#include <GraphicsDefs.h>
#include <SoundPlayer.h>

extern "C"
{
#include "../rdesktop.h"
#include "../rdpsnd.h"
#include "../rdpsnd_dsp.h"
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

static short g_samplewidth;
static BSoundPlayer *p = 0;
static char *outbuf = 0;
int	buf_len;
static int g_snd_rate;
bool	opened = false;

void proc(void *cookie, void *buffer, size_t len, const media_raw_audio_format &format)
{
	//printf("proc:  %d\n", len);

	struct audio_packet *packet;
	unsigned int i;
	STREAM out;

	memset(buffer,0,len);
	
	if (rdpsnd_queue_empty())
		return; 

	packet = rdpsnd_queue_current_packet();
	out = &packet->s;
	
	if(out->end != out->p)
		memcpy(buffer,out->p,len);
	
	out->p += len;
	
	if (out->p >= out->end)	{			
			rdpsnd_send_completion(packet->tick, packet->index);
			rdpsnd_queue_next();
	}	
	
}

BOOL
beos_open(void)
{
	//printf("beos_open\n");
	
   	buf_len=32768/8;

	rdpsnd_queue_init();
	if(opened==false)
	{
		media_raw_audio_format form = {
			22050,
			2,
			media_raw_audio_format::B_AUDIO_SHORT,
			B_MEDIA_LITTLE_ENDIAN,
			buf_len /  2
		};
		outbuf = (char *)malloc(buf_len);
		p = new BSoundPlayer(&form, "audio_play", proc);
	
		if(p->InitCheck() != B_OK) {
			delete p; p = 0;
			return 0;
		}
	
		p->Start();
		p->SetHasData(true);
		opened = true;
	}
	
	return True;
}

void
beos_close(void)
{
	/* Ack all remaining packets */
	//printf("beos_close\n");
	
	while (!rdpsnd_queue_empty())
	{
		rdpsnd_send_completion(rdpsnd_queue_current_packet()->tick,
				       rdpsnd_queue_current_packet()->index);
		rdpsnd_queue_next();
	}
}

void be_close_sound()
{
	//printf("be_close_sound\n");

	if(p) {
		p->Stop();
		delete p;
		p = 0;
	}
	free(outbuf); 
	outbuf = 0;
}

BOOL
beos_format_supported(WAVEFORMATEX * pwfx)
{
	if (pwfx->wFormatTag != WAVE_FORMAT_PCM)return False;
	if (pwfx->nChannels != 2)return False;
	if (pwfx->wBitsPerSample != 16)return False;
	//printf("wave_out_format_supported Ch:%d Bt:%d Sm:%d\n",pwfx->nChannels,pwfx->wBitsPerSample,pwfx->nSamplesPerSec);
	return True;
}

BOOL
beos_set_format(WAVEFORMATEX * pwfx)
{
	//printf("beos_set_format\n");
	
	g_samplewidth = pwfx->wBitsPerSample / 8;
	if (pwfx->nChannels == 2)
	{
		g_samplewidth *= 2;
	}	

	g_snd_rate = pwfx->nSamplesPerSec;


	return True;
}

void
beos_volume(uint16 left, uint16 right)
{
	float l = left/65535.0;
	float r = right/65535.0;
	float gain = (l+r)/2;
	float pan = (r-l)/(gain*2);
}


void
beos_play(void)
{

}



struct audio_driver *
beos_register(char *options)
{
	static struct audio_driver beos_driver;

	beos_driver.wave_out_write = rdpsnd_queue_write;
	beos_driver.wave_out_open = beos_open;
	beos_driver.wave_out_close = beos_close;
	beos_driver.wave_out_format_supported = beos_format_supported;
	beos_driver.wave_out_set_format = beos_set_format;
	beos_driver.wave_out_volume = rdpsnd_dsp_softvol_set;
	beos_driver.wave_out_play = beos_play;
	beos_driver.name = xstrdup("beos");
	beos_driver.description = xstrdup("BeOS output driver");
	beos_driver.need_byteswap_on_be = 1;
	beos_driver.need_resampling = 0;
	beos_driver.next = NULL;


	return &beos_driver;
}
}