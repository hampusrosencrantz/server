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
	Player *target = getSelectedPlayer();
	char msg[100];
	if (target)
	{
		target->bInvincible = !target->bInvincible;
		snprintf(msg, 100, "Invincibility is now %s", target->bInvincible ? "ON. You may have to leave and re-enter this zone for changes to take effect." : "OFF. Exit and re-enter this zone for this change to take effect.");
	}
	else {
		snprintf(msg, 100, "Select a player or yourself first.");
	}
	SystemMessage(msg);
	return true;
}