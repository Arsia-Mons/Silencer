#include "data_retrieval_mode.h"
#include "world.h"

bool DataRetrievalMode::IsMatchOver(const World& w) const {
	return w.GetWinningTeamId() != 0;
}

Uint16 DataRetrievalMode::WinningTeamId(const World& w) const {
	return w.GetWinningTeamId();
}
