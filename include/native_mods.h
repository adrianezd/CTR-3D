#ifndef NATIVE_MODS_H
#define NATIVE_MODS_H

/*
 * Community mods from CTR-ModSDK (github.com/CTR-tools/CTR-ModSDK, GPL-3.0), ported into the native
 * build. Retail CTR-ModSDK mods are PS1 code patched into fixed addresses; here each one is a few
 * calls from the matching game function into platform/native_mods.c.
 *
 *   EngineSelect          (Faradise, Superstarxalien)  pick the engine class after the character
 *   ExtraDifficulty                                     SUPER HARD and ULTRA HARD arcade difficulties
 *   CharacterRandomizer   (Niko)                        random CPU characters in Arcade
 *   BlueFire                                            blue exhaust fire at the ultimate-speed cap
 *   CustomFlag                                          blue checkered flag
 *   PaintJobs             (Superstarxalien)             kart colours, L/R in character select
 *   NewKartTextures       (Superstarxalien)             gold and silver kart pipe framing
 *
 * The toggles are plain ints, ON by default, flipped from the 3DS OPTIONS menu.
 */

#ifdef CTR_NATIVE

extern int g_modEngineSelect;
extern int g_modRandomRivals;
extern int g_modBlueFire;
extern int g_modBlueFlag;
extern int g_modPaintJobs;
extern int g_modKartTextures;
extern int g_modAdvRandom;
extern int g_modAdvDifficulty;
extern int g_modMirror;
extern int g_modMoonGravity;
extern int g_modItemChaos;
extern int g_modIcy;

/* language index of the OPTIONS table (0 English ... 4 Spanish, 5 Dutch); 0 on PC */
s32 NativeOptions_LanguageIndex(void);

/* strings for repointed LNG slots; called after every LOAD_LangFile */
void NativeMods_InstallStrings(void);

/* per-frame hook (BOTS_UpdateGlobals) and end of every VRAM file upload (LOAD_VramFileCallback) */
void NativeMods_Frame(void);
void NativeMods_OnVramLoaded(void);

/* EngineSelect: game/230/MM_Characters.c and game/Vehicle/VehBirth.c */
void NativeMods_CharacterSelectEnter(void);
b32 NativeMods_EngineSelectActive(void);
void NativeMods_EngineSelectBegin(s32 player, s32 characterID);
b32 NativeMods_EngineSelectInput(s32 player, s32 characterID, u32 button, s32 numPlayers, u32 selectFlags);
b32 NativeMods_EngineSelectBack(s32 player);
b32 NativeMods_EngineSelectDrawName(s32 player, s32 characterID, s32 x, s32 y, s32 fontType, s32 numPlayers);
b32 NativeMods_WantsUnlimitedClass(s32 characterID);
void NativeMods_CharacterMenuFrame(void);

/* ExtraDifficulty: game/230/MM_MenuFlow.c */
void NativeMods_SyncDifficultyRows(void);
b32 NativeMods_DifficultyChoice(s16 row);

/* CharacterRandomizer: game/LOAD/LOAD_Assets.c, LOAD_TenStages.c, LOAD_ModelPtrs.c, Vehicle/VehBirth.c */
b32 NativeMods_RandomizeRivals(s32 playerCharacterID);
b32 NativeMods_QueueRivalModels(struct BigHeader *bigfile, void (*callback)(struct LoadQueueSlot *));
void NativeMods_ClearRivalModels(void);
void NativeMods_ResolveRivalModels(void);
void NativeMods_StoreRivalModelPtrs(struct GameTracker *gGT);
struct Model *NativeMods_FindRivalModel(char *searchName);
void NativeMods_PoolArenaBegin(void);
void NativeMods_PoolArenaEnd(void);
void *NativeMods_PoolAlloc(int size);

/* CustomFlag: game/RaceFlag.c (3 = blue checkers) */
int NativeMods_FlagStyle(int style);

/* EngineSelect in the adventure garage: game/233/CS_Garage.c */
b32 NativeMods_GarageEngineInput(s32 characterID, u32 tap);
char *NativeMods_EngineName(s32 characterID);

/* AdvDifficulty: game/BOTS.c; AdvRandomizer: game/232/AH_WarpPad.c, AH_Garage.c */
s32 NativeMods_AdvDifficulty(s32 computed, b32 boss);
s32 NativeMods_AdvRandomPad(s32 levelID);
s32 NativeMods_AdvRandomHub(s32 hubID);

/* CTR Unlimited-style modes: game/GAMEPAD.c, Vehicle/VehBirth.c, Vehicle/VehPhysGeneral.c */
b32 NativeMods_MirrorActive(void);
u32 NativeMods_MirrorButtons(u32 buttons);
void NativeMods_DriverConsts(struct Driver *driver);
void NativeMods_ChaosItem(struct Driver *driver);

/* CustomCups: game/230/MM_CupSelect.c */
void NativeMods_CustomCupsFrame(struct RectMenu *menu);
b32 NativeMods_CustomCupsHideIcons(s32 cup);
void NativeMods_CustomCupsDraw(s32 cup, s32 x, s32 y);

#endif /* CTR_NATIVE */

#endif /* NATIVE_MODS_H */
