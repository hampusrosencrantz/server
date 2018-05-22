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
		return false;
	}

	char buf[256];
	snprintf((char*)buf, 256, "|cff00ff00Current Position: |cffffffffMap: |cff00ff00%d |cffffffffX: |cff00ff00%f |cffffffffY: |cff00ff00%f |cffffffffZ: |cff00ff00%f |cffffffffOrientation: |cff00ff00%f|r",
		(unsigned int)obj->GetMapId(), obj->GetPositionX(), obj->GetPositionY(), obj->GetPositionZ(), obj->GetOrientation());

	SystemMessage(buf);
}

bool ChatHandler::HandleKickCommand(const char* args)
{
	char* kickName = strtok((char*)args, " ");
	if (!kickName)
	{
		RedSystemMessage("No name specified.");
		return false;
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
	uint8 msg = pl->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemid, 1);

	if (dest.empty())
	{
		m_session->SendNotification("No free slots were found in your inventory!");
		return false;
	}

	Item* item = pl->StoreNewItem(dest, itemid, true, Item::GenerateItemRandomPropertyId(itemid));

	if (item)
	{
		pl->SendNewItem(item, 1, false, true);

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

