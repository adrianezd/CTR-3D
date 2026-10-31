#ifndef N3DS_DSPFIRM_H
#define N3DS_DSPFIRM_H

/* The question about the DSP firmware (sdmc:/3ds/dspfirm.cdc), drawn by the bottom screen.
 * ndspInit() needs that file on a console whose CFW does not load it from NAND. When the audio
 * cannot start because it is missing or damaged, the player is asked whether to extract it from
 * this console's own Home Menu (dsp_firm.c). It is Nintendo's code, so it is never shipped. */

enum
{
	N3DS_DSPFIRM_NONE = 0,
	N3DS_DSPFIRM_ASK,     /* missing or damaged: EXTRACT / NO SOUND */
	N3DS_DSPFIRM_WORKING, /* extracting */
	N3DS_DSPFIRM_DONE,
	N3DS_DSPFIRM_FAILED
};

/* After ndspInit failed: opens the question if the firmware file is the reason. */
void N3DS_DspFirm_Raise(void);
int N3DS_DspFirm_State(void);
int N3DS_DspFirm_Missing(void);          /* else damaged */
const char *N3DS_DspFirm_Failure(void);  /* "ERROR n / code" after a failure */
void N3DS_DspFirm_Answer(int yes);       /* the buttons */
void N3DS_DspFirm_Pump(void);            /* once per input poll, game thread */
void N3DS_DspFirm_FrameTick(void);       /* once per bottom-screen frame, game thread */

/* Frames the "EXTRACTING..." box is on the screen before the extraction blocks the game thread. */
#define N3DS_DSPFIRM_SHOWN_FRAMES 6

#endif
