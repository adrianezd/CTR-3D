/* Community mods from CTR-ModSDK, ported into the native build. See include/native_mods.h for the
 * list and credits. Included by main.c after game_unity.h, so it shares the game's translation unit
 * (it calls a few of the game's internal helpers directly). */

#include <native_mods.h>

#ifdef CTR_NATIVE

int g_modEngineSelect = 1;
int g_modRandomRivals = 1;
int g_modBlueFire = 1;
int g_modBlueFlag = 1;
int g_modPaintJobs = 1;
int g_modKartTextures = 1;

#define MOD_LANGS 6
#define MOD_CHARACTERS 16
#define MOD_ENGINE_CLASSES 5
#define MOD_ENGINE_UNLIMITED 4

/* ------------------------------------------------------------------ strings --- */

/* Arcade difficulty rows added by ExtraDifficulty. Uppercase ASCII only (no accented glyphs). */
static const char *const s_modSuperHard[MOD_LANGS] = {"SUPER HARD", "SUPER DIFFICILE", "SUPER SCHWER", "SUPER DIFFICILE", "SUPER DIFICIL", "SUPER MOEILIJK"};
static const char *const s_modUltraHard[MOD_LANGS] = {"ULTRA HARD", "ULTRA DIFFICILE", "ULTRA SCHWER", "ULTRA DIFFICILE", "ULTRA DIFICIL", "ULTRA MOEILIJK"};

/* Engine class names, in class order: 0 balanced, 1 acceleration, 2 speed, 3 turning, 4 unlimited */
static const char *const s_modEngineNames[MOD_LANGS][MOD_ENGINE_CLASSES] = {
    {"BALANCED", "ACCELERATION", "SPEED", "TURNING", "UNLIMITED"},
    {"EQUILIBRE", "ACCELERATION", "VITESSE", "VIRAGE", "ILLIMITE"},
    {"AUSGEWOGEN", "BESCHLEUNIGUNG", "TEMPO", "KURVEN", "UNBEGRENZT"},
    {"EQUILIBRATO", "ACCELERAZIONE", "VELOCITA", "STERZO", "ILLIMITATO"},
    {"EQUILIBRADO", "ACELERACION", "VELOCIDAD", "GIRO", "ILIMITADO"},
    {"GEBALANCEERD", "ACCELERATIE", "SNELHEID", "BOCHTEN", "ONBEPERKT"},
};

static s32 NativeMods_Lang(void)
{
	s32 lang = NativeOptions_LanguageIndex();
	return ((lang >= 0) && (lang < MOD_LANGS)) ? lang : 0;
}

void NativeMods_InstallStrings(void)
{
	if ((sdata->lngStrings == NULL) || (sdata->numLngStrings <= LNG_UNUSED_0EE))
	{
		return;
	}

	sdata->lngStrings[LNG_UNUSED_0ED] = (char *)s_modSuperHard[NativeMods_Lang()];
	sdata->lngStrings[LNG_UNUSED_0EE] = (char *)s_modUltraHard[NativeMods_Lang()];
}

/* ------------------------------------------------------------- EngineSelect --- */

/* Retail engine class per character, captured before anything is changed. The NTSC-U data has
 * only four classes; "unlimited" is the PAL/Japan fifth class, applied the way Penta's PAL class
 * is (engineID stays TURN, the stats are written at kart birth, see include/native_penta.h). */
static u8 s_engineRetail[MOD_CHARACTERS];
static s8 s_engineChoice[MOD_CHARACTERS]; /* -1 = the character's own class */
static b32 s_engineCaptured;
static u8 s_engineConfirmed; /* bit per player: class confirmed with a second Cross */

static void NativeMods_EngineCapture(void)
{
	if (s_engineCaptured)
	{
		return;
	}

	for (s32 c = 0; c < MOD_CHARACTERS; c++)
	{
		s_engineRetail[c] = (u8)data.MetaDataCharacters[c].engineID;
		s_engineChoice[c] = -1;
	}
	s_engineCaptured = true;
}

static void NativeMods_EngineSet(s32 characterID, s32 engineClass)
{
	if ((characterID < 0) || (characterID >= MOD_CHARACTERS))
	{
		return;
	}

	NativeMods_EngineCapture();
	s_engineChoice[characterID] = (s8)engineClass;

	if (engineClass < 0)
	{
		data.MetaDataCharacters[characterID].engineID = s_engineRetail[characterID];
	}
	else
	{
		data.MetaDataCharacters[characterID].engineID = (engineClass == MOD_ENGINE_UNLIMITED) ? 3 : engineClass;
	}
}

static s32 NativeMods_EngineOf(s32 characterID)
{
	NativeMods_EngineCapture();

	if ((characterID < 0) || (characterID >= MOD_CHARACTERS))
	{
		return 0;
	}
	if (s_engineChoice[characterID] >= 0)
	{
		return s_engineChoice[characterID];
	}
	if ((characterID == PENTA_PENGUIN) && g_pentaPal)
	{
		return MOD_ENGINE_UNLIMITED;
	}
	return s_engineRetail[characterID];
}

/* Character select opened: every character back to its own class, so CPU drivers always race
 * with retail engines and only the player's pick changes anything. */
void NativeMods_CharacterSelectEnter(void)
{
	NativeMods_EngineCapture();

	for (s32 c = 0; c < MOD_CHARACTERS; c++)
	{
		NativeMods_EngineSet(c, -1);
	}
	s_engineConfirmed = 0;
}

b32 NativeMods_EngineSelectActive(void)
{
	return g_modEngineSelect != 0;
}

void NativeMods_EngineSelectBegin(s32 player, s32 characterID)
{
	s_engineConfirmed &= (u8) ~(1u << player);
	NativeMods_EngineSet(characterID, NativeMods_EngineOf(characterID));
}

/* Input for a player who already picked a character. Returns true when every player has picked
 * a character and confirmed a class, i.e. the screen should move on. */
b32 NativeMods_EngineSelectInput(s32 player, s32 characterID, u32 button, s32 numPlayers, u32 selectFlags)
{
	u8 bit = (u8)(1u << player);
	b32 confirmed = (s_engineConfirmed & bit) != 0;

	if (!confirmed && ((button & (BTN_LEFT | BTN_RIGHT)) != 0))
	{
		s32 engine = NativeMods_EngineOf(characterID);
		engine = (button & BTN_LEFT) ? (engine + MOD_ENGINE_CLASSES - 1) % MOD_ENGINE_CLASSES : (engine + 1) % MOD_ENGINE_CLASSES;
		NativeMods_EngineSet(characterID, engine);
		OtherFX_Play(0, 1);
		return false;
	}

	if (!confirmed && ((button & (BTN_CIRCLE | BTN_CROSS_one)) != 0))
	{
		s_engineConfirmed |= bit;
		OtherFX_Play(1, 1);

		u32 allPlayers = (1u << numPlayers) - 1;
		return ((selectFlags & allPlayers) == allPlayers) && ((s_engineConfirmed & allPlayers) == allPlayers);
	}

	return false;
}

/* Back on a selected player: drop the class confirmation first, the character second. Returns
 * true when the back press was consumed here. */
b32 NativeMods_EngineSelectBack(s32 player)
{
	u8 bit = (u8)(1u << player);

	if (s_engineConfirmed & bit)
	{
		s_engineConfirmed &= (u8)~bit;
		OtherFX_Play(2, 1);
		return true;
	}
	return false;
}

b32 NativeMods_EngineSelectDrawName(s32 player, s32 characterID, s32 x, s32 y, s32 fontType, s32 numPlayers)
{
	char *name = (char *)s_modEngineNames[NativeMods_Lang()][NativeMods_EngineOf(characterID)];
	b32 confirmed = (s_engineConfirmed & (1u << player)) != 0;

	DecalFont_DrawLine(name, x, y, (s16)fontType, (JUSTIFY_CENTER | (confirmed ? WHITE : ORANGE)));

	if (!confirmed && (numPlayers <= 2))
	{
		MainFreeze_ConfigDrawArrows((s16)x, (s16)y, name);
	}
	return true;
}

b32 NativeMods_WantsUnlimitedClass(s32 characterID)
{
	if ((characterID < 0) || (characterID >= MOD_CHARACTERS))
	{
		return false;
	}
	if (g_modEngineSelect && s_engineCaptured && (s_engineChoice[characterID] >= 0))
	{
		return s_engineChoice[characterID] == MOD_ENGINE_UNLIMITED;
	}
	return g_pentaPal && (characterID == PENTA_PENGUIN);
}

/* ---------------------------------------------------------- ExtraDifficulty --- */

static struct MenuRow s_modDifficultyRows[6];

/* Called every frame the title menus are positioned: keeps the five-row table in step with the
 * retail three rows (their labels change between arcade and cup) and swaps it in. */
void NativeMods_SyncDifficultyRows(void)
{
	for (s32 i = 0; i < 3; i++)
	{
		s_modDifficultyRows[i] = D230.rowsDifficulty[i];
	}
	s_modDifficultyRows[2].rowOnPressDown = 3;

	s_modDifficultyRows[3] = (struct MenuRow){LNG_UNUSED_0ED, 2, 4, 3, 3};
	s_modDifficultyRows[4] = (struct MenuRow){LNG_UNUSED_0EE, 3, 4, 4, 4};
	s_modDifficultyRows[5] = D230.rowsDifficulty[3]; /* terminator */

	D230.menuDifficulty.rows = &s_modDifficultyRows[0];
}

/* Rows 3 and 4: the retail SUPER HARD cheat value and the community ULTRA HARD value. */
b32 NativeMods_DifficultyChoice(s16 row)
{
	if (row == 3)
	{
		sdata->gGT->arcadeDifficulty = 0x140;
		return true;
	}
	if (row == 4)
	{
		sdata->gGT->arcadeDifficulty = 0x280;
		return true;
	}
	return false;
}

/* ------------------------------------------------------ CharacterRandomizer --- */

/* CPU models loaded one by one (high LOD), outside retail's three-slot driverModelExtras array.
 * Slot 0 is unused: the player keeps driverModelExtras[0]. */
static DriverModelExtraSlot s_rivalModels[8];
static b32 s_rivalsRandom;

static b32 NativeMods_RandomRivalsAllowed(void)
{
	struct GameTracker *gGT = sdata->gGT;

	return g_modRandomRivals && ((gGT->gameMode1 & ARCADE_MODE) != 0) && ((gGT->gameMode1 & (ADVENTURE_MODE | TIME_TRIAL | MAIN_MENU)) == 0) &&
	       (gGT->numPlyrCurrGame == 1);
}

/* LOAD_Robots1P replacement. Returns false to let retail fill the standard CPU set. */
b32 NativeMods_RandomizeRivals(s32 playerCharacterID)
{
	struct GameTracker *gGT = sdata->gGT;

	s_rivalsRandom = false;

	if (!NativeMods_RandomRivalsAllowed())
	{
		return false;
	}

	data.characterIDs[0] = (s16)playerCharacterID;
	s_rivalsRandom = true;

	/* in a cup, the CPU drivers of the first track stay for the whole cup */
	if (((gGT->gameMode2 & CUP_ANY_KIND) != 0) && (gGT->cup.trackIndex != 0))
	{
		return true;
	}

	for (s32 i = 1; i < 8;)
	{
		MixRNG_Scramble();
		s32 id = ((Timer_GetTime_Total() & 0xf) + (sdata->randomNumber >> 8)) % 15;
		b32 taken = false;

		for (s32 j = 0; j < i; j++)
		{
			if (data.characterIDs[j] == id)
			{
				taken = true;
				break;
			}
		}

		if (!taken)
		{
			data.characterIDs[i++] = (s16)id;
		}
	}
	return true;
}

/* Replaces the 1P arcade pack for a randomized race. The load queue holds eight files: the
 * player's and six CPUs' high-LOD models, plus the 4P pack of the last CPU, which brings that
 * driver's model and every weapon model. Smaller than the retail 1P pack it replaces. */
b32 NativeMods_QueueRivalModels(struct BigHeader *bigfile, void (*callback)(struct LoadQueueSlot *))
{
	if (!s_rivalsRandom || !NativeMods_RandomRivalsAllowed())
	{
		return false;
	}

	LOAD_AppendQueue(bigfile, LT_GETADDR, BI_RACERMODELHI + data.characterIDs[0], &data.driverModelExtras[0].fileBase, LOAD_QUEUE_CALLBACK_SET_POINTER);

	for (s32 i = 1; i < 7; i++)
	{
		LOAD_AppendQueue(bigfile, LT_GETADDR, BI_RACERMODELHI + data.characterIDs[i], &s_rivalModels[i].fileBase, LOAD_QUEUE_CALLBACK_SET_POINTER);
	}

	LOAD_AppendQueue(bigfile, LT_GETADDR, BI_4PARCADEPACK + data.characterIDs[7], NULL, callback);
	return true;
}

void NativeMods_ClearRivalModels(void)
{
	for (s32 i = 0; i < 8; i++)
	{
		s_rivalModels[i].fileBase = NULL;
	}
}

void NativeMods_ResolveRivalModels(void)
{
	for (s32 i = 0; i < 8; i++)
	{
		if (s_rivalModels[i].fileBase != NULL)
		{
			s_rivalModels[i].model = (struct Model *)((u8 *)s_rivalModels[i].fileBase + LOAD_MODEL_FILE_HEADER_BYTES);
		}
	}
}

void NativeMods_StoreRivalModelPtrs(struct GameTracker *gGT)
{
	for (s32 i = 0; i < 8; i++)
	{
		struct Model *m = s_rivalModels[i].model;

		if ((m != NULL) && (m->id != -1))
		{
			gGT->modelPtr[m->id] = m;
		}
	}
}

struct Model *NativeMods_FindRivalModel(char *searchName)
{
	for (s32 i = 0; i < 8; i++)
	{
		struct Model *m = s_rivalModels[i].model;

		if ((m != NULL) && VehBirth_ModelNameEquals(m, searchName))
		{
			return m;
		}
	}
	return NULL;
}

/* ----------------------------------------------------------------- BlueFire --- */

static const u32 s_redFlamesClut[8] = {0x80DA809A, 0x805A80BA, 0x811A803A, 0x819A81DA, 0x813A817A, 0x829A821A, 0x835A839A, 0x82DA831A};
static const u32 s_blueFlamesClut[8] = {0xFF25FF65, 0xFFA5FF45, 0xFEE5FFC5, 0xFE65FE25, 0xFEC5FE85, 0xFD65FDE5, 0xFCA5FC65, 0xFD25FCE5};
static const u32 s_redPlumesClut[8] = {0x000083FF, 0x821B833E, 0x829D81B8, 0x80F6800D, 0x819A8054, 0x80D98011, 0x80078019, 0x84008015};
static const u32 s_bluePlumesClut[8] = {0x7FFFF260, 0xFC20F980, 0xF400FC20, 0xF980AC00, 0xF260F980, 0xB400FCA0, 0x8800FC20, 0x8800FC20};
static b32 s_fireIsBlue;

static void NativeMods_SetFire(b32 blue)
{
	RECT16 flames = {176, 256, 16, 1};
	RECT16 plumes = {464, 257, 16, 1};

	if (s_fireIsBlue == blue)
	{
		return;
	}

	LoadImage(&flames, (void *)(blue ? s_blueFlamesClut : s_redFlamesClut));
	LoadImage(&plumes, (void *)(blue ? s_bluePlumesClut : s_redPlumesClut));
	s_fireIsBlue = blue;
}

static void NativeMods_BlueFireFrame(void)
{
	struct GameTracker *gGT = sdata->gGT;
	struct Driver *d = gGT->drivers[0];

	if (gGT->gameMode1 & (START_OF_RACE | MAIN_MENU | END_OF_RACE | GAME_CUTSCENE | LOADING))
	{
		return;
	}
	if (!g_modBlueFire)
	{
		NativeMods_SetFire(false);
		return;
	}
	if ((d != NULL) && (d->reserves != 0))
	{
		NativeMods_SetFire(d->fireSpeedCap == 0x4800);
	}
}

/* ---------------------------------------------------------------- PaintJobs --- */

typedef union {
	struct
	{
		const char *front;
		const char *back;
		const char *floor;
		const char *brown;
		const char *motorside;
		const char *motortop;
		const char *bridge;
		const char *exhaust;
	};
	const char *p[8];
} Texture;

#include "mods/mod_paintjobs_data.h"

#define MOD_PAINTS 21
#define MOD_PAINT_CHARACTERS 15

static s8 s_paint[MOD_PAINT_CHARACTERS] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};

static void NativeMods_LoadPaint(s32 character, b32 menu, b32 race)
{
	const Texture *colors = s_paintColors[s_paint[character]];

	for (s32 i = 0; i < 8; i++)
	{
		if (menu)
		{
			LoadImage((RECT16 *)PAINTP_M[character].p[i], (void *)colors->p[i]);
		}
		if (race)
		{
			LoadImage((RECT16 *)PAINTP_R[character].p[i], (void *)colors->p[i]);
		}
	}
}

/* VRAM reloads restore retail colours; put back every non-default paint. In a race only the race
 * slots are rewritten (the menu slots are other textures there). */
static void NativeMods_ReapplyPaints(void)
{
	b32 inMenu = (sdata->gGT->gameMode1 & MAIN_MENU) != 0;

	for (s32 c = 0; c < MOD_PAINT_CHARACTERS; c++)
	{
		if (s_paint[c] != c)
		{
			NativeMods_LoadPaint(c, inMenu, true);
		}
	}
}

/* Character select, player one: L1/L2 and R1/R2 cycle the paint of the highlighted character.
 * Both shoulder pairs, so it also works on a 3DS without ZL/ZR. */
void NativeMods_CharacterMenuFrame(void)
{
	u32 tap = sdata->buttonTapPerPlayer[0];
	s32 character = data.characterIDs[0];

	if (!g_modPaintJobs || (character < 0) || (character >= MOD_PAINT_CHARACTERS))
	{
		return;
	}

	if (tap & (BTN_R1 | BTN_R2))
	{
		s_paint[character] = (s8)((s_paint[character] + 1) % MOD_PAINTS);
	}
	else if (tap & (BTN_L1 | BTN_L2))
	{
		s_paint[character] = (s8)((s_paint[character] + MOD_PAINTS - 1) % MOD_PAINTS);
	}
	else
	{
		return;
	}

	NativeMods_LoadPaint(character, true, true);
	OtherFX_Play(0x6a, 0);
}

/* ---------------------------------------------------------- NewKartTextures --- */

struct KartTexEntry
{
	const char *image;
	const RECT16 *pos;
};

#include "mods/mod_karttex_data.h"

static void NativeMods_LoadKartTextures(void)
{
	for (u32 i = 0; i < sizeof(KARTTEX1) / sizeof(KARTTEX1[0]); i++)
	{
		LoadImage((RECT16 *)KARTTEX1[i].pos, (void *)KARTTEX1[i].image);
	}
	for (u32 i = 0; i < sizeof(KARTTEX2) / sizeof(KARTTEX2[0]); i++)
	{
		LoadImage((RECT16 *)KARTTEX2[i].pos, (void *)KARTTEX2[i].image);
	}
}

/* --------------------------------------------------------------- CustomFlag --- */

int NativeMods_FlagStyle(int style)
{
	return ((style == 0) && g_modBlueFlag) ? 3 : style;
}

/* -------------------------------------------------------------------- hooks --- */

void NativeMods_Frame(void)
{
	NativeMods_BlueFireFrame();
}

void NativeMods_OnVramLoaded(void)
{
	/* a reload put the retail fire palette back */
	s_fireIsBlue = false;

	if (g_modKartTextures)
	{
		NativeMods_LoadKartTextures();
	}
	if (g_modPaintJobs)
	{
		NativeMods_ReapplyPaints();
	}
}

#endif /* CTR_NATIVE */
