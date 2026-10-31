#include <3ds.h>
#include <stdio.h>
#include <string.h>

#include "dsp_firm.h"
#include "n3ds_audio_thread.h"
#include "n3ds_backend.h"
#include "n3ds_dspfirm.h"

static volatile int s_state = N3DS_DSPFIRM_NONE;
static DspFirmStatus s_status;
static char s_failure[48];
static int s_workFrames;

void N3DS_DspFirm_Raise(void)
{
	if (s_state != N3DS_DSPFIRM_NONE)
	{
		return;
	}
	s_status = DspFirm_Check(DSPFIRM_PATH);
	if (s_status == DSPFIRM_OK)
	{
		return; /* the audio failed for another reason: nothing to offer */
	}
	N3DS_DebugLog(s_status == DSPFIRM_MISSING ? "[CTR-3DS] dsp firmware: missing, asking" : "[CTR-3DS] dsp firmware: damaged, asking");
	s_state = N3DS_DSPFIRM_ASK;
}

int N3DS_DspFirm_State(void)
{
	return s_state;
}

int N3DS_DspFirm_Missing(void)
{
	return s_status == DSPFIRM_MISSING;
}

const char *N3DS_DspFirm_Failure(void)
{
	return s_failure;
}

void N3DS_DspFirm_Answer(int yes)
{
	if (s_state == N3DS_DSPFIRM_ASK)
	{
		if (yes)
		{
			s_workFrames = 0;
			s_state = N3DS_DSPFIRM_WORKING;
		}
		else
		{
			N3DS_DebugLog("[CTR-3DS] dsp firmware: extraction declined");
			s_state = N3DS_DSPFIRM_NONE;
		}
	}
	else if ((s_state == N3DS_DSPFIRM_DONE) || (s_state == N3DS_DSPFIRM_FAILED))
	{
		s_state = N3DS_DSPFIRM_NONE;
	}
}

/* Once per frame the bottom screen is drawn: counts the frames "EXTRACTING..." has been up for
 * (the input is polled more often than the panel is painted, so the poll cannot do the counting). */
void N3DS_DspFirm_FrameTick(void)
{
	if (s_state == N3DS_DSPFIRM_WORKING)
	{
		s_workFrames++;
	}
}

/* The extraction takes a moment and blocks the game thread: it starts once "EXTRACTING..." has
 * been on the screen for a few frames. */
void N3DS_DspFirm_Pump(void)
{
	int32_t detail = 0;
	char note[600];
	DspFirmExtractResult r;

	if ((s_state != N3DS_DSPFIRM_WORKING) || (s_workFrames < N3DS_DSPFIRM_SHOWN_FRAMES))
	{
		return;
	}
	r = DspFirm_Extract(DSPFIRM_PATH, &detail);
	snprintf(note, sizeof(note), "[CTR-3DS] dsp firmware: extraction %d (%s), detail %08lX | %s", (int)r, DspFirm_ExtractText(r),
	         (unsigned long)detail, g_dspfirm_trace);
	N3DS_DebugLog(note);
	if (r == DSPFIRM_EX_OK)
	{
		if (N3DS_NdspRetryInit())
		{
			s_state = N3DS_DSPFIRM_DONE;
			return;
		}
		snprintf(s_failure, sizeof(s_failure), "AUDIO DID NOT START");
	}
	else
	{
		snprintf(s_failure, sizeof(s_failure), "ERROR %d / %08lX", (int)r, (unsigned long)detail);
	}
	s_state = N3DS_DSPFIRM_FAILED;
}
