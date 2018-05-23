#include "World/World.h"
#include "Spells/SpellMgr.h"
#include "Chat/Chat.h"

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

	WorldDatabase.PExecuteLog("UPDATE creature_template SET faction_A = '%u', faction_H = '%u' WHERE entry = '%u'", npcFaction, npcFaction, pCreature->GetEntry());

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
		CharacterDatabase.PExecute("UPDATE characters SET bannedReason = \"%s\" WHERE name = '%s'", Character);

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

