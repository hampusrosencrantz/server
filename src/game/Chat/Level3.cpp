#include "World/World.h"
#include "Spells/SpellMgr.h"
#include "Chat/Chat.h"
#include "Accounts\AccountMgr.h"

bool ChatHandler::HandleWeatherCommand(const char* args)
{
}

bool ChatHandler::HandleSecurityCommand(const char* args)
{
	char* arg1 = strtok((char*)args, " ");
	if (!arg1)
		return false;

	char* arg2 = strtok(NULL, " ");
	if (!arg2)
		return false;

	int32 gm = (int32)atoi(arg2);

	Player *target = sObjectMgr.GetPlayer((const char*)arg1);
	if (target)
	{
		SystemMessage("You change security string of %s to [%i].", target->GetName(), gm);
		ChatHandler(target).SystemMessage("%s changed your security string to [%i].", m_session->GetPlayer()->GetName(), gm);
		LoginDatabase.PExecute("UPDATE account SET gmlevel = '%i' WHERE id = '%u'", gm, target->GetSession()->GetAccountId());
	}
	else
		SystemMessage("Player (%s) does not exist or is not logged in.", arg1);

	return true;
}

bool ChatHandler::HandleWorldPortCommand(const char* args)
{
	if (!*args)
		return false;

	Player* _player = m_session->GetPlayer();

	char* pmapid = strtok((char*)args, " ");
	char* px = strtok(NULL, " ");
	char* pz = strtok(NULL, " ");
	char* py = strtok(NULL, " ");

	if (!px || !py || !pz)
		return false;

	float x = (float)atof(px);
	float y = (float)atof(py);
	float z = (float)atof(pz);
	uint32 mapid;
	if (pmapid)
		mapid = (uint32)atoi(pmapid);
	else
		mapid = _player->GetMapId();

	if (!MapManager::IsValidMapCoord(mapid, x, y, z))
		return false;

	_player->TeleportTo(mapid, x, y, z, _player->GetOrientation());

	return true;
}

bool ChatHandler::HandleAllowMovementCommand(const char* args)
{
	if (sWorld.getAllowMovement())
	{
		sWorld.SetAllowMovement(false);
		SystemMessage("Creature Movement Disabled.");
	}
	else
	{
		sWorld.SetAllowMovement(true);
		SystemMessage("Creature Movement Enabled.");
	}
	return true;
}

bool ChatHandler::HandleNPCFactionCommand(const char* args)
{
	if (!*args)
		return false;

	uint32 npcFaction = (uint32)atoi((char*)args);

	Unit* unit = getSelectedUnit();

	if (!unit)
	{
		SystemMessage("You should select a creature.");
		return true;
	}

	Creature *pCreature = m_session->GetPlayer()->GetMap()->GetCreature(unit->GetObjectGuid());
	if (!pCreature)
	{
		SystemMessage("You should select a creature.");
		return true;
	}

	pCreature->setFaction(npcFaction);

	if (CreatureInfo const *cinfo = pCreature->GetCreatureInfo())
	{
		const_cast<CreatureInfo*>(cinfo)->FactionAlliance = npcFaction;
		const_cast<CreatureInfo*>(cinfo)->FactionHorde = npcFaction;
	}

	WorldDatabase.PExecuteLog("UPDATE creature_template SET FactionAlliance = '%u', FactionHorde = '%u' WHERE entry = '%u'", npcFaction, npcFaction, pCreature->GetEntry());

	return true;
}

bool ChatHandler::HandleLearnCommand(const char* args)
{
	if (!*args)
		return false;

	Player* player = m_session->GetPlayer();

	if (strlen(args) >= 3 && !strnicmp(args, "all", 3))
	{
		ChrClassesEntry const* clsEntry = sChrClassesStore.LookupEntry(player->getClass());
		if (!clsEntry)
			return true;
		uint32 family = clsEntry->spellfamily;

		for (uint32 i = 0; i < sSkillLineAbilityStore.GetNumRows(); ++i)
		{
			SkillLineAbilityEntry const* entry = sSkillLineAbilityStore.LookupEntry(i);
			if (!entry)
				continue;

			SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(entry->spellId);
			if (!spellInfo)
				continue;

			// skip server-side/triggered spells
			if (spellInfo->spellLevel == 0)
				continue;

			// skip wrong class/race skills
			if (!player->IsSpellFitByClassAndRace(spellInfo->Id))
				continue;

			// skip other spell families
			if (spellInfo->SpellFamilyName != family)
				continue;

			// skip spells with first rank learned as talent (and all talents then also)
			uint32 first_rank = sSpellMgr.GetFirstSpellInChain(spellInfo->Id);
			if (GetTalentSpellCost(first_rank) > 0)
				continue;

			// skip broken spells
			if (!SpellMgr::IsSpellValid(spellInfo, player, false))
				continue;

			player->learnSpell(spellInfo->Id, false);
		}
		return true;
	}

	uint32 spell = atol((char*)args);

	if (player->HasSpell(spell))
	{
		SystemMessage("%s already knows that spell.", player->GetName());
		return true;
	}

	player->learnSpell(spell, false);
	ChatHandler(player).BlueSystemMessage("%s taught you Spell %d", m_session->GetPlayer()->GetName(), spell);

	return true;
}

bool ChatHandler::HandleAddWeaponCommand(const char* args)
{
	if (!*args)
		return false;

	Unit *unit = getSelectedUnit();
	if (!unit)
	{
		SystemMessage("No selection.");
		return true;
	}

	Creature *pCreature = m_session->GetPlayer()->GetMap()->GetCreature(unit->GetObjectGuid());

	if (!pCreature)
	{
		SystemMessage("You should select a creature.");
		return true;
	}

	char* pSlotID = strtok((char*)args, " ");
	if (!pSlotID)
		return false;

	char* pItemID = strtok(NULL, " ");
	if (!pItemID)
		return false;

	uint32 ItemID = atoi(pItemID);
	uint32 SlotID = atoi(pSlotID);

	const ItemPrototype* tmpItem = sObjectMgr.GetItemPrototype(ItemID);

	bool added = false;
	std::stringstream sstext;
	if (tmpItem)
	{
		switch (SlotID)
		{
		case 1:
			pCreature->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, ItemID);
			added = true;
			break;
		case 2:
			pCreature->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, ItemID);
			added = true;
			break;
		case 3:
			pCreature->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, ItemID);
			added = true;
			break;
		default:
			sstext << "Item Slot '" << SlotID << "' doesn't exist." << '\0';
			added = false;
			break;
		}
		if (added)
		{
			sstext << "Item '" << ItemID << "' '" << tmpItem->Name1 << "' Added to Slot " << SlotID << '\0';
		}
	}
	else
		sstext << "Item '" << ItemID << "' Not Found in Database." << '\0';
	SystemMessage(sstext.str().c_str());
	return true;
}

bool ChatHandler::HandleReviveCommand(const char* args)
{
	Player *SelectedPlayer = getSelectedPlayer();

	if (!SelectedPlayer)
		return true;

	SelectedPlayer->ResurrectPlayer(0.5f);
	SelectedPlayer->SpawnCorpseBones();
	SelectedPlayer->SaveToDB();
	return true;
}

bool ChatHandler::HandleMorphCommand(const char* args)
{
	if (!*args)
		return false;

	uint16 display_id = (uint16)atoi((char*)args);

	Unit *target = getSelectedUnit();
	if (!target)
		target = m_session->GetPlayer();

	target->SetDisplayId(display_id);

	return true;
}

bool ChatHandler::HandleExploreCheatCommand(const char* args)
{
	if (!*args)
		return false;

	int flag = atoi((char*)args);

	Player *chr = getSelectedPlayer();
	if (chr == NULL)
	{
		SystemMessage("No character selected.");
		return true;
	}

	if (flag != 0)
	{
		SystemMessage("%s has explored all zones now.", chr->GetName());
		ChatHandler(chr).SystemMessage("%s has explored all zones for you.",
			m_session->GetPlayer()->GetName());
	}
	else
	{
		SystemMessage("%s has no more explored zones.", chr->GetName());
		ChatHandler(chr).SystemMessage("%s has hidden all zones from you.",
			m_session->GetPlayer()->GetName());
	}

	for (uint8 i = 0; i<128; i++)
	{
		if (flag != 0)
		{
			m_session->GetPlayer()->SetFlag(PLAYER_EXPLORED_ZONES_1 + i, 0xFFFFFFFF);
		}
		else
		{
			m_session->GetPlayer()->SetFlag(PLAYER_EXPLORED_ZONES_1 + i, 0);
		}
	}

	return true;
}

bool ChatHandler::HandleLevelUpCommand(const char* args)
{
	int levels = 0;

	if (!*args)
		levels = 1;
	else
		levels = atoi(args);

	if (levels <= 0)
		return false;

	Player *plr = getSelectedPlayer();
	if (!plr)
		return false;

	uint32 startlvl = plr->getLevel();
	for (uint32 i = startlvl; i < (startlvl + levels); i++)
	{
		uint32 curXP = plr->GetUInt32Value(PLAYER_XP);
		uint32 nextLvlXP = plr->GetUInt32Value(PLAYER_NEXT_LEVEL_XP);
		uint32 givexp = nextLvlXP - curXP;

		plr->GiveXP(givexp, static_cast<Unit*>(plr));
		if (plr->getLevel() >= 255) break;
	}

	SystemMessage("You have been leveled up to Level %i", startlvl + levels);
}

bool ChatHandler::HandleBanCharacterCommand(const char* args)
{
	if (!*args)
		return false;

	char Character[255];
	char Reason[1024];
	bool HasReason = true;

	int Args = sscanf(args, "%s %s", Character, Reason);
	if (Args == 1)
		HasReason = false;
	else if (Args == 0)
	{
		RedSystemMessage("A character name and reason is required.");
		return true;
	}

	Player *player = sObjectMgr.GetPlayer(Character);
	if (player)
	{
		GreenSystemMessage("Banned player %s ingame.", player->GetName());
		if (HasReason)
			player->SetBanned(Reason);
		else
			player->SetBanned();
	}
	else
		GreenSystemMessage("Player %s not found ingame.", Character);

	CharacterDatabase.PExecute("UPDATE characters SET banned = 4 WHERE name = '%s'", Character);
	if (HasReason)
		CharacterDatabase.PExecute("UPDATE characters SET banReason = \"%s\" WHERE name = '%s'", Character);

	SystemMessage("Banned character %s in database.", Character);
	return true;
}

bool ChatHandler::HandleUnBanCharacterCommand(const char* args)
{
	if (!*args)
		return false;

	char Character[255];
	if (sscanf(args, "%s", Character) == 0)
	{
		RedSystemMessage("A character name and reason is required.");
		return true;
	}

	Player *player = sObjectMgr.GetPlayer(Character);
	if (!player)
	{
		GreenSystemMessage("Unbanned player %s ingame.", player->GetName());
		player->UnSetBanned();
	}
	else
		GreenSystemMessage("Player %s not found ingame.", Character);

	CharacterDatabase.PExecute("UPDATE characters SET banned = 0 WHERE name = '%s'", Character);

	SystemMessage("Unbanned character %s in database.", Character);

	return true;
}

bool ChatHandler::HandleNpcInfoCommand(const char *args)
{
	Creature* target = getSelectedCreature();

	if (!target)
		return false;

	BlueSystemMessage("Showing creature info for %s", target->GetName());
	SystemMessage("GUID: %d\nFaction: %d\nNPCFlags: %d\nDisplayID: %d", target->GetGUIDLow(), target->getFaction(), target->GetUInt32Value(UNIT_NPC_FLAGS), target->GetDisplayId());
	GreenSystemMessage("Combat Support: 0x%.3X", target->getFactionTemplateEntry()->friendGroupMask);
	GreenSystemMessage("Base Health: %d", target->GetHealth());
	GreenSystemMessage("Base Armor: %d", target->GetArmor());
	GreenSystemMessage("Base Mana: %d", target->GetUInt32Value(UNIT_FIELD_MAXPOWER1));
	GreenSystemMessage("Base Holy: %d", target->GetResistance(SPELL_SCHOOL_HOLY));
	GreenSystemMessage("Base Fire: %d", target->GetResistance(SPELL_SCHOOL_FIRE));
	GreenSystemMessage("Base Nature: %d", target->GetResistance(SPELL_SCHOOL_NATURE));
	GreenSystemMessage("Base Frost: %d", target->GetResistance(SPELL_SCHOOL_FROST));
	GreenSystemMessage("Base Shadow: %d", target->GetResistance(SPELL_SCHOOL_SHADOW));
	GreenSystemMessage("Base Arcane: %d", target->GetResistance(SPELL_SCHOOL_ARCANE));
	GreenSystemMessage("Damage min/max: %f/%f", target->GetFloatValue(UNIT_FIELD_MINDAMAGE), target->GetFloatValue(UNIT_FIELD_MAXDAMAGE));

	ColorSystemMessage(MSG_COLOR_RED, "Entry ID: %d", target->GetEntry());
	ColorSystemMessage(MSG_COLOR_RED, "SQL Entry ID: %d", target->GetCreatureInfo()->Entry);
	// show byte
	std::stringstream sstext;
	uint32 theBytes = target->GetUInt32Value(UNIT_FIELD_BYTES_0);
	sstext << "UNIT_FIELD_BYTES_0 are " << uint16((uint8)theBytes & 0xFF) << " " << uint16((uint8)(theBytes >> 8) & 0xFF) << " ";
	sstext << uint16((uint8)(theBytes >> 16) & 0xFF) << " " << uint16((uint8)(theBytes >> 24) & 0xFF) << '\0';
	BlueSystemMessage(sstext.str().c_str());
	return true;
}

bool ChatHandler::HandleIncreaseWeaponSkill(const char *args)
{
	char *pMin = strtok((char*)args, " ");
	uint32 cnt = 0;
	if (!pMin)
		cnt = 1;
	else
		cnt = atol(pMin);

	Player * target = getSelectedPlayer();
	if (!target)
		return false;

	uint32 SubClassSkill = 0;
	Item *item = target->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
	const ItemPrototype *proto = NULL;
	if (!item)
		item = target->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED);
	if (item)
		proto = item->GetProto();
	if (proto)
	{
		switch (proto->SubClass)
		{
			// Weapons
		case 0:	// 1 handed axes
			SubClassSkill = SKILL_AXES;
			break;
		case 1:	// 2 handed axes
			SubClassSkill = SKILL_2H_AXES;
			break;
		case 2:	// bows
			SubClassSkill = SKILL_BOWS;
			break;
		case 3:	// guns
			SubClassSkill = SKILL_GUNS;
			break;
		case 4:	// 1 handed mace
			SubClassSkill = SKILL_MACES;
			break;
		case 5:	// 2 handed mace
			SubClassSkill = SKILL_2H_MACES;
			break;
		case 6:	// polearms
			SubClassSkill = SKILL_POLEARMS;
			break;
		case 7: // 1 handed sword
			SubClassSkill = SKILL_SWORDS;
			break;
		case 8: // 2 handed sword
			SubClassSkill = SKILL_2H_SWORDS;
			break;
		case 9: // obsolete
			SubClassSkill = 136;
			break;
		case 10: //1 handed exotic
			SubClassSkill = 136;
			break;
		case 11: // 2 handed exotic
			SubClassSkill = 0;
			break;
		case 12: // fist
			SubClassSkill = SKILL_FIST_WEAPONS;
			break;
		case 13: // misc
			SubClassSkill = 0;
			break;
		case 15: // daggers
			SubClassSkill = SKILL_DAGGERS;
			break;
		case 16: // thrown
			SubClassSkill = SKILL_THROWN;
			break;
		case 18: // crossbows
			SubClassSkill = SKILL_CROSSBOWS;
			break;
		case 19: // wands
			SubClassSkill = SKILL_WANDS;
			break;
		case 20: // fishing
			SubClassSkill = SKILL_FISHING;
			break;
		}
	}
	else
	{
		SubClassSkill = 162;
	}

	uint32 skill = SubClassSkill;

	SkillLineEntry const* sl = sSkillLineStore.LookupEntry(skill);
	if (!sl)
	{
		RedSystemMessage("Can't find skill ID :-/");
		return false;
	}

	BlueSystemMessage("Modifying skill line %d. Advancing %d times.", skill, cnt);
	if (!target->HasSkill(skill))
		SystemMessage("Does not have skill line, adding.");

	target->SetSkill(skill, cnt, target->GetMaxSkillValue(skill));
	return true;
}

bool ChatHandler::HandleCellDeleteCommand(const char *args)
{
	return true;
}

bool ChatHandler::HandleResetLevelCommand(const char* args)
{
	Player *player = getSelectedPlayer();
	if (!player)
		return true;

	player->SetLevel(1);
	player->InitStatsForLevel(true);
	player->InitTaxiNodesForLevel();
	player->InitTalentForLevel();
	player->SetUInt32Value(PLAYER_XP, 0);

	// reset level to summoned pet
	Pet* pet = player->GetPet();
	if (pet && pet->getPetType() == SUMMON_PET)
		pet->InitStatsForLevel(1);

	SystemMessage("Reset stats of %s to level 1. Use .levelup to change level, and .resettalents and/or .resetspells if necessary.", player->GetName());
	ChatHandler(player).BlueSystemMessage("%s reset all your stats to starting values.", m_session->GetPlayer()->GetName());

	return true;
}

bool ChatHandler::HandleResetTalentsCommand(const char* args)
{
	Player *player = getSelectedPlayer();
	if (!player)
		return true;

	player->resetTalents();

	SystemMessage("Reset talents of %s.", player->GetName());;
	ChatHandler(player).BlueSystemMessage("%s reset all your talents.", m_session->GetPlayer()->GetName());
	return true;
}

bool ChatHandler::HandleResetSpellsCommand(const char* args)
{
	Player *player = getSelectedPlayer();
	if (!player)
		return true;

	player->resetSpells();

	SystemMessage("Reset spells of %s to level 1.", player->GetName());;
	ChatHandler(player).BlueSystemMessage("%s reset all your spells to starting values.", m_session->GetPlayer()->GetName());
	return true;
}

bool ChatHandler::HandleCreateAccountCommand(const char* args)
{
	char *user = strtok((char *)args, " ");
	if (!user) return false;
	char *pass = strtok(NULL, " ");
	if (!pass) return false;
	char *email = strtok(NULL, "\n");
	if (!email) return false;

	sAccountMgr.CreateAccount(user, pass, email);

	BlueSystemMessage("Attempting to create account: %s, %s (Email: %s)...", user, pass, email);
	GreenSystemMessage("Account creation successful. The account will be active with the next reload cycle.");
	return true;
}

bool ChatHandler::HandleGetTransporterTime(const char* args)
{
	Creature* crt = getSelectedCreature();
	if (!crt)
		return false;

	WorldPacket data(SMSG_ATTACKERSTATEUPDATE, 1000);
	data << uint32(0x00000102);
	data << crt->GetPackGUID();
	data << m_session->GetPlayer()->GetPackGUID();

	data << uint32(6);
	data << uint8(1);
	data << uint32(1);
	data << uint32(0x40c00000);
	data << uint32(6);
	data << uint32(0);
	data << uint32(0);
	data << uint32(1);
	data << uint32(0x000003e8);
	data << uint32(0);
	data << uint32(0);
	m_session->SendPacket(data);
	return true;
}

bool ChatHandler::HandleRemoveAurasCommand(const char *args)
{
	Player *player = getSelectedPlayer();
	if (!player)
		return false;

	BlueSystemMessage("Removing all auras...");
	player->RemoveAllAuras();
	return true;
}

bool ChatHandler::HandleParalyzeCommand(const char* args)
{
	Player *player = getSelectedPlayer();
	if (!player)
	{
		RedSystemMessage("Invalid target.");
		return true;
	}

	BlueSystemMessage("Rooting target.");
	ChatHandler(player).BlueSystemMessage("You have been rooted by %s.", m_session->GetPlayer()->GetName());
	player->SetRoot(true);
	return true;
}

bool ChatHandler::HandleUnParalyzeCommand(const char* args)
{
	Player *player = getSelectedPlayer();
	if (!player)
	{
		RedSystemMessage("Invalid target.");
		return true;
	}

	BlueSystemMessage("Unrooting target.");
	ChatHandler(player).BlueSystemMessage("You have been unrooted by %s.", m_session->GetPlayer()->GetName());
	player->SetRoot(false);
	return true;
}

bool ChatHandler::HandleSetMotdCommand(const char* args)
{
	if (!args || strlen(args) < 2)
	{
		RedSystemMessage("You must specify a message.");
		return true;
	}
	sWorld.SetMotd(args);
	GreenSystemMessage("Motd has been set to: %s", args);
	return true;
}

bool ChatHandler::HandleAddItemSetCommand(const char* args)
{
	uint32 setid = (args ? atoi(args) : 0);
	if (!setid)
	{
		RedSystemMessage("You must specify a setid.");
		return true;
	}

	Player *player = getSelectedPlayer();
	if (player == NULL) {
		RedSystemMessage("Unable to select character.");
		return true;
	}

	QueryResult *result = WorldDatabase.PQuery("SELECT entry FROM item_template WHERE itemset = %u", setid);

	if (!result)
	{
		RedSystemMessage("Invalid item set.");
		return true;
	}

	const ItemSetEntry *itemset = sItemSetStore.LookupEntry(setid);
	BlueSystemMessage("Searching item set %u (%s)...", setid, itemset->name);
	uint32 start = WorldTimer::getMSTime();
	do
	{
		Field *fields = result->Fetch();
		uint32 itemId = fields[0].GetUInt32();

		ItemPosCountVec dest;
		uint8 msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, 1);
		if (dest.empty)
		{
			m_session->SendNotification("No free slots left!");
			return true;
		}
		else
		{
			Item* item = player->StoreNewItem(dest, itemId, true);

			player->SendNewItem(item, 1, true, false);

			SystemMessage("Added item: %s [%u]", item->GetProto()->Name1, item->GetProto()->ItemId);
		}
	} while (result->NextRow());
	delete result;

	GreenSystemMessage("Added set to inventory complete. Time: %u ms", WorldTimer::getMSTime() - start);

	return true;
}

bool ChatHandler::HandleCastTimeCheatCommand(const char* args)
{
	Player * player = getSelectedPlayer();
	if (!player)
		return true;

	bool val = player->CastTimeCheat;
	BlueSystemMessage("%s cast time cheat on %s.", val ? "Deactivating" : "Activating", player->GetName());
	ChatHandler(player).GreenSystemMessage("%s %s a cast time cheat on you.", m_session->GetPlayer()->GetName(), val ? "deactivated" : "activated");

	player->CastTimeCheat = !val;
	return true;
}

bool ChatHandler::HandleCooldownCheatCommand(const char* args)
{
	Player * player = getSelectedPlayer();
	if (!player)
		return true;

	bool val = player->CooldownCheat;
	BlueSystemMessage("%s cooldown cheat on %s.", val ? "Deactivating" : "Activating", player->GetName());
	ChatHandler(player).GreenSystemMessage("%s %s a cooldown cheat on you.", m_session->GetPlayer()->GetName(), val ? "deactivated" : "activated");

	player->CooldownCheat = !val;
	return true;
}

bool ChatHandler::HandleGodModeCommand(const char* args)
{
	Player * player = getSelectedPlayer();
	if (!player)
		return true;

	bool val = player->GodModeCheat;
	BlueSystemMessage("%s godmode cheat on %s.", val ? "Deactivating" : "Activating", player->GetName());
	ChatHandler(player).GreenSystemMessage("%s %s a godmode cheat on you.", m_session->GetPlayer()->GetName(), val ? "deactivated" : "activated");

	player->GodModeCheat = !val;
	return true;
}

bool ChatHandler::HandlePowerCheatCommand(const char* args)
{
	Player * player = getSelectedPlayer();
	if (!player)
		return true;

	bool val = player->PowerCheat;
	BlueSystemMessage("%s power cheat on %s.", val ? "Deactivating" : "Activating", player->GetName());
	ChatHandler(player).GreenSystemMessage("%s %s a power cheat on you.", m_session->GetPlayer()->GetName(), val ? "deactivated" : "activated");

	player->PowerCheat = !val;
	return true;
}

bool ChatHandler::HandleShowCheatsCommand(const char* args)
{
	Player * player = getSelectedPlayer();
	if (!player)
		return true;

	uint32 active = 0, inactive = 0;
#define print_cheat_status(CheatName, CheatVariable) SystemMessage("%s%s: %s%s", MSG_COLOR_LIGHTBLUE, CheatName, \
		CheatVariable ? MSG_COLOR_LIGHTRED : MSG_COLOR_GREEN, CheatVariable ? "Active" : "Inactive");  \
		if(CheatVariable) \
		active++; \
		else \
		inactive++; 

	GreenSystemMessage("Showing cheat status for: %s", player->GetName());
	print_cheat_status("Cooldown", player->CooldownCheat);
	print_cheat_status("CastTime", player->CastTimeCheat);
	print_cheat_status("GodMode", player->GodModeCheat);
	print_cheat_status("Power", player->PowerCheat);
	print_cheat_status("Fly", player->FlyCheat);
	print_cheat_status("AuraStack", player->stack_cheat);
	SystemMessage("%u cheats active, %u inactive.", active, inactive);

#undef print_cheat_status

	return true;
}

bool ChatHandler::HandleFlyCommand(const char* args)
{
	WorldPacket data(SMSG_MOVE_SET_CAN_FLY, 12);

	Player *player = getSelectedPlayer();

	if (!player)
		player = m_session->GetPlayer();

	player->FlyCheat = true;
	data << player->GetPackGUID();
	data << uint32(0);
	player->SendMessageToSet(data, true);
	ChatHandler(player).BlueSystemMessage("Flying mode enabled.");
	return true;
}

bool ChatHandler::HandleLandCommand(const char* args)
{
	WorldPacket data(SMSG_MOVE_UNSET_CAN_FLY, 12);

	Player *player = getSelectedPlayer();

	if (!player)
		player = m_session->GetPlayer();

	player->FlyCheat = false;
	data << player->GetPackGUID();
	data << uint32(0);
	player->SendMessageToSet(data, true);
	ChatHandler(player).BlueSystemMessage("Flying mode disabled.");
	return true;
}

bool ChatHandler::HandleDBReloadCommand(const char* args)
{
}