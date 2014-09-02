#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"

#include "as/asui.h"
#include "as/asui_local.h"
#include "as/asui_scheduled.h"

namespace ASUI
{

ScheduledFunction::ScheduledFunction()
	: any( NULL ), sched( 0 )
{

}

ScheduledFunction::ScheduledFunction( asIScriptFunction *func, unsigned int delay, bool intervalled, CScriptAnyInterface *any, FunctionCallScheduler *sched )
	: any( any ), sched( sched ), start( trap::Milliseconds() ), delay( delay ), intervalled( intervalled )
{
	if( any ) {
		funcPtr2 = ASBind::CreateFunctionPtr( func, funcPtr2 );
		funcPtr2.addref();
		any->AddRef();
	}
	else {
		funcPtr = ASBind::CreateFunctionPtr( func, funcPtr );
		funcPtr.addref();
	}
}

ScheduledFunction::~ScheduledFunction()
{
	if( any ) {
		funcPtr2.release();
		any->Release();
	}
	else {
		funcPtr.release();
	}
}

bool ScheduledFunction::run()
{
	unsigned int now = trap::Milliseconds();

	if( funcPtr.isValid() || funcPtr2.isValid() )
	{
		// call function
		if( now >= ( start + delay ) )
		{
			bool res = intervalled;

			try {
				if( funcPtr2.isValid() ) {
					if (!funcPtr2.getModule()) {
						// the module is gone (freed, etc)
						res = false;
					}
					else {
						funcPtr2.setContext( sched->getAS()->getContext() );
						res = funcPtr2( any );
					}
				}
				else {
					if (!funcPtr.getModule ()) {
						// the module is gone (freed, etc)
						res = false;
					}
					else {
						funcPtr.setContext( sched->getAS ()->getContext () );
						res = funcPtr ();
					}
				}
			} catch( ASBind::Exception & ) {
				Com_Printf( S_COLOR_RED "SheduledFunction: Failed to call function %s\n", funcPtr.getName() );
			}

			if( !res )
				return false;	// stop this madness

			// push start time forwards
			// FIXME: push until now in case of big gaps between run()
			start += delay;
		}
	}

	// signal for keep going
	return true;
}

//============================================================

FunctionCallScheduler::FunctionCallScheduler( ASInterface *asmodule )
	: asmodule( asmodule ), counter( 0 )
{
	// assert( asmodule != 0 );
}

FunctionCallScheduler::~FunctionCallScheduler()
{
	functions.clear();
}

void FunctionCallScheduler::init( ASInterface *_asmodule )
{
	asmodule = _asmodule;
}

// runs all active functions
void FunctionCallScheduler::update( void )
{
	for( FunctionMap::iterator it = functions.begin(); it!= functions.end(); )
	{
		ScheduledFunction *func = it->second;
		if( !func->run() ) {
			functions.erase( it++ );
			__delete__( func );
		}
		else
			++it;
	}
}

void FunctionCallScheduler::shutdown( void )
{
	for( FunctionMap::iterator it = functions.begin(); it!= functions.end(); )
	{
		ScheduledFunction *func = it->second;
		functions.erase( it++ );
		__delete__( func );
	}
}

int FunctionCallScheduler::setTimeout( asIScriptFunction *func, unsigned int ms )
{
	functions[counter] = __new__( ScheduledFunction )( func, ms, false, NULL, this );
	if( func ) {
		func->Release();
	}
	return counter++;
}

int FunctionCallScheduler::setInterval( asIScriptFunction *func, unsigned int ms )
{
	functions[counter] = __new__( ScheduledFunction )( func, ms, true, NULL, this );
	if( func ) {
		func->Release();
	}
	return counter++;
}

int FunctionCallScheduler::setTimeout( asIScriptFunction *func, unsigned int ms, CScriptAnyInterface &any )
{
	functions[counter] = __new__( ScheduledFunction )( func, ms, false, &any, this );
	if( func ) {
		func->Release();
	}
	return counter++;
}

int FunctionCallScheduler::setInterval( asIScriptFunction *func, unsigned int ms, CScriptAnyInterface &any )
{
	functions[counter] = __new__( ScheduledFunction )( func, ms, true, &any, this );
	if( func ) {
		func->Release();
	}
	return counter++;
}

void FunctionCallScheduler::clearTimeout( int id )
{
	removeFunction( id );
}

void FunctionCallScheduler::clearInterval( int id )
{
	removeFunction( id );
}

void FunctionCallScheduler::removeFunction( int id )
{
	FunctionMap::iterator it = functions.find( id );
	if( it != functions.end() ) {
		ScheduledFunction *func = it->second;
		functions.erase( it );
		__delete__( func );
	}
}

}
