#include "Common.h"
#include "Tools/Language.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "Server/WorldSession.h"
#include "Server/Opcodes.h"
#include "Log.h"
#include "World/World.h"
#include "Globals/ObjectMgr.h"
#include "Entities/Player.h"
#include "Chat/Chat.h"

bool ChatHandler::load_command_table = true;

ChatCommand * ChatHandler::getCommandTable()
{
	static ChatCommand commandTable[] = {
		{ "commands", SEC_PLAYER, &ChatHandler::HandleCommandsCommand, "Shows Commands", NULL, 0, 0, 0 },
	};

	if (load_command_table)
	{
		load_command_table = false;

		QueryResult *result = WorldDatabase.Query("SELECT name,security FROM command");
		if (result)
		{
			do
			{
				Field *fields = result->Fetch();
				std::string name = fields[0].GetCppString();
				for (uint32 i = 0; commandTable[i].Name != NULL; i++)
				{
					if (name == commandTable[i].Name)
						commandTable[i].SecurityLevel = (uint16)fields[1].GetUInt16();
					if (commandTable[i].ChildCommands != NULL)
					{
						ChatCommand *ptable = commandTable[i].ChildCommands;
						for (uint32 j = 0; ptable[j].Name != NULL; j++)
						{
							// first case for "" named subcommand
							if (ptable[j].Name[0] == '\0' && name == commandTable[i].Name || name == fmtstring("%s %s", commandTable[i].Name, ptable[j].Name))
								ptable[j].SecurityLevel = (uint16)fields[1].GetUInt16();
						}
					}
				}
			} while (result->NextRow());
			delete result;
		}
	}

	return commandTable;
}

bool ChatHandler::hasStringAbbr(const char* name, const char* part)
{
	// non "" command
	if (*name)
	{
		// "" part from non-"" command
		if (!*part)
			return false;

		for (;;)
		{
			if (!*part)
				return true;
			else if (!*name)
				return false;
			else if (tolower(*name) != tolower(*part))
				return false;
			++name; ++part;
		}
	}
	// allow with any for ""

	return true;
}

void ChatHandler::SendMultilineMessage(const char *str)
{
	char * start = (char*)str, *end;
	for (;;)
	{
		end = strchr(start, '\n');
		if (!end)
			break;

		*end = '\0';
		SystemMessage(start);
		start = end + 1;
	}
	if (*start != '\0')
		SystemMessage(start);
}

bool ChatHandler::ExecuteCommandInTable(ChatCommand *table, const char* text, std::string fullcmd)
{
	char const* oldtext = text;
	std::string cmd = "";

	while (*text != ' ' && *text != '\0')
	{
		cmd += *text;
		++text;
	}

	while (*text == ' ') ++text;

	for (uint32 i = 0; table[i].Name != NULL; i++)
	{
		if (!hasStringAbbr(table[i].Name, cmd.c_str()))
			continue;

		// select subcommand from child commands list
		if (table[i].ChildCommands != NULL)
		{
			if (!ExecuteCommandInTable(table[i].ChildCommands, text, fullcmd))
			{
				if (!table[i].Help.empty())
					SendMultilineMessage(table[i].Help.c_str());
				else
					GreenSystemMessage("Available Subcommands:");
				for (uint32 k = 0; table[i].ChildCommands[k].Name; k++)
				{
					if (m_session->GetSecurity() >= table[i].SecurityLevel)
						BlueSystemMessage("	%s - %s", table[i].Name, table[i].Help.empty() ? "No Help Available" : table[i].ChildCommands[k].Help.c_str());
				}
			}

			return true;
		}

		if (m_session->GetSecurity() < table[i].SecurityLevel)
			continue;

		if ((this->*(table[i].Handler))((char*)text))
		{
			Player* p = m_session->GetPlayer();
			ObjectGuid sel_guid = p->GetSelectionGuid();
			sLog.outCommand(m_session->GetAccountId(), "Command: %s [Player: %s (Account: %u) X: %f Y: %f Z: %f Map: %u Selected: %s]",
				fullcmd, p->GetName(), m_session->GetAccountId(), p->GetPositionX(), p->GetPositionY(), p->GetPositionZ(), p->GetMapId(),
				sel_guid.GetString().c_str());
		}

		if (!table[i].Help.empty())
			SendMultilineMessage(table[i].Help.c_str());
		else
			RedSystemMessage("Incorrect syntax specified. Try .help %s for the correct syntax.", table[i].Name);

		return true;
	}

	return false;
}

int ChatHandler::ParseCommands(const char* text)
{
	MANGOS_ASSERT(text);
	MANGOS_ASSERT(*text);

	//if(m_session->GetSecurity() == 0)
	//    return 0;

	if (text[0] != '!' && text[0] != '.')
		return 0;

	// ignore single . and ! in line
	if (strlen(text) < 2)
		return 0;

	// ignore messages staring from many dots.
	if (text[0] == '.' && text[1] == '.' || text[0] == '!' && text[1] == '!')
		return 0;

	++text;

	std::string fullcmd = text;                             // original `text` can't be used. It content destroyed in command code processing.

	if (!ExecuteCommandInTable(getCommandTable(), text, fullcmd))
		SystemMessage("There is no such command, or you do not have access to it.");

	return 1;
}

WorldPacket* ChatHandler::FillMessageData(uint32 type, uint32 language, const char* message, uint64 guid, uint8 flag) const
{
	//Packet    structure
	//uint8	    type;
	//uint32	language;
	//uint64	guid;
	//uint64	guid;
	//uint32	len_of_text;
	//char	    text[];		 // not sure ? i think is null terminated .. not null terminated
	//uint8	    afk_state;
	MANGOS_ASSERT(type != CHAT_MSG_CHANNEL);
	//channels are handled in channel handler and so on
	uint32 messageLength = (uint32)strlen(message) + 1;

	WorldPacket* data = new WorldPacket(SMSG_MESSAGECHAT, messageLength + 30);

	*data << (uint8)type;
	*data << language;

	*data << guid;
	*data << uint32(0);

	*data << guid;

	*data << messageLength;
	*data << message;

	*data << uint8(flag);
	return data;
}

WorldPacket* ChatHandler::FillSystemMessageData(const char* message) const
{
	uint32 messageLength = (uint32)strlen(message) + 1;

	WorldPacket* data = new WorldPacket(SMSG_MESSAGECHAT, 30 + messageLength);
	*data << (uint8)CHAT_MSG_SYSTEM;
	*data << (uint32)LANG_UNIVERSAL;

	// Who cares about guid when there's no nickname displayed heh ?
	*data << (uint64)0;
	*data << (uint32)0;
	*data << (uint64)0;

	*data << messageLength;
	*data << message;

	*data << uint8(0);

	return data;
}

Player* ChatHandler::getSelectedPlayer() const
{
	if (!m_session)
		return nullptr;

	ObjectGuid guid = m_session->GetPlayer()->GetSelectionGuid();

	if (!guid)
		return m_session->GetPlayer();

	return sObjectMgr.GetPlayer(guid);
}

Unit* ChatHandler::getSelectedUnit() const
{
	if (!m_session)
		return nullptr;

	ObjectGuid guid = m_session->GetPlayer()->GetSelectionGuid();

	if (!guid)
		return m_session->GetPlayer();

	// can be selected player at another map
	return ObjectAccessor::GetUnit(*m_session->GetPlayer(), guid);
}

Creature* ChatHandler::getSelectedCreature() const
{
	if (!m_session)
		return nullptr;

	return m_session->GetPlayer()->GetMap()->GetAnyTypeCreature(m_session->GetPlayer()->GetSelectionGuid());
}

void ChatHandler::SystemMessage(const char* message, ...)
{
	if (!message) return;
	va_list ap;
	va_start(ap, message);
	char msg1[1024];
	vsnprintf(msg1, 1024, message, ap);
	WorldPacket *data = FillSystemMessageData(msg1);
	if (m_session != NULL)
		m_session->SendPacket(*data);
	delete data;
}

void ChatHandler::ColorSystemMessage(const char* colorcode, const char *message, ...)
{
	if (!message) return;
	va_list ap;
	va_start(ap, message);
	char msg1[1024];
	vsnprintf(msg1, 1024, message, ap);
	char msg[1024];
	snprintf(msg, 1024, "%s%s|r", colorcode, msg1);
	WorldPacket * data = FillSystemMessageData(msg);
	if (m_session != NULL)
		m_session->SendPacket(*data);
	delete data;
}

void ChatHandler::RedSystemMessage(const char *message, ...)
{
	if (!message) return;
	va_list ap;
	va_start(ap, message);
	char msg1[1024];
	vsnprintf(msg1, 1024, message, ap);
	char msg[1024];
	snprintf(msg, 1024, "%s%s|r", MSG_COLOR_LIGHTRED/*MSG_COLOR_RED*/, msg1);
	WorldPacket * data = FillSystemMessageData(msg);
	if (m_session != NULL)
		m_session->SendPacket(*data);
	delete data;
}

void ChatHandler::GreenSystemMessage(const char *message, ...)
{
	if (!message) return;
	va_list ap;
	va_start(ap, message);
	char msg1[1024];
	vsnprintf(msg1, 1024, message, ap);
	char msg[1024];
	snprintf(msg, 1024, "%s%s|r", MSG_COLOR_GREEN, msg1);
	WorldPacket * data = FillSystemMessageData(msg);
	if (m_session != NULL)
		m_session->SendPacket(*data);
	delete data;
}

void ChatHandler::BlueSystemMessage(const char *message, ...)
{
	if (!message) return;
	va_list ap;
	va_start(ap, message);
	char msg1[1024];
	vsnprintf(msg1, 1024, message, ap);
	char msg[1024];
	snprintf(msg, 1024, "%s%s|r", MSG_COLOR_LIGHTBLUE, msg1);
	WorldPacket * data = FillSystemMessageData(msg);
	if (m_session != NULL)
		m_session->SendPacket(*data);
	delete data;
}

bool ChatHandler::CmdSetValueField(WorldSession *m_session, uint32 field, uint32 fieldmax, const char *fieldname, const char *args)
{
	if (!args) return false;
	char* pvalue = strtok((char*)args, " ");
	uint32 mv, av;

	if (!pvalue)
		return false;
	else
		av = atol(pvalue);

	if (fieldmax)
	{
		char* pvaluemax = strtok(NULL, " ");
		if (!pvaluemax)
			return false;
		else
			mv = atol(pvaluemax);
	}
	else
	{
		mv = 0;
	}

	if (av <= 0 && mv > 0)
	{
		RedSystemMessage("Values are invalid. Value must be < max (if max exists), and both must be > 0.");
		return true;
	}
	if (fieldmax)
	{
		if (mv < av || mv <= 0)
		{
			RedSystemMessage("Values are invalid. Value must be < max (if max exists), and both must be > 0.");
			return true;
		}
	}

	Player *plr = getSelectedPlayer();
	if (plr)
	{
		if (fieldmax)
		{
			BlueSystemMessage("You set the %s of %s to %d/%d.", fieldname, plr->GetName(), av, mv);
			GreenSystemMessageToPlr(plr, "%s set your %s to %d/%d.", m_session->GetPlayer()->GetName(), fieldname, av, mv);
		}
		else
		{
			BlueSystemMessage("You set the %s of %s to %d.", fieldname, plr->GetName(), av);
			GreenSystemMessageToPlr(plr, "%s set your %s to %d.", m_session->GetPlayer()->GetName(), fieldname, av);
		}

		if (field == UNIT_FIELD_STAT1) av /= 2;
		if (field == UNIT_FIELD_BASE_HEALTH)
		{
			plr->SetUInt32Value(UNIT_FIELD_HEALTH, av);
		}

		plr->SetUInt32Value(field, av);

		if (fieldmax) {
			plr->SetUInt32Value(fieldmax, mv);
		}
	}
	else
	{
		Creature *cr = getSelectedCreature();
		if (cr)
		{
			if (!(field < UNIT_END && fieldmax < UNIT_END)) return false;
			std::string creaturename = "Unknown Being";
			if (cr->GetName())
				creaturename = cr->GetName();
			if (fieldmax)
				BlueSystemMessage("Setting %s of %s to %d/%d.", fieldname, creaturename.c_str(), av, mv);
			else
				BlueSystemMessage("Setting %s of %s to %d.", fieldname, creaturename.c_str(), av);
			if (field == UNIT_FIELD_STAT1) av /= 2;
			if (field == UNIT_FIELD_BASE_HEALTH)
				cr->SetUInt32Value(UNIT_FIELD_HEALTH, av);

			cr->SetUInt32Value(field, av);

			if (fieldmax) {
				cr->SetUInt32Value(fieldmax, mv);
			}
			// reset faction
			if (field == UNIT_FIELD_FACTIONTEMPLATE)
				cr->setFaction(cr->getFaction());

			cr->SaveToDB();
		}
		else
		{
			RedSystemMessage("Invalid Selection.");
		}
	}
	return true;
}

bool ChatHandler::CmdSetFloatField(WorldSession *m_session, uint32 field, uint32 fieldmax, const char *fieldname, const char *args)
{
	char* pvalue = strtok((char*)args, " ");
	float mv, av;

	if (!pvalue)
		return false;
	else
		av = (float)atof(pvalue);

	if (fieldmax)
	{
		char* pvaluemax = strtok(NULL, " ");
		if (!pvaluemax)
			return false;
		else
			mv = (float)atof(pvaluemax);
	}
	else
	{
		mv = 0;
	}

	if (av <= 0)
	{
		RedSystemMessage("Values are invalid. Value must be < max (if max exists), and both must be > 0.");
		return true;
	}
	if (fieldmax)
	{
		if (mv < av || mv <= 0)
		{
			RedSystemMessage("Values are invalid. Value must be < max (if max exists), and both must be > 0.");
			return true;
		}
	}

	Player *plr = getSelectedPlayer();
	if (plr)
	{
		if (fieldmax)
		{
			BlueSystemMessage("You set the %s of %s to %.1f/%.1f.", fieldname, plr->GetName(), av, mv);
			GreenSystemMessageToPlr(plr, "%s set your %s to %.1f/%.1f.", m_session->GetPlayer()->GetName(), fieldname, av, mv);
		}
		else
		{
			BlueSystemMessage("You set the %s of %s to %.1f.", fieldname, plr->GetName(), av);
			GreenSystemMessageToPlr(plr, "%s set your %s to %.1f.", m_session->GetPlayer()->GetName(), fieldname, av);
		}
		plr->SetFloatValue(field, av);
		if (fieldmax) plr->SetFloatValue(fieldmax, mv);
	}
	else
	{
		Creature *cr = getSelectedCreature();
		if (cr)
		{
			if (!(field < UNIT_END && fieldmax < UNIT_END)) return false;
			std::string creaturename = "Unknown Being";
			if (cr->GetName())
				creaturename = cr->GetName();
			if (fieldmax)
				BlueSystemMessage("Setting %s of %s to %.1f/%.1f.", fieldname, creaturename.c_str(), av, mv);
			else
				BlueSystemMessage("Setting %s of %s to %.1f.", fieldname, creaturename.c_str(), av);
			cr->SetFloatValue(field, av);
			if (fieldmax) {
				cr->SetFloatValue(fieldmax, mv);
			}
			//cr->SaveToDB();
		}
		else
		{
			RedSystemMessage("Invalid Selection.");
		}
	}
	return true;
}

char const *fmtstring(char const *format, ...)
{
	va_list        argptr;
#define    MAX_FMT_STRING    32000
	static char        temp_buffer[MAX_FMT_STRING];
	static char        string[MAX_FMT_STRING];
	static int        index = 0;
	char    *buf;
	int len;

	va_start(argptr, format);
	vsnprintf(temp_buffer, MAX_FMT_STRING, format, argptr);
	va_end(argptr);

	len = strlen(temp_buffer);

	if (len >= MAX_FMT_STRING)
		return "ERROR";

	if (len + index >= MAX_FMT_STRING - 1)
	{
		index = 0;
	}

	buf = &string[index];
	memcpy(buf, temp_buffer, len + 1);

	index += len + 1;

	return buf;
}