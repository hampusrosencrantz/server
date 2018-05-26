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
#include "Spells/SpellMgr.h"

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

ChatHandler::ChatHandler(WorldSession* session) : m_session(session)
{}

ChatHandler::ChatHandler(Player* player) : m_session(player->GetSession())
{}

ChatHandler::~ChatHandler() {}

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

bool ChatHandler::isValidChatMessage(const char* message) const
{
	/*
	valid examples:
	|cffa335ee|Hitem:812:0:0:0:0:0:0:0:70|h[Glowing Brightwood Staff]|h|r
	|cff808080|Hquest:2278:47|h[The Platinum Discs]|h|r
	|cffffd000|Htrade:4037:1:150:1:6AAAAAAAAAAAAAAAAAAAAAAOAADAAAAAAAAAAAAAAAAIAAAAAAAAA|h[Engineering]|h|r
	|cff4e96f7|Htalent:2232:-1|h[Taste for Blood]|h|r
	|cff71d5ff|Hspell:21563|h[Command]|h|r
	|cffffd000|Henchant:3919|h[Engineering: Rough Dynamite]|h|r
	|cffffff00|Hachievement:546:0000000000000001:0:0:0:-1:0:0:0:0|h[Safe Deposit]|h|r
	|cff66bbff|Hglyph:21:762|h[Glyph of Bladestorm]|h|r
	| will be escaped to ||
	*/

	if (strlen(message) > 255)
		return false;

	const char validSequence[6] = "cHhhr";
	const char* validSequenceIterator = validSequence;

	// more simple checks
	if (sWorld.getConfig(CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_SEVERITY) < 3)
	{
		const std::string validCommands = "cHhr|";

		while (*message)
		{
			// find next pipe command
			message = strchr(message, '|');

			if (!message)
				return true;

			++message;
			char commandChar = *message;
			if (validCommands.find(commandChar) == std::string::npos)
				return false;

			++message;
			// validate sequence
			if (sWorld.getConfig(CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_SEVERITY) == 2)
			{
				if (commandChar == *validSequenceIterator)
				{
					if (validSequenceIterator == validSequence + 4)
						validSequenceIterator = validSequence;
					else
						++validSequenceIterator;
				}
				else if (commandChar != '|')
					return false;
			}
		}
		return true;
	}

	std::istringstream reader(message);
	char buffer[256];

	uint32 color = 0;

	ItemPrototype const* linkedItem = nullptr;
	Quest const* linkedQuest = nullptr;
	SpellEntry const* linkedSpell = nullptr;
	AchievementEntry const* linkedAchievement = nullptr;
	ItemRandomPropertiesEntry const* itemProperty = nullptr;
	ItemRandomSuffixEntry const* itemSuffix = nullptr;

	while (!reader.eof())
	{
		if (validSequence == validSequenceIterator)
		{
			linkedItem = nullptr;
			linkedQuest = nullptr;
			linkedSpell = nullptr;
			linkedAchievement = nullptr;
			itemProperty = nullptr;
			itemSuffix = nullptr;

			reader.ignore(255, '|');
		}
		else if (reader.get() != '|')
		{
			DEBUG_LOG("ChatHandler::isValidChatMessage sequence aborted unexpectedly");
			return false;
		}

		// pipe has always to be followed by at least one char
		if (reader.peek() == '\0')
		{
			DEBUG_LOG("ChatHandler::isValidChatMessage pipe followed by \\0");
			return false;
		}

		// no further pipe commands
		if (reader.eof())
			break;

		char commandChar;
		reader >> commandChar;

		// | in normal messages is escaped by ||
		if (commandChar != '|')
		{
			if (commandChar == *validSequenceIterator)
			{
				if (validSequenceIterator == validSequence + 4)
					validSequenceIterator = validSequence;
				else
					++validSequenceIterator;
			}
			else
			{
				DEBUG_LOG("ChatHandler::isValidChatMessage invalid sequence, expected %c but got %c", *validSequenceIterator, commandChar);
				return false;
			}
		}
		else if (validSequence != validSequenceIterator)
		{
			// no escaped pipes in sequences
			DEBUG_LOG("ChatHandler::isValidChatMessage got escaped pipe in sequence");
			return false;
		}

		switch (commandChar)
		{
		case 'c':
			color = 0;
			// validate color, expect 8 hex chars
			for (int i = 0; i < 8; ++i)
			{
				char c;
				reader >> c;
				if (!c)
				{
					DEBUG_LOG("ChatHandler::isValidChatMessage got \\0 while reading color in |c command");
					return false;
				}

				color <<= 4;
				// check for hex char
				if (c >= '0' && c <= '9')
				{
					color |= c - '0';
					continue;
				}
				if (c >= 'a' && c <= 'f')
				{
					color |= 10 + c - 'a';
					continue;
				}
				DEBUG_LOG("ChatHandler::isValidChatMessage got non hex char '%c' while reading color", c);
				return false;
			}
			break;
		case 'H':
			// read chars up to colon  = link type
			reader.getline(buffer, 256, ':');
			if (reader.eof())                           // : must be
				return false;

			if (strcmp(buffer, "item") == 0)
			{
				// read item entry
				reader.getline(buffer, 256, ':');
				if (reader.eof())                       // : must be
					return false;

				linkedItem = ObjectMgr::GetItemPrototype(atoi(buffer));
				if (!linkedItem)
				{
					DEBUG_LOG("ChatHandler::isValidChatMessage got invalid itemID %u in |item command", atoi(buffer));
					return false;
				}

				if (color != ItemQualityColors[linkedItem->Quality])
				{
					DEBUG_LOG("ChatHandler::isValidChatMessage linked item has color %u, but user claims %u", ItemQualityColors[linkedItem->Quality],
						color);
					return false;
				}

				// the itementry is followed by several integers which describe an instance of this item

				// position relative after itemEntry
				const uint8 randomPropertyPosition = 6;

				int32 propertyId = 0;
				bool negativeNumber = false;
				char c;
				for (uint8 i = 0; i < randomPropertyPosition; ++i)
				{
					propertyId = 0;
					negativeNumber = false;
					while ((c = reader.get()) != ':')
					{
						if (c >= '0' && c <= '9')
						{
							propertyId *= 10;
							propertyId += c - '0';
						}
						else if (c == '-')
							negativeNumber = true;
						else
							return false;
					}
				}
				if (negativeNumber)
					propertyId *= -1;

				if (propertyId > 0)
				{
					itemProperty = sItemRandomPropertiesStore.LookupEntry(propertyId);
					if (!itemProperty)
						return false;
				}
				else if (propertyId < 0)
				{
					itemSuffix = sItemRandomSuffixStore.LookupEntry(-propertyId);
					if (!itemSuffix)
						return false;
				}

				// ignore other integers
				while ((c >= '0' && c <= '9') || c == ':')
				{
					reader.ignore(1);
					c = reader.peek();
				}
			}
			else if (strcmp(buffer, "quest") == 0)
			{
				// no color check for questlinks, each client will adapt it anyway
				uint32 questid = 0;
				// read questid
				char c = reader.peek();
				while (c >= '0' && c <= '9')
				{
					reader.ignore(1);
					questid *= 10;
					questid += c - '0';
					c = reader.peek();
				}

				linkedQuest = sObjectMgr.GetQuestTemplate(questid);

				if (!linkedQuest)
				{
					DEBUG_LOG("ChatHandler::isValidChatMessage Questtemplate %u not found", questid);
					return false;
				}

				if (c != ':')
				{
					DEBUG_LOG("ChatHandler::isValidChatMessage Invalid quest link structure");
					return false;
				}

				reader.ignore(1);
				c = reader.peek();
				// level
				uint32 questlevel = 0;
				while (c >= '0' && c <= '9')
				{
					reader.ignore(1);
					questlevel *= 10;
					questlevel += c - '0';
					c = reader.peek();
				}

				if (questlevel >= STRONG_MAX_LEVEL)
				{
					DEBUG_LOG("ChatHandler::isValidChatMessage Quest level %u too big", questlevel);
					return false;
				}

				if (c != '|')
				{
					DEBUG_LOG("ChatHandler::isValidChatMessage Invalid quest link structure");
					return false;
				}
			}
			else if (strcmp(buffer, "trade") == 0)
			{
				if (color != CHAT_LINK_COLOR_TRADE)
					return false;

				// read spell entry
				reader.getline(buffer, 256, ':');
				if (reader.eof())                       // : must be
					return false;

				linkedSpell = sSpellTemplate.LookupEntry<SpellEntry>(atoi(buffer));
				if (!linkedSpell)
					return false;

				char c = reader.peek();
				// base64 encoded stuff
				while (c != '|' && c != '\0')
				{
					reader.ignore(1);
					c = reader.peek();
				}
			}
			else if (strcmp(buffer, "talent") == 0)
			{
				// talent links are always supposed to be blue
				if (color != CHAT_LINK_COLOR_TALENT)
					return false;

				// read talent entry
				reader.getline(buffer, 256, ':');
				if (reader.eof())                       // : must be
					return false;

				TalentEntry const* talentInfo = sTalentStore.LookupEntry(atoi(buffer));
				if (!talentInfo)
					return false;

				linkedSpell = sSpellTemplate.LookupEntry<SpellEntry>(talentInfo->RankID[0]);
				if (!linkedSpell)
					return false;

				char c = reader.peek();
				// skillpoints? whatever, drop it
				while (c != '|' && c != '\0')
				{
					reader.ignore(1);
					c = reader.peek();
				}
			}
			else if (strcmp(buffer, "spell") == 0)
			{
				if (color != CHAT_LINK_COLOR_SPELL)
					return false;

				uint32 spellid = 0;
				// read spell entry
				char c = reader.peek();
				while (c >= '0' && c <= '9')
				{
					reader.ignore(1);
					spellid *= 10;
					spellid += c - '0';
					c = reader.peek();
				}
				linkedSpell = sSpellTemplate.LookupEntry<SpellEntry>(spellid);
				if (!linkedSpell)
					return false;
			}
			else if (strcmp(buffer, "enchant") == 0)
			{
				if (color != CHAT_LINK_COLOR_ENCHANT)
					return false;

				uint32 spellid = 0;
				// read spell entry
				char c = reader.peek();
				while (c >= '0' && c <= '9')
				{
					reader.ignore(1);
					spellid *= 10;
					spellid += c - '0';
					c = reader.peek();
				}
				linkedSpell = sSpellTemplate.LookupEntry<SpellEntry>(spellid);
				if (!linkedSpell)
					return false;
			}
			else if (strcmp(buffer, "achievement") == 0)
			{
				if (color != CHAT_LINK_COLOR_ACHIEVEMENT)
					return false;

				reader.getline(buffer, 256, ':');
				if (reader.eof())                       // : must be
					return false;

				uint32 achievementId = atoi(buffer);
				linkedAchievement = sAchievementStore.LookupEntry(achievementId);

				if (!linkedAchievement)
					return false;

				char c = reader.peek();
				// skip progress
				while (c != '|' && c != '\0')
				{
					reader.ignore(1);
					c = reader.peek();
				}
			}
			else if (strcmp(buffer, "glyph") == 0)
			{
				if (color != CHAT_LINK_COLOR_GLYPH)
					return false;

				// first id is slot, drop it
				reader.getline(buffer, 256, ':');
				if (reader.eof())                       // : must be
					return false;

				uint32 glyphId = 0;
				char c = reader.peek();
				while (c >= '0' && c <= '9')
				{
					glyphId *= 10;
					glyphId += c - '0';
					reader.ignore(1);
					c = reader.peek();
				}
				GlyphPropertiesEntry const* glyph = sGlyphPropertiesStore.LookupEntry(glyphId);
				if (!glyph)
					return false;

				linkedSpell = sSpellTemplate.LookupEntry<SpellEntry>(glyph->SpellId);

				if (!linkedSpell)
					return false;
			}
			else
			{
				DEBUG_LOG("ChatHandler::isValidChatMessage user sent unsupported link type '%s'", buffer);
				return false;
			}
			break;
		case 'h':
			// if h is next element in sequence, this one must contain the linked text :)
			if (*validSequenceIterator == 'h')
			{
				// links start with '['
				if (reader.get() != '[')
				{
					DEBUG_LOG("ChatHandler::isValidChatMessage link caption doesn't start with '['");
					return false;
				}
				reader.getline(buffer, 256, ']');
				if (reader.eof())                       // ] must be
					return false;

				// verify the link name
				if (linkedSpell)
				{
					// spells with that flag have a prefix of "$PROFESSION: "
					if (linkedSpell->HasAttribute(SPELL_ATTR_TRADESPELL))
					{
						// lookup skillid
						SkillLineAbilityMapBounds bounds = sSpellMgr.GetSkillLineAbilityMapBounds(linkedSpell->Id);
						if (bounds.first == bounds.second)
						{
							return false;
						}

						SkillLineAbilityEntry const* skillInfo = bounds.first->second;

						if (!skillInfo)
						{
							return false;
						}

						SkillLineEntry const* skillLine = sSkillLineStore.LookupEntry(skillInfo->skillId);
						if (!skillLine)
						{
							return false;
						}

						for (uint8 i = 0; i < MAX_LOCALE; ++i)
						{
							uint32 skillLineNameLength = strlen(skillLine->name[i]);
							if (skillLineNameLength > 0 && strncmp(skillLine->name[i], buffer, skillLineNameLength) == 0)
							{
								// found the prefix, remove it to perform spellname validation below
								// -2 = strlen(": ")
								uint32 spellNameLength = strlen(buffer) - skillLineNameLength - 2;
								memmove(buffer, buffer + skillLineNameLength + 2, spellNameLength + 1);
							}
						}
					}
					bool foundName = false;
					for (uint8 i = 0; i < MAX_LOCALE; ++i)
					{
						if (*linkedSpell->SpellName[i] && strcmp(linkedSpell->SpellName[i], buffer) == 0)
						{
							foundName = true;
							break;
						}
					}
					if (!foundName)
						return false;
				}
				else if (linkedQuest)
				{
					if (linkedQuest->GetTitle() != buffer)
					{
						QuestLocale const* ql = sObjectMgr.GetQuestLocale(linkedQuest->GetQuestId());

						if (!ql)
						{
							DEBUG_LOG("ChatHandler::isValidChatMessage default questname didn't match and there is no locale");
							return false;
						}

						bool foundName = false;
						for (uint8 i = 0; i < ql->Title.size(); ++i)
						{
							if (ql->Title[i] == buffer)
							{
								foundName = true;
								break;
							}
						}
						if (!foundName)
						{
							DEBUG_LOG("ChatHandler::isValidChatMessage no quest locale title matched");
							return false;
						}
					}
				}
				else if (linkedItem)
				{
					char* const* suffix = itemSuffix ? itemSuffix->nameSuffix : (itemProperty ? itemProperty->nameSuffix : nullptr);

					std::string expectedName = std::string(linkedItem->Name1);
					if (suffix)
					{
						expectedName += " ";
						expectedName += suffix[LOCALE_enUS];
					}

					if (expectedName != buffer)
					{
						ItemLocale const* il = sObjectMgr.GetItemLocale(linkedItem->ItemId);

						bool foundName = false;
						for (uint8 i = LOCALE_koKR; i < MAX_LOCALE; ++i)
						{
							int8 dbIndex = sObjectMgr.GetIndexForLocale(LocaleConstant(i));
							if (dbIndex == -1 || il == nullptr || (size_t)dbIndex >= il->Name.size())
								// using strange database/client combinations can lead to this case
								expectedName = linkedItem->Name1;
							else
								expectedName = il->Name[dbIndex];
							if (suffix)
							{
								expectedName += " ";
								expectedName += suffix[i];
							}
							if (expectedName == buffer)
							{
								foundName = true;
								break;
							}
						}
						if (!foundName)
						{
							DEBUG_LOG("ChatHandler::isValidChatMessage linked item name wasn't found in any localization");
							return false;
						}
					}
				}
				else if (linkedAchievement)
				{
					bool foundName = false;
					for (uint8 i = 0; i < MAX_LOCALE; ++i)
					{
						if (*linkedAchievement->name[i] && strcmp(linkedAchievement->name[i], buffer) == 0)
						{
							foundName = true;
							break;
						}
					}
					if (!foundName)
						return false;
				}
				// that place should never be reached - if nothing linked has been set in |H
				// it will return false before
				else
					return false;
			}
			break;
		case 'r':
		case '|':
			// no further payload
			break;
		default:
			DEBUG_LOG("ChatHandler::isValidChatMessage got invalid command |%c", commandChar);
			return false;
		}
	}

	// check if every opened sequence was also closed properly
	if (validSequence != validSequenceIterator)
		DEBUG_LOG("ChatHandler::isValidChatMessage EOF in active sequence");

	return validSequence == validSequenceIterator;
}

void ChatHandler::BuildChatPacket(WorldPacket& data, ChatMsg msgtype, char const* message, Language language /*= LANG_UNIVERSAL*/, ChatTagFlags chatTag /*= CHAT_TAG_NONE*/,
	ObjectGuid const& senderGuid /*= ObjectGuid()*/, char const* senderName /*= nullptr*/,
	ObjectGuid const& targetGuid /*= ObjectGuid()*/, char const* targetName /*= nullptr*/,
	char const* channelName /*= nullptr*/, uint32 achievementId /*= 0*/)
{
	const bool isGM = !!(chatTag & CHAT_TAG_GM);
	bool isAchievement = false;

	data.Initialize(isGM ? SMSG_GM_MESSAGECHAT : SMSG_MESSAGECHAT);
	data << uint8(msgtype);
	data << uint32(language);
	data << ObjectGuid(senderGuid);
	data << uint32(0);                                              // 2.1.0

	switch (msgtype)
	{
	case CHAT_MSG_MONSTER_SAY:
	case CHAT_MSG_MONSTER_PARTY:
	case CHAT_MSG_MONSTER_YELL:
	case CHAT_MSG_MONSTER_WHISPER:
	case CHAT_MSG_MONSTER_EMOTE:
	case CHAT_MSG_RAID_BOSS_WHISPER:
	case CHAT_MSG_RAID_BOSS_EMOTE:
	case CHAT_MSG_BATTLENET:
	case CHAT_MSG_WHISPER_FOREIGN:
		MANGOS_ASSERT(senderName);
		data << uint32(strlen(senderName) + 1);
		data << senderName;
		data << ObjectGuid(targetGuid);                         // Unit Target
		if (targetGuid && !targetGuid.IsPlayer() && !targetGuid.IsPet() && (msgtype != CHAT_MSG_WHISPER_FOREIGN))
		{
			data << uint32(strlen(targetName) + 1);             // target name length
			data << targetName;                                 // target name
		}
		break;
	case CHAT_MSG_BG_SYSTEM_NEUTRAL:
	case CHAT_MSG_BG_SYSTEM_ALLIANCE:
	case CHAT_MSG_BG_SYSTEM_HORDE:
		data << ObjectGuid(targetGuid);                         // Unit Target
		if (targetGuid && !targetGuid.IsPlayer())
		{
			MANGOS_ASSERT(targetName);
			data << uint32(strlen(targetName) + 1);             // target name length
			data << targetName;                                 // target name
		}
		break;
	case CHAT_MSG_ACHIEVEMENT:
	case CHAT_MSG_GUILD_ACHIEVEMENT:
		data << ObjectGuid(targetGuid);                         // Unit Target
		isAchievement = true;
		break;
	default:
		if (isGM)
		{
			MANGOS_ASSERT(senderName);
			data << uint32(strlen(senderName) + 1);
			data << senderName;
		}

		if (msgtype == CHAT_MSG_CHANNEL)
		{
			MANGOS_ASSERT(channelName);
			data << channelName;
		}
		data << ObjectGuid(targetGuid);
		break;
	}
	MANGOS_ASSERT(message);
	data << uint32(strlen(message) + 1);
	data << message;
	data << uint8(chatTag);

	if (isAchievement)
		data << uint32(achievementId);
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

void ChatHandler::SystemMessage(int32 entry, ...)
{
	const char *format = m_session->GetMangosString(entry);
	va_list ap;
	char str[1024];
	va_start(ap, entry);
	vsnprintf(str, 1024, format, ap);
	va_end(ap);
	SystemMessage(str);
}

void ChatHandler::SystemMessage(const char* message, ...)
{
	if (!message) return;
	va_list ap;
	va_start(ap, message);
	char msg1[1024];
	vsnprintf(msg1, 1024, message, ap);
	WorldPacket data;
	ChatHandler::BuildChatPacket(data, CHAT_MSG_SYSTEM, msg1, LANG_UNIVERSAL, CHAT_TAG_NONE, m_session->GetPlayer()->GetObjectGuid());
	m_session->SendPacket(data);
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
	WorldPacket data;
	ChatHandler::BuildChatPacket(data, CHAT_MSG_SYSTEM, msg, LANG_UNIVERSAL, CHAT_TAG_NONE, m_session->GetPlayer()->GetObjectGuid());
	m_session->SendPacket(data);
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
	WorldPacket data;
	ChatHandler::BuildChatPacket(data, CHAT_MSG_SYSTEM, msg, LANG_UNIVERSAL, CHAT_TAG_NONE, m_session->GetPlayer()->GetObjectGuid());
	m_session->SendPacket(data);
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
	WorldPacket data;
	ChatHandler::BuildChatPacket(data, CHAT_MSG_SYSTEM, msg, LANG_UNIVERSAL, CHAT_TAG_NONE, m_session->GetPlayer()->GetObjectGuid());
	m_session->SendPacket(data);
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
	WorldPacket data;
	ChatHandler::BuildChatPacket(data, CHAT_MSG_SYSTEM, msg, LANG_UNIVERSAL, CHAT_TAG_NONE, m_session->GetPlayer()->GetObjectGuid());
	m_session->SendPacket(data);
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
			ChatHandler(plr).GreenSystemMessage("%s set your %s to %d/%d.", m_session->GetPlayer()->GetName(), fieldname, av, mv);
		}
		else
		{
			BlueSystemMessage("You set the %s of %s to %d.", fieldname, plr->GetName(), av);
			ChatHandler(plr).GreenSystemMessage("%s set your %s to %d.", m_session->GetPlayer()->GetName(), fieldname, av);
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
			ChatHandler(plr).GreenSystemMessage("%s set your %s to %.1f/%.1f.", m_session->GetPlayer()->GetName(), fieldname, av, mv);
		}
		else
		{
			BlueSystemMessage("You set the %s of %s to %.1f.", fieldname, plr->GetName(), av);
			ChatHandler(plr).GreenSystemMessage("%s set your %s to %.1f.", m_session->GetPlayer()->GetName(), fieldname, av);
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

uint16 ChatHandler::GetItemIDFromLink(const char* link, uint32* itemid)
{
	if (link == NULL)
	{
		*itemid = 0;
		return 0;
	}

	uint16 slen = (uint16)strlen(link);
	const char* ptr = strstr(link, "|Hitem:");
	if (ptr == NULL)
	{
		*itemid = 0;
		return slen;
	}

	ptr += 7;
	*itemid = atoi(ptr);

	ptr = strstr(link, "|r");
	if (ptr == NULL)
	{
		*itemid = 0;
		return slen;
	}

	ptr += 2;
	return (ptr - link) & 0x0000ffff;
}

int32 ChatHandler::GetSpellIDFromLink(const char* link)
{
	if (link == NULL)
		return 0;

	const char* ptr = strstr(link, "|Hspell:");
	if (ptr == NULL)
		return 0;

	return atol(ptr + 8);
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