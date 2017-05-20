/*
Copyright (C) 2009-2010 Chasseur de bots

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

// Do not create instances of this class dynamically, give each client a permanent global instance
abstract class AIScriptGoal
{
	AIGoal @nativeGoal;

	AIScriptGoal()
	{	
		@this.nativeGoal = null;
	}

	AIScriptGoal @reuseWith( AIGoal @nativeGoal )
	{
		@this.nativeGoal = nativeGoal;
		return this;
	}
	
	// Override this method in a subclass
	float getWeight( const AIWorldState &worldState )
	{
		return 0.0f;
	}

	// Override this method in a subclass
	void getDesiredWorldState( AIWorldState &worldState )
	{
		worldState.setIgnoreAll( true );
	}
}

abstract class AIScriptGoalFactory 
{
	// Override this method in a subclass
	// Do not access nativeGoal fields (it is not constructed at the moment of this call), use owner instead!
	AIScriptGoal @instantiateGoal( Entity &owner, AIGoal &nativeGoal )
	{
		return null;
	}
}

// Do not create instances of this class dynamically, give each client a permanent global instance.
abstract class AIScriptAction
{
	AIAction @nativeAction;

	AIScriptAction()
	{
		@this.nativeAction = null;
	}

	AIScriptAction @reuseWith( AIAction @nativeAction )
	{
		@this.nativeAction = nativeAction;
		return this;
	}

	// Override this method in a subclass
	AIPlannerNode @tryApply( const AIWorldState &worldState )
	{
		return null;
	}

	// Do not use the underlying call directly due to unsafe usage pattern, use this wrapper instread
	AIPlannerNode @newNodeForRecord( AIScriptActionRecord &record, float cost, const AIWorldState &worldState )
	{
		return nativeAction.newNodeForRecord( any( @record ), cost, worldState );
	}

	void debug( const String &in message )
	{
		this.nativeAction.debug( message );
	}
}

abstract class AIScriptActionFactory 
{
	// Override this method in a subclass
	// Do not access nativeAction fields (it is not constructed at the movement of this call), use owner instead! 
	AIScriptAction @instantiateAction( Entity &owner, AIAction &nativeAction )
	{
		return null;
	}
}

abstract class AIScriptActionRecordsPool
{
	// Override this method in a subclass
	uint getSize() const { return 0; }

	// Override this method in a subclass
	AIScriptActionRecord @itemAt( uint index ) { return null; }

	AIScriptActionRecord @free;

	const String @name;

	AIScriptActionRecordsPool( const String &in name )
	{
		@this.name = @name;
	}

	void clear() final
	{
		uint size = getSize();
		for ( uint i = 0; i < size - 1; ++i )
			@itemAt( i ).next = @itemAt( i + 1 );

		@itemAt( size - 1 ).next = null;
		@free = @itemAt( 0 );
	}

	AIScriptActionRecord @unlinkFree() final
	{
		if ( @free == null )
		{
			debug("Can't allocate an action record (there is no free records left)");
			return null;
		}

		AIScriptActionRecord @result = @free;
		@free = @free.next;
		return result;
	}

	void linkFree( AIScriptActionRecord @record ) final 
	{
		@record.next = @free;
		@record.nativeRecord = null;
		@free = @record;
	}

	void debug( const String &in message ) const final
	{
		G_Print( this.name + ": " + message + "\n" );
	}
}

// Do not create instances of this class dynamically, use a freelist-based pool
abstract class AIScriptActionRecord
{
	AIScriptActionRecordsPool @pool;
	
	AIScriptActionRecord @next;
	AIActionRecord @nativeRecord;

	AIScriptActionRecord( AIScriptActionRecordsPool &pool )
	{
		@this.pool = @pool;

		@this.next = null;		
		@this.nativeRecord = null;
	}

	// Override this method in a subclass
	void activate() {}

	// Override this method in a subclass
	void deactivate() {}

	void deleteSelf() final
	{
		pool.linkFree( this );
	}

	// Override this method in a subclass
	action_record_status_e checkStatus( const AIWorldState &currWorldState ) const
	{
		return AI_ACTION_RECORD_STATUS_INVALID;
	}

	void debug(const String &in message) const final
	{
		this.nativeRecord.debug( message );
	}
}

// Do not access nativeGoal fields (it is not constructed at the moment of this call), use owner instead!
AIScriptGoal @GENERIC_InstantiateGoal( AIScriptGoalFactory &factory, Entity &owner, AIGoal &nativeGoal )
{
	return factory.instantiateGoal( owner, nativeGoal );
}

// Do not access nativeAction fields (it is not contructed at the moment of this call), use owner instead!
AIScriptAction @GENERIC_InstantiateAction( AIScriptActionFactory &factory, Entity &owner, AIAction &nativeAction )
{
	return factory.instantiateAction( owner, nativeAction );
}

void GENERIC_ActivateScriptActionRecord( AIScriptActionRecord &record )
{
	record.activate();
}

void GENERIC_DeactivateScriptActionRecord( AIScriptActionRecord &record )
{
	record.deactivate();
}

void GENERIC_DeleteScriptActionRecord( AIScriptActionRecord &record )
{
	record.deleteSelf();
}

int GENERIC_CheckScriptActionRecordStatus( AIScriptActionRecord &record, const AIWorldState &currWorldState )
{
	return int( record.checkStatus( currWorldState ) );
}

float GENERIC_GetScriptGoalWeight( AIScriptGoal &goal, const AIWorldState &currWorldState )
{
	return goal.getWeight( currWorldState );
}

void GENERIC_GetScriptGoalDesiredWorldState( AIScriptGoal &goal, AIWorldState &worldState )
{
	goal.getDesiredWorldState( worldState );
}

AIPlannerNode @GENERIC_TryApplyScriptAction( AIScriptAction &action, const AIWorldState &worldState )
{
	return action.tryApply( worldState );
}

// Do not use functions exposed by script api directly due to unsafe usage patterns, use these wrappers

void GENERIC_RegisterScriptGoal( const String &name, const AIScriptGoalFactory &factory )
{
	AI::RegisterScriptGoal( name, any( @factory ) );
}

void GENERIC_RegisterScriptAction( const String &name, const AIScriptActionFactory &factory )
{
	AI::RegisterScriptAction( name, any( @factory ) );
} 

// The underlying call is not unsafe but should used for uniformity
void GENERIC_AddApplicableAction( const String &goalName, const String &actionName )
{
	AI::AddApplicableAction( goalName, actionName );
}

abstract class AIScriptWorldStateAttachment
{
	AIScriptWorldStateAttachment @next;

	AIScriptWorldStateAttachment() 
	{
		@next = null;
	}

	void setIgnoreAll( bool ignore ) {}

	uint hash() const 
	{
		return 0;
	}

	bool opEquals( const AIScriptWorldStateAttachment &that ) const
	{
		return false;
	}

	bool isSatisfiedBy( const AIScriptWorldStateAttachment &that ) const
	{
		return false;
	}

	void debugPrint() const {}

	void debugPrintDiff( const AIScriptWorldStateAttachment &that ) const {}
}

void GENERIC_SetScriptWorldStateAttachmentIgnoreAllVars( any @attachment, bool ignore )
{
	AIScriptWorldStateAttachment @value;
	if ( !attachment.retrieve( value ) )
		G_Error( "GENERIC_SetScriptWorldStateAttachmentIgnoreAllVars(): `attachment` argument contains a value of an illegal type\n" );

	value.setIgnoreAll( ignore );
}

uint GENERIC_ScriptWorldStateAttachmentHash( any @attachment )
{
	AIScriptWorldStateAttachment @value;
	if ( !attachment.retrieve( value ) )
		G_Error( "GENERIC_ScriptWorldStateAttachmentHash(): `attachment` argument contains a value of an illegal type\n" );
	
	return value.hash();
}

bool GENERIC_ScriptWorldStateAttachmentEquals( any @rhs, any @lhs )
{
	AIScriptWorldStateAttachment @lhsValue;
	AIScriptWorldStateAttachment @rhsValue;

	if ( !lhs.retrieve( lhsValue ) )
		G_Error( "GENERIC_ScriptWorldStateAttachmentEquals(): `lhs` argument contains a value of an illegal type\n" );

	if ( !rhs.retrieve( lhsValue ) )
		G_Error( "GENERIC_ScriptWorldStateAttachmentEquals(): `rhs` argument contains a value of an illegal type\n" );

	return lhsValue == rhsValue;
}

bool GENERIC_IsScriptWorldStateAttachmentSatisfiedBy( any @rhs, any @lhs )
{
	AIScriptWorldStateAttachment @lhsValue;
	AIScriptWorldStateAttachment @rhsValue;

	if ( !lhs.retrieve( lhsValue ) )
		G_Error( "GENERIC_IsScriptWorldStateAttachmentSatisfiedBy(): `lhs` argument contains a value of an illegal type\n" );

	if ( !rhs.retrieve( lhsValue ) )
		G_Error( "GENERIC_IsScriptWorldStateAttachmentSatisfiedBy(): `rhs` argument contains a value of an illegal type\n" );

	return lhsValue.isSatisfiedBy( rhsValue );
}

void GENERIC_DebugPrintScriptWorldStateAttachment( any @attachment )
{
	AIScriptWorldStateAttachment @value;
	if ( !attachment.retrieve( value ) )
		G_Error( "GENERIC_DebugPrintScriptWorldState(): `attachment` argument contains a value of an illegal type\n" );

	value.debugPrint();
}

void GENERIC_DebugPrintScriptWorldStateAttachmentDiff( any @lhs, any @rhs )
{
	AIScriptWorldStateAttachment @lhsValue;
	AIScriptWorldStateAttachment @rhsValue;

	if ( !lhs.retrieve( lhsValue ) )
		G_Error( "GENERIC_DebugPrintScriptWorldStateAttachmentDiff(): `lhs` argument contains a value of an illegal type\n" );

	if ( !rhs.retrieve( lhsValue ) )
		G_Error( "GENERIC_DebugPrintScriptWorldStateAttachmentDiff(): `rhs` argument contains a value of an illegal type\n" );

	lhsValue.debugPrintDiff( rhsValue );
}

class AIScriptWeightConfigVarGroup
{
	AIScriptWeightConfigVarGroup @nextSibling;
	const String @name;

    AIScriptWeightConfigVar @childVarsHead;
	AIScriptWeightConfigVarGroup @childGroupsHead;


	AIScriptWeightConfigVarGroup( AIScriptWeightConfigVarGroup @parent, const String @name )
	{
		@this.nextSibling = null;
		@this.name = @name;
		@this.childVarsHead = null;
		@this.childGroupsHead = null;

		if ( @parent != null )
			parent.registerGroup( this );
	}

	void registerVar( AIScriptWeightConfigVar @var )
	{
		@var.nextSibling = @this.childVarsHead;
		@this.childVarsHead = @var;
	}

	void registerGroup( AIScriptWeightConfigVarGroup @group )
	{
		@group.nextSibling = @this.childGroupsHead;
		@this.childGroupsHead = @group;
	}
}

class AIScriptWeightConfigVar
{
	AIScriptWeightConfigVar @nextSibling;
	const String @name;
	float value;
	float minValue;
	float maxValue;
	float defaultValue;

	AIScriptWeightConfigVar( AIScriptWeightConfigVarGroup @parent, const String @name )
	{
		@this.nextSibling = null;
		@this.name = @name;

		if ( @parent != null )
			parent.registerVar( this );
	}

	void getValueProps( float &out value, float &out minValue, float &out maxValue, float &out defaultValue )
	{
		value = this.value;
		minValue = this.minValue;
		maxValue = this.maxValue;
		defaultValue = this.defaultValue;
	}

	void setValue( float value )
	{
		this.value = value;
	}
}

void GENERIC_GetScriptWeightConfigVarValueProps( AIScriptWeightConfigVar @var, float &out value, float &out minValue, float &out maxValue, float &out defaultValue)
{
	var.getValueProps( value, minValue, maxValue, defaultValue );
}

void GENERIC_SetScriptWeightConfigVarValue( AIScriptWeightConfigVar @var, float value )
{
	var.setValue( value );
}
