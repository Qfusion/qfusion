#include "ai_frame_aware_updatable.h"

void AiFrameAwareUpdatable::Update() {
	PreFrame();
	Frame();
	if( !ShouldSkipThinkFrame() ) {
		PreThink();
		Think();
		PostThink();
	}
	PostFrame();
}