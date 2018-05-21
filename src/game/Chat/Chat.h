#ifndef MANGOSSERVER_CHAT_H
#define MANGOSSERVER_CHAT_H

#include "Globals/SharedDefines.h"

#define MSG_COLOR_LIGHTRED	  "|cffff6060"
#define MSG_COLOR_LIGHTBLUE	 "|cff00ccff"
#define MSG_COLOR_BLUE		  "|cff0000ff"
#define MSG_COLOR_GREEN		 "|cff00ff00"
#define MSG_COLOR_RED		   "|cffff0000"
#define MSG_COLOR_GOLD		  "|cffffcc00"
#define MSG_COLOR_GREY		  "|cff888888"
#define MSG_COLOR_WHITE		 "|cffffffff"
#define MSG_COLOR_SUBWHITE	  "|cffbbbbbb"
#define MSG_COLOR_MAGENTA	   "|cffff00ff"
#define MSG_COLOR_YELLOW		"|cffffff00"
#define MSG_COLOR_CYAN		  "|cff00ffff"


class ChatCommand
{
public:
	const char *       Name;
	uint32             SecurityLevel;                   // function pointer required correct align (use uint32)
	bool (ChatHandler::*Handler)(const char* args);
	std::string        Help;
	ChatCommand *      ChildCommands;
	uint32			 NormalValueField;
	uint32			 MaxValueField;
	uint16			 ValueType;	// 0 = nothing, 1 = uint, 2 = float
};

class ChatHandler
{
public:
	explicit ChatHandler(WorldSession* session) : m_session(session) {}
	explicit ChatHandler(Player* player) : m_session(player->GetSession()) {}
	~ChatHandler() {}

	WorldPacket* FillMessageData(uint32 type, uint32 language, const char* message, uint64 guid, uint8 flag = 0) const;
	WorldPacket* FillSystemMessageData(const char* message) const;

	int ParseCommands(const char* text);

	void SystemMessage(const char *message, ...);
	void ColorSystemMessage(const char *colorcode, const char *message, ...);
	void RedSystemMessage(const char *message, ...);
	void GreenSystemMessage(const char *message, ...);
	void BlueSystemMessage(const char *message, ...);

protected:
	bool hasStringAbbr(const char* name, const char* part);
	void SendMultilineMessage(const char *str);

	bool ExecuteCommandInTable(ChatCommand *table, const char* text, std::string fullcommand);
	bool ShowHelpForCommand(ChatCommand *table, const char* cmd);

	ChatCommand* getCommandTable();

	// Level 0 commands
	bool HandleHelpCommand(const char* args);
	bool HandleCommandsCommand(const char* args);
	bool HandleNYICommand(const char* args);
	bool HandleAcctCommand(const char* args);
	bool HandleStartCommand(const char* args);
	bool HandleInfoCommand(const char* args);
	bool HandleDismountCommand(const char* args);
	bool HandleSaveCommand(const char* args);
	bool HandleGMListCommand(const char* args);

	// Level 1 commands
	bool CmdSetValueField(WorldSession *m_session, uint32 field, uint32 fieldmax, const char *fieldname, const char* args);
	bool CmdSetFloatField(WorldSession *m_session, uint32 field, uint32 fieldmax, const char *fieldname, const char* args);
	bool HandleSummonCommand(const char* args);
	bool HandleAppearCommand(const char* args);
	bool HandleAnnounceCommand(const char* args);
	bool HandleWAnnounceCommand(const char* args);
	bool HandleGMOnCommand(const char* args);
	bool HandleGMOffCommand(const char* args);
	bool HandleGPSCommand(const char* args);
	bool HandleKickCommand(const char* args);
	bool HandleTaxiCheatCommand(const char* args);
	bool HandleModifySpeedCommand(const char* args);

	// Debug Commands
	bool HandleDebugInFrontCommand(const char* args);
	bool HandleShowReactionCommand(const char* args);
	bool HandleAIMoveCommand(const char* args);
	bool HandleMoveInfoCommand(const char* args);
	bool HandleDistanceCommand(const char* args);
	bool HandleFaceCommand(const char* args);
	bool HandleSetBytesCommand(const char* args);
	bool HandleGetBytesCommand(const char* args);
	bool HandleDebugLandWalk(const char* args);
	bool HandleDebugUnroot(const char* args);
	bool HandleDebugRoot(const char* args);
	bool HandleDebugWaterWalk(const char* args);
	bool HandleAggroRangeCommand(const char* args);
	bool HandleKnockBackCommand(const char* args);
	bool HandleFadeCommand(const char* args);
	bool HandleThreatModCommand(const char* args);
	bool HandleCalcThreatCommand(const char* args);
	bool HandleThreatListCommand(const char* args);
	bool HandleNpcSpawnLinkCommand(const char* args);
	bool HandleGoStateLinkCommand(const char* args);
	bool HandleSilentPlayerCommand(const char* args);
	bool HandleDebugDumpCoordsCommmand(const char * args);
	bool HandleSendRunSpeedChange(const char * args);

	//WayPoint Commands
	bool HandleWPAddCommand(const char* args);
	bool HandleWPShowCommand(const char* args);
	bool HandleWPHideCommand(const char* args);
	bool HandleWPDeleteCommand(const char* args);
	bool HandleWPFlagsCommand(const char* args);
	bool HandleWPMoveHereCommand(const char* args);
	bool HandleWPWaitCommand(const char* args);
	bool HandleWPEmoteCommand(const char* args);
	bool HandleWPSkinCommand(const char* args);
	bool HandleWPChangeNoCommand(const char* args);
	bool HandleWPInfoCommand(const char* args);
	bool HandleWPMoveTypeCommand(const char* args);
	bool HandleSaveWaypoints(const char* args);
	bool HandleGenerateWaypoints(const char* args);
	bool HandleDeleteWaypoints(const char* args);

	// Level 2 commands
	bool HandleGUIDCommand(const char* args);
	bool HandleNameCommand(const char* args);
	bool HandleSubNameCommand(const char* args);
	bool HandleDeleteCommand(const char* args);
	bool HandleDeMorphCommand(const char* args);
	bool HandleItemCommand(const char* args);
	bool HandleItemRemoveCommand(const char* args);
	bool HandleRunCommand(const char* args);
	bool HandleNPCFlagCommand(const char* args);
	bool HandleSaveAllCommand(const char* args);
	bool HandleRegenerateGossipCommand(const char* args);
	bool CreateGuildCommand(const char* args);
	bool HandleStartBGCommand(const char* args);
	bool HandlePauseBGCommand(const char* args);
	bool HandleResetScoreCommand(const char* args);
	bool HandleBGInfoCommnad(const char *args);
	bool HandleInvincibleCommand(const char *args);
	bool HandleInvisibleCommand(const char *args);
	bool HandleKillCommand(const char *args);
	bool HandleAddSpiritCommand(const char *args);
	bool HandleNPCFactionCommand(const char *args);
	bool HandleCreatureSpawnCommand(const char *args);
	bool HandleGOSelect(const char *args);
	bool HandleGODelete(const char *args);
	bool HandleGOSpawn(const char *args);
	bool HandleGOInfo(const char *args);
	bool HandleGOEnable(const char *args);
	bool HandleGOActivate(const char* args);
	bool HandleGORotate(const char * args);
	bool HandleGOMove(const char * args);
	bool HandleAddAIAgentCommand(const char* args);
	bool HandleDelAIAgentCommand(const char* args);
	bool HandleListAIAgentCommand(const char* args);

	// Level 3 commands
	bool HandleMassSummonCommand(const char* args);
	bool HandleSecurityCommand(const char* args);
	bool HandleWorldPortCommand(const char* args);
	bool HandleAddWeaponCommand(const char* args);
	bool HandleAllowMovementCommand(const char* args);
	bool HandleMoveCommand(const char* args);
	bool HandleLearnCommand(const char* args);
	bool HandleReviveCommand(const char* args);
	bool HandleMorphCommand(const char* args);
	bool HandleAddGraveCommand(const char* args);
	bool HandleAddSHCommand(const char* args);
	bool HandleExploreCheatCommand(const char* args);
	bool HandleLevelUpCommand(const char* args);
	bool HandleGMTicketGetAllCommand(const char* args);
	bool HandleGMTicketGetByIdCommand(const char* args);
	bool HandleGMTicketDelByIdCommand(const char* args);
	bool HandleAddSkillCommand(const char* args);
	bool HandleAddInvItemCommand(const char* args);
	bool HandleWeatherCommand(const char* args);
	bool HandleGetRankCommand(const char* args);
	bool HandleSetRankCommand(const char* args);
	bool HandleResetReputationCommand(const char* args);
	bool HandleLearnSkillCommand(const char* args);
	bool HandleModifySkillCommand(const char* args);
	bool HandleRemoveSkillCommand(const char* args);
	bool HandleNpcInfoCommand(const char* args);
	bool HandleEmoteCommand(const char* args);
	bool HandleIncreaseWeaponSkill(const char* args);
	bool HandleCastSpellCommand(const char* args);
	bool HandleCastSpellNECommand(const char* args);
	bool HandleCellDeleteCommand(const char* args);
	bool HandleAddRestXPCommand(const char* args);
	bool HandleModifyGoldCommand(const char* args);
	bool HandleGenerateNameCommand(const char* args);
	bool HandleMonsterSayCommand(const char* args);
	bool HandleMonsterYellCommand(const char* args);
	bool HandleNpcComeCommand(const char* args);
	bool HandleBattlegroundCommand(const char* args);
	bool HandleSetWorldStateCommand(const char* args);
	bool HandlePlaySoundCommand(const char* args);
	bool HandleSetBattlefieldStatusCommand(const char* args);
	bool HandleAttackerInfoCommand(const char* args);
	bool HandleShowAttackersCommand(const char* args);
	bool HandleNpcReturnCommand(const char* args);
	bool HandleCreateAccountCommand(const char* args);
	bool HandleSetRateCommand(const char* args);
	bool HandleGetRateCommand(const char* args);
	bool HandleResetLevelCommand(const char* args);
	bool HandleResetTalentsCommand(const char* args);
	bool HandleResetSpellsCommand(const char* args);
	bool HandleNpcFollowCommand(const char* args);
	bool HandleFormationLink1Command(const char* args);
	bool HandleFormationLink2Command(const char* args);
	bool HandleNullFollowCommand(const char* args);
	bool HandleFormationClearCommand(const char* args);
	bool HandleResetSkillsCommand(const char* args);
	bool HandlePlayerInfo(const char* args);

	//Ban
	bool HandleBanCharacterCommand(const char* args);
	bool HandleUnBanCharacterCommand(const char* args);

	//BG
	bool HandleSetBGScoreCommand(const char* args);

	bool HandleGOScale(const char* args);
	bool HandleUptimeCommand(const char* args);
	bool HandleReviveStringcommand(const char* args);
	bool HandleMountCommand(const char* args);
	bool HandleGetPosCommand(const char* args);
	bool HandleGetTransporterTime(const char* args);
	bool HandleSendItemPushResult(const char* args);
	bool HandleGOAnimProgress(const char * args);
	bool HandleGOExport(const char * args);
	bool HandleNpcExport(const char * args);
	bool HandleRemoveAurasCommand(const char *args);
	bool HandleParalyzeCommand(const char* args);
	bool HandleUnParalyzeCommand(const char* args);
	bool HandleSetMotdCommand(const char* args);
	bool HandleAddItemSetCommand(const char* args);
	bool HandleTriggerCommand(const char* args);
	bool HandleModifyValueCommand(const char* args);
	bool HandleModifyBitCommand(const char* args);
	bool HandleGoInstanceCommand(const char* args);
	bool HandleCreateInstanceCommand(const char* args);
	bool HandleBattlegroundExitCommand(const char* args);
	bool HandleExitInstanceCommand(const char* args);
	bool HandleCooldownCheatCommand(const char* args);
	bool HandleCastTimeCheatCommand(const char* args);
	bool HandlePowerCheatCommand(const char* args);
	bool HandleGodModeCommand(const char* args);
	bool HandleShowCheatsCommand(const char* args);
	bool HandleFlySpeedCheatCommand(const char* args);
	bool HandleStackCheatCommand(const char* args);
	bool HandleFlyCommand(const char* args);
	bool HandleLandCommand(const char* args);

	bool HandleDBReloadCommand(const char* args);
	bool HandleResetHPCommand(const char* args);

	// honor
	bool HandleAddHonorCommand(const char* args);
	bool HandleAddKillCommand(const char* args);
	bool HandleGlobalHonorDailyMaintenanceCommand(const char* args);
	bool HandleNextDayCommand(const char* args);
	bool HandlePVPCreditCommand(const char* args);

	bool HandleSpawnSpiritGuideCommand(const char* args);
	bool HandleUnlearnCommand(const char* args);
	bool HandleModifyLevelCommand(const char* args);

	// pet
	bool HandleCreatePetCommand(const char* args);
	bool HandleAddPetSpellCommand(const char* args);
	bool HandleRemovePetSpellCommand(const char* args);
	bool HandleEnableRenameCommand(const char* args);
	bool HandleRenamePetCommand(const char* args);

	bool HandleShutdownCommand(const char* args);
	bool HandleShutdownRestartCommand(const char* args);

	bool HandleAllowWhispersCommand(const char* args);
	bool HandleBlockWhispersCommand(const char* args);

	bool HandleAdvanceAllSkillsCommand(const char* args);
	bool HandleKillBySessionCommand(const char* args);
	bool HandleKillByPlayerCommand(const char* args);

	bool HandleUnlockMovementCommand(const char* args);
	bool HandleCastAllCommand(const char* args);

	//Recall Commands
	bool HandleRecallListCommand(const char* args);
	bool HandleRecallGoCommand(const char* args);
	bool HandleRecallAddCommand(const char* args);
	bool HandleRecallDelCommand(const char* args);
	bool HandleModPeriodCommand(const char* args);
	bool HandleGlobalPlaySoundCommand(const char* args);
	bool HandleRecallPortPlayerCommand(const char* args);

	bool HandleBanAccountCommand(const char * args);
	bool HandleIPBanCommand(const char * args);

	bool HandleRemoveItemCommand(const char * args);
	bool HandleRenameCommand(const char * args);
	bool HandleForceRenameCommand(const char * args);
	bool HandleGetStandingCommand(const char * args);
	bool HandleSetStandingCommand(const char * args);
	bool HandleGetBaseStandingCommand(const char * args);

	bool HandleReloadAccountsCommand(const char * args);
	bool HandleLookupItemCommand(const char * args);
	bool HandleLookupCreatureCommand(const char * args);
	bool HandleLookupObjectCommand(const char * args);
	bool HandleLookupSpellCommand(const char * args);

	bool HandleReloadScriptsCommand(const char * args);
	bool HandleNpcPossessCommand(const char * args);
	bool HandleNpcUnPossessCommand(const char * args);
	bool HandleChangePasswordCommand(const char * args);
	bool HandleRehashCommand(const char * args);

	/** AI AGENT DEBUG COMMANDS */
	bool HandleAIAgentDebugBegin(const char * args);
	bool HandleAIAgentDebugContinue(const char * args);
	bool HandleAIAgentDebugSkip(const char * args);

	Player*   getSelectedPlayer() const;
	Creature* getSelectedCreature() const;
	Unit*     getSelectedUnit() const;

	WorldSession * m_session;

	private:
		static bool load_command_table;
};

#endif

char const *fmtstring(char const *format, ...);