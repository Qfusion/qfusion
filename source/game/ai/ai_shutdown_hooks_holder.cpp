#include "ai_shutdown_hooks_holder.h"
#include "ai_local.h"

static AiShutdownHooksHolder aiShutdownHooksHolderInstance;

AiShutdownHooksHolder *AiShutdownHooksHolder::Instance() {
	return &aiShutdownHooksHolderInstance;
}

void AiShutdownHooksHolder::RegisterHook( const std::function<void(void)> &hook ) {
	untaggedHooks.push_back( hook );
}

void AiShutdownHooksHolder::RegisterHook( uint64_t tag, const std::function<void(void)> &hook ) {
	for( const auto &tagAndHook: taggedHooks ) {
		if( tagAndHook.first == tag ) {
			const char *format = "Some hook tagged by %p is already present\n";
			AI_FailWith( "AiShutdownHooksHolder::RegisterHook()", format, (const void *)tagAndHook.first );
		}
	}
	taggedHooks.push_back( std::make_pair( tag, hook ) );
}

void AiShutdownHooksHolder::UnregisterHook( uint64_t tag ) {
	for( auto it = taggedHooks.begin(), end = taggedHooks.end(); it != end; ++it ) {
		if( ( *it ).first == tag ) {
			taggedHooks.erase( it );
			return;
		}
	}

	AI_FailWith( "AiShutdownHooksHolder::UnregisterHook()", "Can't find a hook by tag %p\n", (const void *)tag );
}

void AiShutdownHooksHolder::InvokeHooks() {
	if( hooksInvoked ) {
		AI_FailWith( "AiShutdownHooksHolder::InvokeHooks()", "Hooks have been already invoked\n" );
	} else {
		for( const auto &hook: untaggedHooks )
			hook();
		for( const auto &tagAndHook: taggedHooks )
			tagAndHook.second();

		hooksInvoked = true;
	}
}

AiShutdownHooksHolder::~AiShutdownHooksHolder() {
	if( !hooksInvoked ) {
		AI_FailWith( "AiShutdownHooksHolder::~AiShutdownHooksHolder()", "Hooks have not been invoked\n" );
	}
}
