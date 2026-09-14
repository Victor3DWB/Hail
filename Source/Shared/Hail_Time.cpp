#include "Shared_PCH.h"
#include "Hail_Time.h"
#include "InternalMessageHandling/InternalMessageHandling.h"

Hail::Timer* g_globalTimer = nullptr;

void Hail::SetGlobalTimer(Timer* pTimer)
{
	H_ASSERT(!g_globalTimer, "Secondary Initialization of the global timer is not allowed.");
	g_globalTimer = pTimer;
}

Hail::Timer* Hail::GetGlobalTimer()
{
	return g_globalTimer;
}
