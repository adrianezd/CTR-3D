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

/* Adventure mods and the CTR Unlimited-style modes change how the game plays, so they start off. */
int g_modAdvRandom = 0;
int g_modAdvDifficulty = 0; /* 0 classic (retail rubber band), 1 easy, 2 medium, 3 hard, 4 super hard, 5 ultra hard */
int g_modMirror = 0;
int g_modMoonGravity = 0;
int g_modItemChaos = 0;
int g_modIcy = 0;

#define MOD_LANGS 6
#define MOD_CHARACTERS 16
#define MOD_ENGINE_CLASSES 5
#define MOD_ENGINE_UNLIMITED 4

/* Skins (defined further down) */
static void NativeMods_SkinInit(void);
static void NativeMods_SkinInstallNames(void);
static void NativeMods_SkinApplyIcon(s32 c);
static void NativeMods_LoadSkinModels(void);
static struct Model *NativeMods_FindSkinModel(char *searchName);
static void NativeMods_SkinCharacterMenu(void);
static u8 s_skinOn[16];
static struct Model *s_skinModel[16];

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
	NativeMods_SkinInstallNames();
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

#define MOD_RIVALS_OUTSIDE_PACK 4

/* The player's 1P arcade pack carries the CPUs LOAD_Robots1P would pick: the first seven
 * characters, skipping the player. */
static b32 NativeMods_InPlayerPack(s32 playerCharacterID, s32 characterID)
{
	s32 slot = (characterID < playerCharacterID) ? characterID : (characterID - 1);

	return (characterID != playerCharacterID) && (slot < 7);
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

	s32 outsidePack = 0;

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

		/* memory: at most MOD_RIVALS_OUTSIDE_PACK drivers that the player's pack does not carry */
		if (!taken && !NativeMods_InPlayerPack(playerCharacterID, id))
		{
			if (outsidePack >= MOD_RIVALS_OUTSIDE_PACK)
			{
				taken = true;
			}
			else
			{
				outsidePack++;
			}
		}

		if (!taken)
		{
			data.characterIDs[i++] = (s16)id;
		}
	}
	return true;
}

/* A randomized race keeps the player's retail 1P arcade pack: it carries the player, the seven
 * CPUs retail would pick, every weapon model and the 1P HUD icons (the 4P pack the retail mod used
 * brings the 4P icon set, and the 1P HUD then draws from icon groups that are not there -- a
 * data abort at the first race). CPUs the pack does not carry are loaded one by one, low LOD,
 * at most MOD_RIVALS_OUTSIDE_PACK of them, so the load stays within a few dozen KB of retail's. */
b32 NativeMods_QueueRivalModels(struct BigHeader *bigfile, void (*callback)(struct LoadQueueSlot *))
{
	s32 player = data.characterIDs[0];

	if (!s_rivalsRandom || !NativeMods_RandomRivalsAllowed())
	{
		return false;
	}

	for (s32 i = 1; i < 8; i++)
	{
		if (!NativeMods_InPlayerPack(player, data.characterIDs[i]))
		{
			LOAD_AppendQueue(bigfile, LT_GETADDR, BI_RACERMODELLOW + data.characterIDs[i], &s_rivalModels[i].fileBase, LOAD_QUEUE_CALLBACK_SET_POINTER);
		}
	}

	LOAD_AppendQueue(bigfile, LT_GETADDR, BI_1PARCADEPACK + player, NULL, callback);
	return true;
}

/* SaveRAM, the native way: the extra CPU models cost MEMPACK room retail never had to spare, so
 * in a randomized race the JIT pools (threads, instances, stacks, particles...; ~85 KB in a 1P
 * race) come from this host buffer instead -- the job SaveRAM did on PS1 by moving pools into
 * unused executable space. Every other mode keeps retail's MEMPACK layout untouched. */
static u8 s_modPoolArena[192 * 1024] __attribute__((aligned(16)));
static u32 s_modPoolUsed;
static b32 s_modPoolArmed;

void NativeMods_PoolArenaBegin(void)
{
	s_modPoolUsed = 0;
	s_modPoolArmed = NativeMods_RandomRivalsAllowed();
}

void NativeMods_PoolArenaEnd(void)
{
	s_modPoolArmed = false;
}

void *NativeMods_PoolAlloc(int size)
{
	u32 aligned = ((u32)size + 15u) & ~15u;
	void *p;

	if (!s_modPoolArmed || (size <= 0) || ((s_modPoolUsed + aligned) > sizeof(s_modPoolArena)))
	{
		return NULL;
	}

	p = &s_modPoolArena[s_modPoolUsed];
	s_modPoolUsed += aligned;
	return p;
}

void NativeMods_ClearRivalModels(void)
{
	for (s32 c = 0; c < 16; c++)
	{
		s_skinModel[c] = NULL;
	}
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
	NativeMods_LoadSkinModels();
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
	struct Model *skin = NativeMods_FindSkinModel(searchName);

	if (skin != NULL)
	{
		return skin;
	}
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

	NativeMods_SkinCharacterMenu();

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

/* ------------------------------------------------- EngineSelect (adventure) --- */

/* Adventure garage, after the first Cross: Up/Down cycle the class of the highlighted character
 * (the stat bars follow engineID), the second Cross goes on to the name entry as in retail. */
b32 NativeMods_GarageEngineInput(s32 characterID, u32 tap)
{
	s32 engine;

	if (!g_modEngineSelect || ((tap & (BTN_UP | BTN_DOWN)) == 0))
	{
		return false;
	}

	engine = NativeMods_EngineOf(characterID);
	engine = (tap & BTN_DOWN) ? (engine + MOD_ENGINE_CLASSES - 1) % MOD_ENGINE_CLASSES : (engine + 1) % MOD_ENGINE_CLASSES;
	NativeMods_EngineSet(characterID, engine);
	OtherFX_Play(0, 1);
	return true;
}

char *NativeMods_EngineName(s32 characterID)
{
	return (char *)s_modEngineNames[NativeMods_Lang()][NativeMods_EngineOf(characterID)];
}

/* ----------------------------------------------------------- AdvDifficulty --- */

/* Replaces the adventure rubber band (max difficulty minus times lost) with a fixed level, the
 * way the mod patched BOTS_Adv_AdjustDifficulty; boss races take twice the value, as there. */
s32 NativeMods_AdvDifficulty(s32 computed, b32 boss)
{
	s32 value;

	switch (g_modAdvDifficulty)
	{
	case 1:
	case 2:
	case 3:
		value = D230.cupDifficulty.speed[g_modAdvDifficulty - 1];
		break;
	case 4:
		value = 0x140;
		break;
	case 5:
		value = 0x280;
		break;
	default:
		return computed;
	}
	return boss ? (value * 2) : value;
}

/* ----------------------------------------------------------- AdvRandomizer --- */

/* Which track sits behind each warp pad, shuffled inside its own kind (the 16 trophy tracks,
 * Slide Coliseum and Turbo Track, the four crystal-challenge arenas, the five gem cups), and which
 * boss waits in each of the four hub garages. The retail mod reshuffled on every new game and did
 * not save the result; here the seed lives in <base dir>/advrandom.cfg, so a playthrough keeps its
 * layout across launches. Delete the file for a new one. */
#define MOD_ADV_SEED_FILE "advrandom.cfg"

static s16 s_advPad[24];
static s16 s_advCup[5];
static s16 s_advHub[5];
static b32 s_advReady;

static u32 NativeMods_AdvNext(u32 *state)
{
	*state = (*state * 1103515245u) + 12345u;
	return (*state >> 16) & 0x7fff;
}

static void NativeMods_AdvShuffle(s16 *table, const s16 *slots, s32 count, u32 *state)
{
	for (s32 i = count - 1; i > 0; i--)
	{
		s32 j = (s32)(NativeMods_AdvNext(state) % (u32)(i + 1));
		s16 tmp = table[slots[i]];
		table[slots[i]] = table[slots[j]];
		table[slots[j]] = tmp;
	}
}

static u32 NativeMods_AdvSeed(void)
{
	char path[1024];
	FILE *file;
	unsigned int seed = 0;

	if (!NativePath_Join(path, sizeof(path), NativeStr8_FromCString(NativeAssets_GetBaseDir()), NATIVE_STR8_LIT(MOD_ADV_SEED_FILE)))
	{
		return 0x1234567u;
	}

	file = fopen(path, "rb");
	if (file != NULL)
	{
		if (fscanf(file, "%u", &seed) != 1)
		{
			seed = 0;
		}
		fclose(file);
	}

	if (seed == 0)
	{
		MixRNG_Scramble();
		seed = ((u32)Timer_GetTime_Total() * 2654435761u) ^ (u32)sdata->randomNumber ^ (u32)sdata->frameCounter;
		if (seed == 0)
		{
			seed = 1;
		}

		file = fopen(path, "wb");
		if (file != NULL)
		{
			fprintf(file, "%u\n", seed);
			fclose(file);
		}
	}
	return seed;
}

static void NativeMods_AdvBuild(void)
{
	static const s16 races[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
	static const s16 bonus[2] = {16, 17};
	static const s16 arenas[4] = {18, 19, 21, 23};
	static const s16 cups[5] = {0, 1, 2, 3, 4};
	static const s16 hubs[4] = {1, 2, 3, 4};
	u32 state;

	if (s_advReady)
	{
		return;
	}

	state = NativeMods_AdvSeed();

	for (s32 i = 0; i < 24; i++)
	{
		s_advPad[i] = (s16)i;
	}
	for (s32 i = 0; i < 5; i++)
	{
		s_advCup[i] = (s16)(ADVENTURE_CUP_SYNTHETIC_LEVEL_ID_BASE + i);
		s_advHub[i] = (s16)i;
	}

	NativeMods_AdvShuffle(s_advPad, races, 16, &state);
	NativeMods_AdvShuffle(s_advPad, bonus, 2, &state);
	NativeMods_AdvShuffle(s_advPad, arenas, 4, &state);
	NativeMods_AdvShuffle(s_advCup, cups, 5, &state);
	NativeMods_AdvShuffle(s_advHub, hubs, 4, &state);
	s_advReady = true;
}

s32 NativeMods_AdvRandomPad(s32 levelID)
{
	if (!g_modAdvRandom)
	{
		return levelID;
	}

	NativeMods_AdvBuild();

	if ((levelID >= 0) && (levelID < 24))
	{
		return s_advPad[levelID];
	}
	if ((levelID >= ADVENTURE_CUP_SYNTHETIC_LEVEL_ID_BASE) && (levelID < ADVENTURE_CUP_SYNTHETIC_LEVEL_ID_BASE + 5))
	{
		return s_advCup[levelID - ADVENTURE_CUP_SYNTHETIC_LEVEL_ID_BASE];
	}
	return levelID;
}

s32 NativeMods_AdvRandomHub(s32 hubID)
{
	if (!g_modAdvRandom || (hubID < 0) || (hubID > 4))
	{
		return hubID;
	}

	NativeMods_AdvBuild();
	return s_advHub[hubID];
}

/* ---------------------------------------------------- CTR Unlimited modes --- */

/* Mirror mode: the top screen is flipped at the display transfer (n3ds_backend.c, which also
 * swaps the per-eye disparity so stereo depth survives the flip) and steering is swapped here.
 * Only while racing: menus, pause and results stay readable. */
#ifdef __3DS__
extern int g_n3dsMirrorTop;
#endif

b32 NativeMods_MirrorActive(void)
{
	struct GameTracker *gGT = sdata->gGT;

	return g_modMirror && (gGT != NULL) && ((gGT->gameMode1 & (MAIN_MENU | PAUSE_ALL | LOADING | GAME_CUTSCENE | END_OF_RACE)) == 0);
}

u32 NativeMods_MirrorButtons(u32 buttons)
{
	u32 lr = buttons & (BTN_LEFT | BTN_RIGHT);

	if ((lr == 0) || (lr == (BTN_LEFT | BTN_RIGHT)))
	{
		return buttons;
	}
	return (buttons & ~(u32)(BTN_LEFT | BTN_RIGHT)) | ((lr == BTN_LEFT) ? BTN_RIGHT : BTN_LEFT);
}

/* Moon gravity: 40% of every kart's gravity (karts are born per race, so it applies next race). */
void NativeMods_DriverConsts(struct Driver *driver)
{
	if (g_modMoonGravity)
	{
		driver->const_Gravity = (s16)((driver->const_Gravity * 2) / 5);
	}
}

/* Item chaos: any weapon from any box, whatever your place (not in battle or boss races). */
void NativeMods_ChaosItem(struct Driver *driver)
{
	static const u8 pool[] = {HELD_ITEM_TURBO,   HELD_ITEM_BOMB_1X,  HELD_ITEM_MISSILE_1X, HELD_ITEM_TNT,         HELD_ITEM_POTION,
	                          HELD_ITEM_SHIELD,  HELD_ITEM_MASK,     HELD_ITEM_CLOCK,      HELD_ITEM_WARPBALL,    HELD_ITEM_BOMB_3X,
	                          HELD_ITEM_MISSILE_3X, HELD_ITEM_INVISIBILITY, HELD_ITEM_SUPER_ENGINE};
	struct GameTracker *gGT = sdata->gGT;

	if (!g_modItemChaos || (gGT->gameMode1 & (BATTLE_MODE | ADVENTURE_BOSS)))
	{
		return;
	}

	MixRNG_Scramble();
	driver->heldItemID = pool[((u32)sdata->randomNumber >> 8) % (u32)sizeof(pool)];
}

/* Icy tracks: retail's SLEDROAD cheat flag, kept in step with the toggle. */
static void NativeMods_SyncFlags(void)
{
	static int s_icyWas;
	struct GameTracker *gGT = sdata->gGT;

	if (gGT == NULL)
	{
		return;
	}
	if (g_modIcy)
	{
		gGT->gameMode2 |= CHEAT_ICY;
	}
	else if (s_icyWas)
	{
		gGT->gameMode2 &= ~CHEAT_ICY;
	}
	s_icyWas = g_modIcy;
}

/* ------------------------------------------------------------- CustomCups --- */

/* Arcade cup screen: SELECT edits the highlighted cup, Up/Down pick one of its four races,
 * Left/Right change the track, R fills the cup at random. Picks index the 18 arcade tracks of
 * the track-select list, as in the original mod. */
static const char *const s_modCupHint[MOD_LANGS] = {"SELECT: CUSTOM CUP", "SELECT: COUPE PERSO", "SELECT: EIGENER CUP",
                                                    "SELECT: COPPA LIBERA", "SELECT: COPA A MEDIDA", "SELECT: EIGEN BEKER"};
static const char *const s_modCupKeys[MOD_LANGS] = {"D-PAD: TRACK   R: RANDOM", "CROIX: PISTE   R: HASARD", "KREUZ: STRECKE   R: ZUFALL",
                                                    "CROCE: PISTA   R: CASUALE", "CRUCETA: PISTA   R: AZAR", "PIJLEN: BAAN   R: WILLEKEURIG"};

#define MOD_CUP_TRACKS 18

static s8 s_cupPick[4][4];
static b32 s_cupReady;
static b32 s_cupEditing;
static s8 s_cupEdit;
static s8 s_cupRow;

static void NativeMods_CupApply(s32 cup)
{
	for (s32 i = 0; i < 4; i++)
	{
		const struct MainMenu_LevelRow *row = &D230.arcadeTracks[s_cupPick[cup][i]];

		data.ArcadeCups[cup].CupTrack[i].trackID = row->levID;
		data.ArcadeCups[cup].CupTrack[i].iconID = row->videoThumbnail;
	}
}

void NativeMods_CustomCupsFrame(struct RectMenu *menu)
{
	u32 tap = sdata->gGamepads->gamepad[0].buttonsTapped;

	if (!s_cupReady)
	{
		for (s32 cup = 0; cup < 4; cup++)
		{
			for (s32 i = 0; i < 4; i++)
			{
				s_cupPick[cup][i] = 0;
				for (s32 t = 0; t < MOD_CUP_TRACKS; t++)
				{
					if (D230.arcadeTracks[t].levID == data.ArcadeCups[cup].CupTrack[i].trackID)
					{
						s_cupPick[cup][i] = (s8)t;
						break;
					}
				}
			}
		}
		s_cupReady = true;
	}

	DecalFont_DrawLine((char *)s_modCupHint[NativeMods_Lang()], 0x100, 0x4, FONT_SMALL, (JUSTIFY_CENTER | ORANGE));

	if (D230.cupSelectTransition.state != IN_MENU)
	{
		return;
	}

	if (tap & BTN_SELECT)
	{
		s_cupEditing = !s_cupEditing;
		s_cupEdit = (s8)(menu->rowSelected & 3);
		s_cupRow = 0;
	}

	if (!s_cupEditing)
	{
		return;
	}

	menu->rowSelected = s_cupEdit;

	if (tap & BTN_UP)
	{
		s_cupRow = (s8)((s_cupRow + 3) & 3);
	}
	if (tap & BTN_DOWN)
	{
		s_cupRow = (s8)((s_cupRow + 1) & 3);
	}
	if (tap & BTN_LEFT)
	{
		s_cupPick[s_cupEdit][s_cupRow] = (s8)((s_cupPick[s_cupEdit][s_cupRow] + MOD_CUP_TRACKS - 1) % MOD_CUP_TRACKS);
	}
	if (tap & BTN_RIGHT)
	{
		s_cupPick[s_cupEdit][s_cupRow] = (s8)((s_cupPick[s_cupEdit][s_cupRow] + 1) % MOD_CUP_TRACKS);
	}
	if (tap & BTN_R1)
	{
		for (s32 i = 0; i < 4;)
		{
			MixRNG_Scramble();
			s32 id = (s32)((((u32)Timer_GetTime_Total() & 0xf) + ((u32)sdata->randomNumber >> 8)) % MOD_CUP_TRACKS);
			b32 taken = false;

			for (s32 j = 0; j < i; j++)
			{
				if (s_cupPick[s_cupEdit][j] == id)
				{
					taken = true;
				}
			}
			if (!taken)
			{
				s_cupPick[s_cupEdit][i++] = (s8)id;
			}
		}
	}

	NativeMods_CupApply(s_cupEdit);
	DecalFont_DrawLine((char *)s_modCupKeys[NativeMods_Lang()], 0x100, 0xcc, FONT_SMALL, (JUSTIFY_CENTER | ORANGE));
}

/* While a cup is being edited, its window lists the four track names instead of their icons. */
b32 NativeMods_CustomCupsHideIcons(s32 cup)
{
	return s_cupEditing && (cup == s_cupEdit);
}

void NativeMods_CustomCupsDraw(s32 cup, s32 x, s32 y)
{
	if (!NativeMods_CustomCupsHideIcons(cup))
	{
		return;
	}

	for (s32 i = 0; i < 4; i++)
	{
		s32 levID = D230.arcadeTracks[s_cupPick[cup][i]].levID;

		DecalFont_DrawLine(sdata->lngStrings[data.metaDataLEV[levID].name_LNG], x, y + i * 10, FONT_SMALL, (i == s_cupRow) ? WHITE : ORANGE);
	}
}

/* ------------------------------------------------------------------- Skins --- */

/* Per-character skins built by tools/make_skins.py from a character-pack disc into
 * <base dir>/skins/: skin_NN.ctr (the pack's high-LOD model) and skins.dat (names, HUD icon pixels
 * and CLUT, original and pack). SELECT in the arcade character select switches the highlighted
 * character; the choice is kept in skins.cfg. A skinned character races with the pack model (any
 * mode that loads drivers), shows the pack icon and name; the menu's own 3D models stay retail.
 * The pack models are untextured and the pack icons overwrite only the 16 icon rectangles, so
 * skinned and retail characters mix freely. */
#define MOD_SKIN_ICON_MAX (16 * 32)
#define MOD_SKIN_CLUT_MAX 256

struct NativeModSkin
{
	b32 hasModel;
	char longName[32];
	char shortName[16];
	RECT16 icon;
	RECT16 clut;
	u16 iconPix[2][MOD_SKIN_ICON_MAX]; /* [0] retail, [1] pack */
	u16 clutPix[2][MOD_SKIN_CLUT_MAX];
};

static struct NativeModSkin s_skin[MOD_CHARACTERS];
static b32 s_skinTried;
static b32 s_skinReady;
static char *s_skinRetailLong[MOD_CHARACTERS];
static char *s_skinRetailShort[MOD_CHARACTERS];

static s32 NativeMods_SkinPath(char *path, size_t size, const char *file)
{
	char rel[64];

	snprintf(rel, sizeof(rel), "skins/%s", file);
	return NativePath_Join(path, size, NativeStr8_FromCString(NativeAssets_GetBaseDir()), NativeStr8_FromCString(rel));
}

static void NativeMods_SkinSaveChoices(void)
{
	char path[1024];
	FILE *file;

	if (!NativeMods_SkinPath(path, sizeof(path), "skins.cfg") || ((file = fopen(path, "wb")) == NULL))
	{
		return;
	}
	for (s32 c = 0; c < MOD_CHARACTERS; c++)
	{
		fputc(s_skinOn[c] ? '1' : '0', file);
	}
	fputc('\n', file);
	fclose(file);
}

static void NativeMods_SkinInit(void)
{
	char path[1024];
	FILE *file;
	u32 header[2];

	if (s_skinTried)
	{
		return;
	}
	s_skinTried = true;

	if (!NativeMods_SkinPath(path, sizeof(path), "skins.dat") || ((file = fopen(path, "rb")) == NULL))
	{
		return;
	}

	if ((fread(header, sizeof(header), 1, file) == 1) && (header[0] == 0x314e4b53u /* "SKN1" */) && (header[1] == MOD_CHARACTERS))
	{
		b32 ok = true;

		for (s32 c = 0; ok && (c < MOD_CHARACTERS); c++)
		{
			struct NativeModSkin *s = &s_skin[c];
			u32 hasModel;
			u16 r[8];

			ok = (fread(&hasModel, 4, 1, file) == 1) && (fread(s->longName, 32, 1, file) == 1) && (fread(s->shortName, 16, 1, file) == 1) &&
			     (fread(r, sizeof(r), 1, file) == 1);
			if (!ok)
			{
				break;
			}
			s->longName[31] = 0;
			s->shortName[15] = 0;
			s->hasModel = hasModel != 0;
			s->icon = (RECT16){(s16)r[0], (s16)r[1], (s16)r[2], (s16)r[3]};
			s->clut = (RECT16){(s16)r[4], (s16)r[5], (s16)r[6], (s16)r[7]};

			u32 iconCount = (u32)r[2] * r[3];
			u32 clutCount = (u32)r[6] * r[7];
			ok = (iconCount <= MOD_SKIN_ICON_MAX) && (clutCount <= MOD_SKIN_CLUT_MAX) && (fread(s->iconPix[0], 2, iconCount, file) == iconCount) &&
			     (fread(s->iconPix[1], 2, iconCount, file) == iconCount) && (fread(s->clutPix[0], 2, clutCount, file) == clutCount) &&
			     (fread(s->clutPix[1], 2, clutCount, file) == clutCount);
		}
		s_skinReady = ok;
	}
	fclose(file);

	if (s_skinReady && NativeMods_SkinPath(path, sizeof(path), "skins.cfg") && ((file = fopen(path, "rb")) != NULL))
	{
		for (s32 c = 0; c < MOD_CHARACTERS; c++)
		{
			int ch = fgetc(file);
			s_skinOn[c] = (ch == '1') && s_skin[c].hasModel;
		}
		fclose(file);
	}
}

static void NativeMods_SkinApplyIcon(s32 c)
{
	struct NativeModSkin *s = &s_skin[c];
	s32 which = s_skinOn[c] ? 1 : 0;

	if (!s_skinReady || (s->icon.w == 0))
	{
		return;
	}
	LoadImage(&s->icon, s->iconPix[which]);
	if (s->clut.w != 0)
	{
		LoadImage(&s->clut, s->clutPix[which]);
	}
}

static void NativeMods_SkinApplyName(s32 c)
{
	struct MetaDataCHAR *mdc = &data.MetaDataCharacters[c];

	if (!s_skinReady || (sdata->lngStrings == NULL))
	{
		return;
	}
	if (s_skinRetailLong[c] == NULL)
	{
		return;
	}
	sdata->lngStrings[mdc->name_LNG_long] = s_skinOn[c] ? s_skin[c].longName : s_skinRetailLong[c];
	sdata->lngStrings[mdc->name_LNG_short] = s_skinOn[c] ? s_skin[c].shortName : s_skinRetailShort[c];
}

/* After every LOAD_LangFile: remember the retail names of this language, then put the skins' on. */
static void NativeMods_SkinInstallNames(void)
{
	NativeMods_SkinInit();
	if (!s_skinReady || (sdata->lngStrings == NULL))
	{
		return;
	}

	for (s32 c = 0; c < MOD_CHARACTERS; c++)
	{
		struct MetaDataCHAR *mdc = &data.MetaDataCharacters[c];
		char *curLong = sdata->lngStrings[mdc->name_LNG_long];
		char *curShort = sdata->lngStrings[mdc->name_LNG_short];

		if (curLong != s_skin[c].longName)
		{
			s_skinRetailLong[c] = curLong;
		}
		if (curShort != s_skin[c].shortName)
		{
			s_skinRetailShort[c] = curShort;
		}
		NativeMods_SkinApplyName(c);
	}
}

/* Load the pack model of every skinned character in this race into the main MEMPACK. Skipped
 * (retail model kept) when it would not fit, rather than stopping on the out-of-memory screen. */
static void NativeMods_LoadSkinModels(void)
{
	struct GameTracker *gGT = sdata->gGT;
	struct Mempack *saved;

	for (s32 c = 0; c < MOD_CHARACTERS; c++)
	{
		s_skinModel[c] = NULL;
	}

	NativeMods_SkinInit();
	if (!s_skinReady || (gGT->gameMode1 & (MAIN_MENU | GAME_CUTSCENE)))
	{
		return;
	}

	saved = sdata->PtrMempack;
	MEMPACK_SwapPacks(LOAD_MAIN_PACK_INDEX);

	for (s32 i = 0; i < 8; i++)
	{
		s32 c = data.characterIDs[i];
		char path[1024];
		char file[32];
		FILE *f;
		long size;

		if ((c < 0) || (c >= MOD_CHARACTERS) || !s_skinOn[c] || (s_skinModel[c] != NULL))
		{
			continue;
		}

		snprintf(file, sizeof(file), "skin_%02d.ctr", (int)c);
		if (!NativeMods_SkinPath(path, sizeof(path), file) || ((f = fopen(path, "rb")) == NULL))
		{
			continue;
		}

		if ((fseek(f, 0, SEEK_END) == 0) && ((size = ftell(f)) > 8) && (fseek(f, 0, SEEK_SET) == 0) && (MEMPACK_GetFreeBytes() > (int)size + 0x20000))
		{
			char *buf = MEMPACK_AllocMem((int)size);

			if (fread(buf, 1, (size_t)size, f) == (size_t)size)
			{
				int ptrMapOffset = *(int *)buf;
				char *real = buf + 4;

				if ((ptrMapOffset >= 0) && (ptrMapOffset + 8 <= size))
				{
					struct DramPointerMap *dpm = (struct DramPointerMap *)&real[ptrMapOffset];
					LOAD_RunPtrMap(real, (int *)DRAM_GETOFFSETS(dpm), dpm->numBytes >> 2);
				}
				s_skinModel[c] = (struct Model *)real;
			}
		}
		fclose(f);
	}

	sdata->PtrMempack = saved;

	/* modes that load the player (and ghost/boss) into driverModelExtras take the skin there too */
	for (s32 i = 0; i < LOAD_DRIVER_MODEL_EXTRA_COUNT; i++)
	{
		struct Model *m = data.driverModelExtras[i].model;

		if (m == NULL)
		{
			continue;
		}
		for (s32 c = 0; c < MOD_CHARACTERS; c++)
		{
			if ((s_skinModel[c] != NULL) && VehBirth_ModelNameEquals(s_skinModel[c], m->name))
			{
				data.driverModelExtras[i].model = s_skinModel[c];
				break;
			}
		}
	}
}

static struct Model *NativeMods_FindSkinModel(char *searchName)
{
	for (s32 c = 0; c < MOD_CHARACTERS; c++)
	{
		if ((s_skinModel[c] != NULL) && VehBirth_ModelNameEquals(s_skinModel[c], searchName))
		{
			return s_skinModel[c];
		}
	}
	return NULL;
}

static const char *const s_modSkinHint[MOD_LANGS] = {"SELECT: SKIN", "SELECT: APPARENCE", "SELECT: AUSSEHEN", "SELECT: ASPETTO", "SELECT: ASPECTO", "SELECT: UITERLIJK"};

/* Character select, player one: SELECT switches the highlighted character's skin. */
static void NativeMods_SkinCharacterMenu(void)
{
	s32 c = data.characterIDs[0];

	NativeMods_SkinInit();
	if (!s_skinReady || (c < 0) || (c >= MOD_CHARACTERS) || !s_skin[c].hasModel)
	{
		return;
	}

	DecalFont_DrawLine((char *)s_modSkinHint[NativeMods_Lang()], 0x100, 0xcc, FONT_SMALL, (JUSTIFY_CENTER | ORANGE));

	if (sdata->buttonTapPerPlayer[0] & BTN_SELECT)
	{
		s_skinOn[c] = !s_skinOn[c];
		NativeMods_SkinApplyIcon(c);
		NativeMods_SkinApplyName(c);
		NativeMods_SkinSaveChoices();
		OtherFX_Play(1, 1);
	}
}

/* -------------------------------------------------------------------- hooks --- */

void NativeMods_Frame(void)
{
	NativeMods_BlueFireFrame();
	NativeMods_SyncFlags();
#ifdef __3DS__
	g_n3dsMirrorTop = NativeMods_MirrorActive() ? 1 : 0;
#endif
}

void NativeMods_OnVramLoaded(void)
{
	/* a reload put the retail fire palette back */
	s_fireIsBlue = false;
	NativeMods_SyncFlags();

	if (g_modKartTextures)
	{
		NativeMods_LoadKartTextures();
	}

	/* skins: the shared VRAM upload put the retail icons back */
	NativeMods_SkinInit();
	for (s32 c = 0; c < 16; c++)
	{
		if (s_skinOn[c])
		{
			NativeMods_SkinApplyIcon(c);
		}
	}
	if (g_modPaintJobs)
	{
		NativeMods_ReapplyPaints();
	}
}

#endif /* CTR_NATIVE */
