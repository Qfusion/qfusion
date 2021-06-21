#pragma once

#ifndef __ASUI_SCHEDULED_H__
#define __ASUI_SCHEDULED_H__

#include "as/asui_local.h"

namespace ASUI
{
class FunctionCallScheduler;

// Intervalled or timeouted function

class ScheduledFunction
{
public:
	friend class FunctionCallScheduler;

	ScheduledFunction( void );
	ScheduledFunction( asIScriptFunction *func, unsigned int _delay, bool _intervalled, CScriptAnyInterface *any, FunctionCallScheduler *_sched );
	~ScheduledFunction();

	// returns false if function should be removed
	bool run();

private:
	// TODO: additional parameter?
	ASBind::FunctionPtr<bool()> funcPtr;
	ASBind::FunctionPtr<bool( CScriptAnyInterface *)> funcPtr2;
	CScriptAnyInterface *any;

	FunctionCallScheduler *sched;
	int64_t start;
	unsigned delay;
	bool intervalled;
};

// Actual scheduler

class FunctionCallScheduler
{
public:
	friend class ScheduledFunction;

	FunctionCallScheduler( ASInterface *asmodule = 0 );
	~FunctionCallScheduler( void );

	// runs all active functions
	void init( ASInterface *as );
	void update( void );
	void shutdown( void  );

	int setTimeout( asIScriptFunction *func, unsigned int ms );
	int setTimeout( asIScriptFunction *func, unsigned int ms, CScriptAnyInterface &any );

	int setInterval( asIScriptFunction *func, unsigned int ms );
	int setInterval( asIScriptFunction *func, unsigned int ms, CScriptAnyInterface &any );

	void clearTimeout( int id );
	void clearInterval( int id );

	ASInterface *getAS() { return asmodule; }

	// this is merely a typedef'ed protopype function for AS funcdef binding
	static bool ASFuncdef() { return false; }
	static bool ASFuncdef2( CScriptAnyInterface &any ) { return false; }

private:
	void removeFunction( int id );

	ASInterface *asmodule;
	int counter;

	typedef std::map<int, ScheduledFunction *>  FunctionMap;
	FunctionMap functions;

};
}

ASBIND_TYPE( ASUI::FunctionCallScheduler, FunctionCallScheduler )

#endif
