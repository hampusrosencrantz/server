#include "World/World.h"
#include "Chat/Chat.h"

bool ChatHandler::HandleResetReputationCommand(const char *args)
{
	Player *target = getSelectedPlayer();
	if (!target)
	{
		SystemMessage("Select a player or yourself first.");
		return true;
	}

	target->GetReputationMgr().SendInitialReputations();
	SystemMessage("Done. Relog for changes to take effect.");
	return true;
}

bool ChatHandler::HandleInvincibleCommand(const char *args)
{
}