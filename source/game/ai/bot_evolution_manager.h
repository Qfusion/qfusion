#ifndef QFUSION_AI_EVOLUTION_MANAGER_H
#define QFUSION_AI_EVOLUTION_MANAGER_H

#include "bot_weight_config.h"

class BotEvolutionManager
{
	static BotEvolutionManager *instance;

protected:
	BotEvolutionManager() {}

public:
	virtual ~BotEvolutionManager() {};

	static void Init();
	static void Shutdown();

	static inline BotEvolutionManager *Instance() { return instance; }

	virtual void OnBotConnected( edict_t *ent ) {};
	virtual void OnBotRespawned( edict_t *ent ) {};

	virtual void SaveEvolutionResults() {};
};

#endif
