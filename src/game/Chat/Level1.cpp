#include "World/World.h"
#include "Chat/Chat.h"

bool ChatHandler::HandleAnnounceCommand(const char* args)
{
	if (!*args)
		return false;

	char pAnnounce[1024];
	std::string input2;

	input2 = "|cffff6060<GM>|r|c1f40af20";
	input2 += m_session->GetPlayer()->GetName();
	input2 += "|r|cffffffff broadcasts: |r";
	snprintf((char*)pAnnounce, 1024, "%s%s", input2.c_str(), args);   // Adds BROADCAST:
	sWorld.SendWorldText(pAnnounce); // send message

	return true;
}

bool ChatHandler::HandleWAnnounceCommand(const char* args)
{
	if (!*args)
		return false;

	char pAnnounce[1024];
	std::string input2;

	input2 = "|cffff6060<GM>|r|c1f40af20";
	input2 += m_session->GetPlayer()->GetName();
	input2 += "|r|cffffffff broadcasts: |r";
	snprintf((char*)pAnnounce, 1024, "%s%s", input2.c_str(), args);   // Adds BROADCAST:
	sWorld.SendWorldWideScreenText(pAnnounce); // send message

	return true;
}

bool ChatHandler::HandleGMOnCommand(const char* args)
{
	GreenSystemMessage("Setting GM Flag on yourself...");
	if (m_session->GetPlayer()->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GM))
		RedSystemMessage("GM Flag is already set on. Use !gmoff to disable it.");
	else
	{
		m_session->GetPlayer()->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_GM);
		BlueSystemMessage("GM Flag Set. It will appear above your name and in chat messages until you use !gmoff.");
	}

	return true;
}

bool ChatHandler::HandleGMOffCommand(const char* args)
{
	GreenSystemMessage("Unsetting GM Flag on yourself...");
	if (!m_session->GetPlayer()->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GM))
		RedSystemMessage("GM Flag not set. Use !gmon to enable it.");
	else
	{
		m_session->GetPlayer()->RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_GM);
		BlueSystemMessage("GM Flag Removed. <GM> Will no longer show in chat messages or above your name.");
	}

	return true;
}

bool ChatHandler::HandleGPSCommand(const char* args)
{
	WorldObject *obj = getSelectedUnit();

	if (!obj)
	{
		SystemMessage("You should select a character or a creature.");
		return true;
	}

	char buf[256];
	snprintf((char*)buf, 256, "|cff00ff00Current Position: |cffffffffMap: |cff00ff00%d |cffffffffX: |cff00ff00%f |cffffffffY: |cff00ff00%f |cffffffffZ: |cff00ff00%f |cffffffffOrientation: |cff00ff00%f|r",
		(unsigned int)obj->GetMapId(), obj->GetPositionX(), obj->GetPositionY(), obj->GetPositionZ(), obj->GetOrientation());

	SystemMessage(buf);

	return true;
}

bool ChatHandler::HandleKickCommand(const char* args)
{
	char* kickName = strtok((char*)args, " ");
	if (!kickName)
	{
		RedSystemMessage("No name specified.");
		return true;
	}

	Player *player = sObjectMgr.GetPlayer((const char*)kickName);
	if (player)
	{
		char *reason = strtok(NULL, "\n");
		std::string kickreason = "No reason";
		if (reason)
			kickreason = reason;

		BlueSystemMessage("Attempting to kick %s from the server for \"%s\".", player->GetName(), kickreason.c_str());

		char msg[200];
		snprintf(msg, 200, "%sGM: %s was kicked from the server by %s. Reason: %s", MSG_COLOR_RED, player->GetName(), m_session->GetPlayer()->GetName(), kickreason.c_str());
		sWorld.SendWorldText(msg);

		player->Kick(6000);
	}
	else
	{
		RedSystemMessage("Player is not online at the moment.");
		return false;
	}

	return true;
}

bool ChatHandler::HandleAddInvItemCommand(const char *args)
{
	if (!*args)
		return false;

	int itemid = atoi(strtok((char*)args, " "));
	uint32 count = 1;
	char *cCount = strtok(NULL, "\n");
	if (cCount) count = atoi(cCount);
	if (!count) count = 1;

	Player* pl = getSelectedPlayer();

	ItemPosCountVec dest;
	pl->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemid, 1);

	if (dest.empty())
	{
		m_session->SendNotification("No free slots were found in your inventory!");
		return true;
	}

	Item* item = pl->StoreNewItem(dest, itemid, true, Item::GenerateItemRandomPropertyId(itemid));

	if (item)
	{
		pl->SendNewItem(item, 1, true, false);

		char messagetext[128];
		snprintf(messagetext, 128, "Adding item %d (%s) to %s's inventory.", itemid, item->GetProto()->Name1, pl->GetName());
		SystemMessage(messagetext);
		snprintf(messagetext, 128, "%s added item %d (%s) to your inventory.", m_session->GetPlayer()->GetName(), itemid, item->GetProto()->Name1);
		ChatHandler(pl).SystemMessage(messagetext);
	}
	else
	{
		RedSystemMessage("Item %d is not a valid item!", itemid);
		return true;
	}

	return true;
}

bool ChatHandler::HandleSummonCommand(const char* args)
{
	if (!*args)
		return false;

	Player *player = sObjectMgr.GetPlayer(args);
	if (player)
	{
		// send message to user
		char buf[256];
		char buf0[256];
		if (player->IsBeingTeleported() == true)
		{
			snprintf((char*)buf, 256, "%s is already being teleported.", player->GetName());
			SystemMessage(buf);
			return true;
		}

		snprintf((char*)buf, 256, "You are summoning %s.", player->GetName());
		SystemMessage(buf);

		if (m_session->GetPlayer()->IsVisibleGloballyFor(player))
		{
			// send message to player
			snprintf((char*)buf0, 256, "You are being summoned by %s.", m_session->GetPlayer()->GetName());
			ChatHandler(player).SystemMessage(buf0);
		}

		if (!player->TaxiFlightInterrupt())
			player->SaveRecallPosition();

		// before GM
		float x, y, z;
		player->GetClosePoint(x, y, z, player->GetObjectBoundingRadius());
		player->TeleportTo(player->GetMapId(), x, y, z, player->GetOrientation());
	}
	else
	{
		char buf[256];
		snprintf((char*)buf, 256, "Player (%s) does not exist or is not logged in.", args);
		SystemMessage(buf);
	}

	return true;
}

bool ChatHandler::HandleAppearCommand(const char* args)
{
	if (!*args)
		return false;

	Player *player = sObjectMgr.GetPlayer(args);
	if (player)
	{
		char buf[256];
		if (player->IsBeingTeleported() == true)
		{
			snprintf((char*)buf, 256, "%s is already being teleported.", player->GetName());
			SystemMessage(buf);
			return true;
		}

		snprintf((char*)buf, 256, "Appearing at %s's location.", player->GetName());
		SystemMessage(buf);

		if (m_session->GetPlayer()->IsVisibleGloballyFor(player))
		{
			char buf0[256];
			snprintf((char*)buf0, 256, "%s is appearing to your location.", m_session->GetPlayer()->GetName());
			ChatHandler(player).SystemMessage(buf0);
		}

		// to point to see at target with same orientation
		float x, y, z;
		player->GetContactPoint(m_session->GetPlayer(), x, y, z);

		m_session->GetPlayer()->TeleportTo(player->GetMapId(), x, y, z, m_session->GetPlayer()->GetAngle(player), TELE_TO_GM_MODE);
	}
	else
	{
		char buf[256];
		snprintf((char*)buf, 256, "Player (%s) does not exist or is not logged in.", args);
		SystemMessage(buf);
	}

	return true;
}

bool ChatHandler::HandleTaxiCheatCommand(const char* args)
{
	if (!*args)
		return false;

	int flag = atoi((char*)args);

	Player *chr = getSelectedPlayer();
	if (!chr)
		return false;

	if (flag != 0)
	{
		chr->SetTaxiCheater(true);
		GreenSystemMessage("%s has all taxi nodes now.", chr->GetName());
		ChatHandler(chr).SystemMessage("%s has given you all taxi nodes.", m_session->GetPlayer()->GetName());
		return true;
	}
	else
	{
		chr->SetTaxiCheater(false);
		GreenSystemMessage("%s has no more taxi nodes now.", chr->GetName());
		ChatHandler(chr).SystemMessage("%s has deleted all your taxi nodes.", m_session->GetPlayer()->GetName());
		return true;
	}

	return false;
}

bool ChatHandler::HandleModifySpeedCommand(const char* args)
{
	if (!*args)
		return false;

	float Speed = (float)atof((char*)args);

	if (Speed > 255 || Speed < 1)
	{
		RedSystemMessage("Incorrect value. Range is 1..255");
		return true;
	}

	Player *chr = getSelectedPlayer();
	if (!chr)
		return true;

	// send message to user
	BlueSystemMessage("You set the speed to %2.2f of %s.", Speed, chr->GetName());

	// send message to player
	ChatHandler(chr).SystemMessage("%s set your speed to %2.2f.", m_session->GetPlayer()->GetName(), Speed);

	chr->UpdateSpeed(MOVE_WALK, true, Speed);
	chr->UpdateSpeed(MOVE_RUN, true, Speed);
	chr->UpdateSpeed(MOVE_SWIM, true, Speed);
	chr->UpdateSpeed(MOVE_FLIGHT, true, Speed);
	return true;
}

bool ChatHandler::HandleLearnSkillCommand(const char *args)
{
	uint32 skill, min, max;
	min = max = 1;
	char *pSkill = strtok((char*)args, " ");
	if (!pSkill)
		return false;
	else
		skill = atol(pSkill);

	char *pMin = strtok(NULL, " ");
	if (pMin)
	{
		min = atol(pMin);
		char *pMax = strtok(NULL, "\n");
		if (pMax)
			max = atol(pMax);
	}
	else {
		return false;
	}

	Player * target = getSelectedPlayer();
	if (!target)
		return false;

	SkillLineEntry const* sl = sSkillLineStore.LookupEntry(skill);
	if (!sl)
		return false;

	target->SetSkill(skill, min, max);
	BlueSystemMessage("Adding skill line %d", skill);
	return true;
}

bool ChatHandler::HandleModifySkillCommand(const char *args)
{
	uint32 skill, min, max;
	min = max = 1;
	char *pSkill = strtok((char*)args, " ");
	if (!pSkill)
		return false;
	else
		skill = atol(pSkill);

	char *pMin = strtok(NULL, " ");
	uint32 cnt = 0;
	if (!pMin)
		cnt = 1;
	else
		cnt = atol(pMin);

	skill = atol(pSkill);

	Player * target = getSelectedPlayer();
	if (!target)
		return false;

	SkillLineEntry const* sl = sSkillLineStore.LookupEntry(skill);
	if (!sl)
		return false;

	BlueSystemMessage("Modifying skill line %d. Advancing %d times.", skill, cnt);
	if (!target->HasSkill(skill))
		SystemMessage("Does not have skill line, adding.");

	target->SetSkill(skill, min, max);
	return true;
}

bool ChatHandler::HandleRemoveSkillCommand(const char *args)
{
	uint32 skill = 0;
	char *pSkill = strtok((char*)args, " ");
	if (!pSkill)
		return false;
	else
		skill = atol(pSkill);
	BlueSystemMessage("Removing skill line %d", skill);

	Player * target = getSelectedPlayer();
	if (!target)
		return false;

	SkillLineEntry const* sl = sSkillLineStore.LookupEntry(skill);
	if (!sl)
		return false;

	BlueSystemMessage("Removing skill line %d", skill);
	target->removeSpell(skill);
	ChatHandler(target).SystemMessage("%s removed skill line %d from you. ", m_session->GetPlayer()->GetName(), skill);
	return true;
}

bool ChatHandler::HandleEmoteCommand(const char* args)
{
	uint32 emote = atoi((char*)args);

	Creature* target = getSelectedCreature();
	if (!target)
		return false;

	target->SetUInt32Value(UNIT_NPC_EMOTESTATE, emote);
	return true;
}

bool ChatHandler::HandleModifyGoldCommand(const char* args)
{
	if (!*args)
		return false;

	int32 gold = atoi((char*)args);

	Player *chr = getSelectedPlayer();
	if (!chr)
		return false;

	BlueSystemMessage("Adding %d gold to %s's backpack...", gold, chr->GetName());

	int32 currentgold = chr->GetMoney();
	int32 newgold = currentgold + gold;

	if (newgold < 0)
	{
		ChatHandler(chr).GreenSystemMessage("%s took the all gold from your backpack.", m_session->GetPlayer()->GetName());
		chr->SetMoney(0);
	}
	else
	{
		if (newgold > currentgold)
		{
			ChatHandler(chr).GreenSystemMessage("%s took %d gold to your backpack.", m_session->GetPlayer()->GetName(), abs(gold));
			chr->SetMoney(gold);
		}
		else
		{
			ChatHandler(chr).GreenSystemMessage("%s added %d gold from your backpack.", m_session->GetPlayer()->GetName(), abs(gold));
			chr->SetMoney(newgold);
		}
	}

	return true;
}

bool ChatHandler::HandleTriggerCommand(const char* args)
{
	Player* _player = m_session->GetPlayer();

	if (!*args)
		return false;

	char *atId = strtok((char*)args, " ");
	if (!atId)
		return false;

	int32 i_atId = atoi(atId);

	if (!i_atId)
		return false;

	AreaTriggerEntry const* at = sAreaTriggerStore.LookupEntry(i_atId);
	if (!at)
	{
		RedSystemMessage("Could not find trigger %s", (args == NULL ? "NULL" : args));
		return true;
	}

	_player->TeleportTo(at->mapid, at->x, at->y, at->z, _player->GetOrientation());
	BlueSystemMessage("Teleported to trigger %u on [%u][%.2f][%.2f][%.2f]", at->id,
		at->mapid, at->x, at->y, at->z);
	return true;
}

bool ChatHandler::HandleUnlearnCommand(const char* args)
{
	if (!*args)
		return false;

	Player *target = getSelectedPlayer();

	if (!target)
		return true;

	uint32 spell = atol(args);
	if (spell == 0)
	{
		RedSystemMessage("You must specify a spell id.");
		return true;
	}

	if (target->HasSpell(spell))
	{
		ChatHandler(target).GreenSystemMessage("Removed spell %u.", spell);
		target->removeSpell(spell);
	}
	else
	{
		RedSystemMessage("That player does not have spell %u learnt.", spell);
	}

	return true;
}

bool ChatHandler::HandleNpcSpawnLinkCommand(const char* args)
{
	RedSystemMessage("Not yet implemented.");
	return true;
}