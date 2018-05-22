#include "Common.h"
#include "Database/DatabaseEnv.h"
#include "World/World.h"
#include "Entities/Player.h"
#include "Server/Opcodes.h"
#include "Chat/Chat.h"
#include "Globals/ObjectAccessor.h"
#include "Tools/Language.h"
#include "Accounts/AccountMgr.h"
#include "AI/ScriptDevAI/ScriptDevAIMgr.h"
#include "SystemConfig.h"
#include "revision.h"
#include "Util.h"

bool ChatHandler::ShowHelpForCommand(ChatCommand *table, const char* cmd)
{
	for (uint32 i = 0; table[i].Name != NULL; ++i)
	{
		if (m_session->GetSecurity() < table[i].SecurityLevel)
			continue;

		if (!hasStringAbbr(table[i].Name, cmd))
			continue;

		// have subcommand
		char const* subcmd = (*cmd) ? strtok(NULL, " ") : "";

		if (table[i].ChildCommands && subcmd && *subcmd)
		{
			if (ShowHelpForCommand(table[i].ChildCommands, subcmd))
				return true;
		}

		if (table[i].Help.empty())
		{
			SystemMessage("There is no help for that command");
			return true;
		}

		SendMultilineMessage(table[i].Help.c_str());

		return true;
	}

	return false;
}

bool ChatHandler::HandleHelpCommand(const char* args)
{
	if (!*args)
		return false;

	char* cmd = strtok((char*)args, " ");
	if (!cmd)
		return false;

	if (!ShowHelpForCommand(getCommandTable(), cmd))
		RedSystemMessage("Sorry, no help was found for this command, or that command does not exist.");

	return true;
}

bool ChatHandler::HandleCommandsCommand(const char* args)
{
	ChatCommand *table = getCommandTable();

	std::string output;
	uint32 count = 0;

	output = "Available commands: \n\n";

	for (uint32 i = 0; table[i].Name != NULL; ++i)
	{
		if (m_session->GetSecurity() < table[i].SecurityLevel)
			continue;

		if (!hasStringAbbr(table[i].Name, args))
			continue;

		switch (table[i].SecurityLevel)
		{
		case SEC_ADMINISTRATOR:
		{
			output += "|cffff6060";
			output += table[i].Name;
			output += "|r, ";
		}
		break;
		case SEC_MODERATOR:
		{
			output += "|cff00ffff";
			output += table[i].Name;
			output += ", ";
		}
		break;
		case SEC_GAMEMASTER:
		{
			output += "|cff00ff00";
			output += table[i].Name;
			output += "|r, ";
		}
		break;
		default:
		{
			output += "|cff00ccff";
			output += table[i].Name;
			output += "|r, ";
		}
		break;
		}
		
		count++;
		if (count == 5)  // 5 per line
		{
			output += "\n";
			count = 0;
		}
	}
	if (count)
		output += "\n";
	SendMultilineMessage(output.c_str());

	return true;
}

bool ChatHandler::HandleStartCommand(const char* args)
{
	std::string race;
	uint32 raceid = 0;

	Player *m_plyr = getSelectedPlayer();

	if (m_plyr && args && strlen(args) < 2)
	{
		raceid = m_plyr->getRace();
		switch (raceid)
		{
		case 1:
			race = "human";
			break;
		case 2:
			race = "orc";
			break;
		case 3:
			race = "dwarf";
			break;
		case 4:
			race = "nightelf";
			break;
		case 5:
			race = "undead";
			break;
		case 6:
			race = "tauren";
			break;
		case 7:
			race = "gnome";
			break;
		case 8:
			race = "troll";
			break;
		case 10:
			race = "bloodelf";
			break;
		case 11:
			race = "draenei";
			break;
		default:
			return false;
			break;
		}
	}
	else if (m_plyr && args && strlen(args) > 2)
	{
		race = args;
		std::transform(race.begin(), race.end(), race.begin(), ::tolower);

		// Teleport to specific race
		if (race == "human")
			raceid = 1;
		else if (race == "orc")
			raceid = 2;
		else if (race == "dwarf")
			raceid = 3;
		else if (race == "nightelf")
			raceid = 4;
		else if (race == "undead")
			raceid = 5;
		else if (race == "tauren")
			raceid = 6;
		else if (race == "gnome")
			raceid = 7;
		else if (race == "troll")
			raceid = 8;
		else if (race == "bloodelf")
			raceid = 10;
		else if (race == "draenei")
			raceid = 11;
		else
		{
			RedSystemMessage("Invalid start location! Valid locations are: human, dwarf, gnome, nightelf, draenei, orc, troll, tauren, undead, bloodelf");
			return true;
		}
	}
	else
	{
		return false;
	}

	const PlayerInfo *pInfo = sObjectMgr.GetPlayerInfo(raceid, m_plyr->getClass());
	if (!pInfo)
	{
		RedSystemMessage("Internal error: Could not find create info.");
		return false;
	}

	GreenSystemMessage("Telporting %s to %s starting location.", m_plyr->GetName(), race.c_str());

	m_session->GetPlayer()->TeleportTo(pInfo->mapId, pInfo->positionX, pInfo->positionY, pInfo->positionZ, pInfo->orientation);
	return true;
}

bool ChatHandler::HandleInfoCommand(const char* args)
{
	uint32 activeClientsNum = sWorld.GetActiveSessionCount();
	uint32 queuedClientsNum = sWorld.GetQueuedSessionCount();
	std::string str = secsToTimeString(sWorld.GetUptime());

	int gm = 0;
	int count = 0;
	int avg = 0;

	HashMapHolder<Player>::MapType &m = HashMapHolder<Player>::GetContainer();
	HashMapHolder<Player>::MapType::iterator itr = m.begin();
	for (; itr != m.end(); ++itr)
	{
		count++;
		avg += itr->second->GetSession()->GetLatency();
		if (itr->second->GetSession()->GetSecurity() && itr->second->isGameMaster())
			gm++;
	}

	GreenSystemMessage("Server Uptime: |r%s", str);
	GreenSystemMessage("Current Players: |r%d (%d GMs, %d queued)", activeClientsNum, gm, queuedClientsNum);
	GreenSystemMessage("Average Latency: |r%.3fms", (float)((float)avg / (float)count));

	return true;
}

bool ChatHandler::HandleNYICommand(const char* args)
{
	RedSystemMessage("Not yet implemented.");
	return true;
}

bool ChatHandler::HandleDismountCommand(const char* args)
{
	Player *m_plyr = getSelectedPlayer();

	if (!m_plyr->IsMounted())
	{
		RedSystemMessage("Target is not mounted.");
		return true;
	}

	m_session->GetPlayer()->Unmount();
	m_session->GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);

	BlueSystemMessage("Now unmounted.");
	return true;
}

bool ChatHandler::HandleSaveCommand(const char* args)
{
	Player *player = m_session->GetPlayer();

	if (m_session->GetSecurity())
	{
		player->SaveToDB();
		GreenSystemMessage("Player saved to DB");
		return true;
	}

	uint32 save_interval = sWorld.getConfig(CONFIG_UINT32_INTERVAL_SAVE);
	if (save_interval == 0 || (save_interval > 20 * IN_MILLISECONDS && player->GetSaveTimer() <= save_interval - 20 * IN_MILLISECONDS))
		player->SaveToDB();

	return true;
}

bool ChatHandler::HandleGMListCommand(const char* args)
{
	bool first = true;

	HashMapHolder<Player>::MapType &m = HashMapHolder<Player>::GetContainer();
	HashMapHolder<Player>::MapType::iterator itr = m.begin();
	for (; itr != m.end(); ++itr)
	{
		if (itr->second->GetSession()->GetSecurity() && itr->second->isGameMaster())
		{
			if (first)
				GreenSystemMessage("There are following active GMs on this server:");

			SystemMessage("%s []", itr->second->GetName());

			first = false;
		}
	}

	if (first)
		SystemMessage("There are no GMs currently logged in on this server.");

	return true;
}