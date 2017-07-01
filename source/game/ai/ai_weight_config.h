#ifndef QFUSION_AI_WEIGHT_CONFIG_H
#define QFUSION_AI_WEIGHT_CONFIG_H

#include <tuple>
#include "ai_local.h"
#include "static_vector.h"

void GT_asRegisterScriptWeightConfig( class AiWeightConfig *weightConfig, const edict_t *configOwner );
void GT_asReleaseScriptWeightConfig( class AiWeightConfig *weightConfig, const edict_t *configOwner );
void GENERIC_asGetScriptWeightConfigVarValueProps( const void *scriptObject, float *value, float *minValue, float *maxValue, float *defaultValue );
void GENERIC_asSetScriptWeightConfigVarValue( void *scriptObject, float value );

class AiBaseWeigthConfigVarGroup;
class AiBaseWeightConfigVar;
class AiScriptWeightConfigVar;
class AiScriptWeightConfigVarGroup;

// This accepts the exact type as as template parameter to make accessors operate on the exact child type.
template <typename Child>
class AiWeightConfigVarGroupChild
{
	friend class AiBaseWeightConfigVarGroup;
	friend class AiBaseWeightConfigVar;

protected:
	const char *name;
	Child *nextSibling;
	Child *nextInHashBin;
	unsigned nameHash;
	unsigned nameLength : 31;
	bool isTouched : 1;

	AiWeightConfigVarGroupChild( const char *name_ )
		: name( name_ ),
		nextSibling( nullptr ),
		nextInHashBin( nullptr ) {
		unsigned length;
		GetHashAndLength( name_, &nameHash, &length );
		nameLength = length;
		isTouched = false;
	}

public:
	// Exposed for script interface
	const char *Name() const { return name; }
	Child *Next() { return nextSibling; }
	const Child *Next() const { return nextSibling; }
	unsigned NameHash() const { return nameHash; }
	unsigned NameLength() const { return nameLength; }
};

class AiBaseWeightConfigVarGroup : public AiWeightConfigVarGroupChild<AiBaseWeightConfigVarGroup>
{
	friend class AiBaseWeightConfigVar;

	template <typename T>
	void LinkItem( T *item, T **linkedItemsHead, T ***hashBins, unsigned *numItems );

	template <typename T>
	void AddItemToHashBin( T *item, T ***hashBins );

	template <typename T>
	T *GetItemByName( const char *name_, unsigned nameHash_, T *linkedItemsHead, T **hashBins, unsigned numItems );

	template <typename T>
	T *GetItemByPath( const char *path, T * ( AiBaseWeightConfigVarGroup::*getByNameMethod )( const char *, unsigned ) );

	template <typename T>
	void AddScriptItem( const char *name_, void *scriptObject, T **allocatedItemsHead );

protected:
	AiBaseWeightConfigVarGroup *childGroupsHead;
	AiBaseWeightConfigVar *childVarsHead;

	// If there are few child items, hash bins are not allocated and a corresponding list should be used for searching.
	// We use a hash and not a trie because its a natural arppoach for already linked node-like items.
	AiBaseWeightConfigVarGroup **groupsHashBins;
	AiBaseWeightConfigVar **varsHashBins;

	// Vars and groups defined and used in native code are intended
	// to be declared as members of this class subtypes, so no extra memory management is nessessary.
	// Vars and groups that correspond to their script counterparts are added dynamically by gametype scripts.
	// Thus we have to allocate these vars via G_Malloc() and link to these lists for further freeing via G_Free().
	// Do not bother about performance because this is done only
	// when bot enters the game and sane count of script groups/vars is low.
	AiScriptWeightConfigVarGroup *allocatedGroupsHead;
	AiScriptWeightConfigVar *allocatedVarsHead;

	// 4 bytes are likely to be lost due to alignment of consequent instances of this class
	unsigned numChildVars;
	unsigned numChildGroups;

	// This should be enough for any sane configs.
	static constexpr const auto NUM_HASH_BINS = 59;
	static constexpr const auto MIN_HASH_ITEMS = 4;

	void LinkGroup( AiBaseWeightConfigVarGroup *childGroup );
	void LinkVar( AiBaseWeightConfigVar *childVar );

	bool Parse( const char *data, const char **restOfTheData );
	int ParseNextEntry( const char *data, const char **nextData );
	bool Write( int fileHandle, int depth ) const;

public:
	inline AiBaseWeightConfigVarGroup( AiBaseWeightConfigVarGroup *parent, const char *name_ )
		: AiWeightConfigVarGroupChild( name_ ),
		childGroupsHead( nullptr ),
		childVarsHead( nullptr ),
		groupsHashBins( nullptr ),
		varsHashBins( nullptr ),
		allocatedGroupsHead( nullptr ),
		allocatedVarsHead( nullptr ),
		numChildVars( 0 ),
		numChildGroups( 0 ) {
		if( parent ) {
			parent->LinkGroup( this );
		}
	}

	virtual ~AiBaseWeightConfigVarGroup();

	// Exposed for script interface.
	AiBaseWeightConfigVarGroup *GetGroupByName( const char *name_, unsigned nameHash_ = 0 );
	AiBaseWeightConfigVar *GetVarByName( const char *name_, unsigned nameHash_ = 0 );

	inline AiBaseWeightConfigVarGroup *GroupsListHead() { return childGroupsHead; }
	inline const AiBaseWeightConfigVarGroup *GroupsListHead() const { return childGroupsHead; }
	inline AiBaseWeightConfigVar *VarsListHead() { return childVarsHead; }
	inline const AiBaseWeightConfigVar *VarsListHead() const { return childVarsHead; }

	// Note: groups and vars do not share namespace
	virtual AiBaseWeightConfigVarGroup *GetGroupByPath( const char *path );
	virtual AiBaseWeightConfigVar *GetVarByPath( const char *path );

	void AddScriptVar( const char *name_, void *scriptObject );
	void AddScriptGroup( const char *name_, void *scriptObject );

	void ResetToDefaultValues();

	void CheckTouched( const char *parentName = nullptr );
	void Touch( const char *parentName = nullptr );

	void CopyValues( const AiBaseWeightConfigVarGroup &that );

	bool operator==( const AiBaseWeightConfigVarGroup &that ) const;
	inline bool operator!=( const AiBaseWeightConfigVarGroup &that ) const { return !( *this == that ); }
};

inline char *G_Strdup( const char *str ) {
	auto len = strlen( str );
	char *mem = (char *)G_Malloc( len + 1 );
	memcpy( mem, str, len + 1 );
	return mem;
}

class AiNativeWeightConfigVarGroup : public AiBaseWeightConfigVarGroup
{
public:
	AiNativeWeightConfigVarGroup( AiBaseWeightConfigVarGroup *parent, const char *name_ )
		: AiBaseWeightConfigVarGroup( parent, name_ ) {}
};

class AiScriptWeightConfigVarGroup : public AiBaseWeightConfigVarGroup
{
	friend class AiBaseWeightConfigVarGroup;
	void *scriptObject;
	AiScriptWeightConfigVarGroup *nextAllocated;

public:
	AiScriptWeightConfigVarGroup( AiBaseWeightConfigVarGroup *parent, const char *name_, void *scriptObject_ )
		: AiBaseWeightConfigVarGroup( parent, G_Strdup( name_ ) ),
		scriptObject( scriptObject_ ),
		nextAllocated( nullptr ) {}

	~AiScriptWeightConfigVarGroup() {
		G_Free( const_cast<char *>( name ) );
	}

	AiScriptWeightConfigVarGroup( const AiScriptWeightConfigVarGroup &that ) = delete;
	const AiScriptWeightConfigVarGroup &operator=( const AiScriptWeightConfigVarGroup &that ) = delete;
	AiScriptWeightConfigVarGroup( AiScriptWeightConfigVarGroup &&that ) = delete;
	inline AiScriptWeightConfigVarGroup &operator=( AiScriptWeightConfigVarGroup &&that ) = delete;
};

class AiBaseWeightConfigVar : public AiWeightConfigVarGroupChild<AiBaseWeightConfigVar>
{
	friend class AiBaseWeightConfigVarGroup;

public:
	inline AiBaseWeightConfigVar( AiBaseWeightConfigVarGroup *parent, const char *name_ )
		: AiWeightConfigVarGroupChild( name_ ) {
		if( parent ) {
			parent->LinkVar( this );
		}
	}

	virtual ~AiBaseWeightConfigVar() {}

	virtual void GetValueProps( float *value_, float *minValue_, float *maxValue_, float *defaultValue_ ) const = 0;
	virtual void SetValue( float value_ ) = 0;

	inline void ResetToDefaultValues() {
		float value, minValue, maxValue, defaultValue;
		GetValueProps( &value, &minValue, &maxValue, &defaultValue );
		SetValue( defaultValue );
		isTouched = false;
	}

	inline void Touch( const char *parentName = nullptr ) {
		// TODO: Show full name somehow?
		if( isTouched ) {
			if( parentName ) {
				G_Printf( S_COLOR_YELLOW "WARNING: var %s in group %s is already touched\n", name, parentName );
			} else {
				G_Printf( S_COLOR_YELLOW "WARNING: var %s has been already touched\n", name );
			}
		}
		isTouched = true;
	}

	inline void CheckTouched( const char *parentName = nullptr ) {
		if( !isTouched ) {
			if( parentName ) {
				G_Printf( S_COLOR_YELLOW "WARNING: var %s in group %s has not been touched\n", name, parentName );
			} else {
				G_Printf( S_COLOR_YELLOW "WARNING: var %s has not been touched\n", name );
			}
		}
		isTouched = false;
	}

	inline bool operator==( const AiBaseWeightConfigVar &that ) const {
		float thisValue, thatValue;
		float dummy[3];
		this->GetValueProps( &thisValue, dummy + 0, dummy + 1, dummy + 2 );
		that.GetValueProps( &thatValue, dummy + 0, dummy + 1, dummy + 2 );
		return fabs( (double)thisValue - (double)thatValue ) < 0.000001;
	}

	inline bool operator!=( const AiBaseWeightConfigVar &that ) const { return !( *this == that ); }
};

// We do not want to even mention script weight config vars in the native code.
// The only operation needed is to set a var value, and this can be done by name.
class AiNativeWeightConfigVar : AiBaseWeightConfigVar
{
	float minValue;
	float maxValue;
	float defaultValue;
	float value;

public:
	AiNativeWeightConfigVar( AiBaseWeightConfigVarGroup *parent,
							 const char *name_,
							 float minValue_,
							 float maxValue_,
							 float defaultValue_ )
		: AiBaseWeightConfigVar( parent, name_ ),
		minValue( minValue_ ),
		maxValue( maxValue_ ),
		defaultValue( defaultValue_ ),
		value( defaultValue_ ) {
#ifdef _DEBUG
		if( minValue >= maxValue ) {
			AI_FailWith( "AiNativeWeightConfigVar()", "%s: minValue %f >= maxValue %f\n", name, minValue, maxValue );
		}
		if( defaultValue < minValue ) {
			AI_FailWith( "AiNativeWeightConfigVar()", "%s: defaultValue %f < minValue %f\n", name, defaultValue, minValue );
		}
		if( defaultValue > maxValue ) {
			AI_FailWith( "AiNativeWeightConfigVar()", "%s: defaultValue %f > maxValue %f\n", name, defaultValue, maxValue );
		}
#endif
	}

	inline operator float() const { return value; }
	inline float MinValue() const { return minValue; }
	inline float MaxValue() const { return maxValue; }
	inline float DefaultValue() const { return defaultValue; }

	void GetValueProps( float *value_, float *minValue_, float *maxValue_, float *defaultValue_ ) const override {
		*value_ = this->value;
		*minValue_ = this->minValue;
		*maxValue_ = this->maxValue;
		*defaultValue_ = this->defaultValue;
	}
	void SetValue( float value_ ) override {
		this->value = value_;
	}
};

class AiScriptWeightConfigVar : public AiBaseWeightConfigVar
{
	friend class AiBaseWeightConfigVarGroup;
	void *scriptObject;
	AiScriptWeightConfigVar *nextAllocated;

public:
	AiScriptWeightConfigVar( AiBaseWeightConfigVarGroup *parent, const char *name_, void *scriptObject_ )
		: AiBaseWeightConfigVar( parent, G_Strdup( name_ ) ), scriptObject( scriptObject_ ), nextAllocated( nullptr ) {}

	~AiScriptWeightConfigVar() {
		G_Free( const_cast<char *>( name ) );
	}

	AiScriptWeightConfigVar( const AiScriptWeightConfigVar &that ) = delete;
	AiScriptWeightConfigVar &operator=( const AiScriptWeightConfigVar &that ) = delete;
	AiScriptWeightConfigVar( AiScriptWeightConfigVar &&that ) = delete;
	AiScriptWeightConfigVar &operator=( AiScriptWeightConfigVar &&that ) = delete;

	void GetValueProps( float *value, float *minValue, float *maxValue, float *defaultValue ) const override {
		GENERIC_asGetScriptWeightConfigVarValueProps( scriptObject, value, minValue, maxValue, defaultValue );
	}
	void SetValue( float value ) override {
		GENERIC_asSetScriptWeightConfigVarValue( scriptObject, value );
	}
};

class AiWeightConfig : protected AiBaseWeightConfigVarGroup
{
	const edict_t *owner;
	bool isRegisteredInScript;

	bool LoadFromData( const char *data );

	inline const char *SkipRootInPath( const char *path ) const;

protected:
	inline const AiBaseWeightConfigVarGroup *Root() const { return this; }
	inline AiBaseWeightConfigVarGroup *Root() { return this; }

	// Must be called in child constructor after all child native objects have been constructed
	void RegisterInScript() {
		GT_asRegisterScriptWeightConfig( this, owner );
		isRegisteredInScript = true;
	}

	AiBaseWeightConfigVarGroup *GetGroupByPath( const char *path ) override;
	AiBaseWeightConfigVar *GetVarByPath( const char *path ) override;

public:
	AiWeightConfig( const edict_t *owner_ )
		: AiBaseWeightConfigVarGroup( nullptr, "Weights" ),
		owner( owner_ ),
		isRegisteredInScript( false ) {}

	~AiWeightConfig() {
		if( isRegisteredInScript ) {
			GT_asReleaseScriptWeightConfig( this, owner );
		}
	}

	bool Load( const char *filename );
	bool Save( const char *filename );

	inline bool Save( int fileHandle ) {
		return AiBaseWeightConfigVarGroup::Write( fileHandle, 0 );
	}

	using AiBaseWeightConfigVarGroup::ResetToDefaultValues;

	// We have to do these wrappers since AiBaseWeightConfigVarGroup is not a public type of `this` (and `that`)
	inline void CopyValues( const AiWeightConfig &that ) {
		AiBaseWeightConfigVarGroup::CopyValues( that );
	}

	inline bool operator==( const AiWeightConfig &that ) { return AiBaseWeightConfigVarGroup::operator==( that ); }
	inline bool operator!=( const AiWeightConfig &that ) { return AiBaseWeightConfigVarGroup::operator!=( that ); }
};

// If we have two linked chains of items that are supposed
// to have the same length and same corresponding item names and to be iterated and checked in parallel,
// this class provides a reusable way to do it
template <typename T1, typename T2>
class ZippedItemChainsIterator
{
	T1 first;
	T2 second;
	const char *tag;

	inline void CheckMatch() {
#ifdef _DEBUG
		if( first ) {
			if( !second ) {
				AI_FailWith( tag, "A first item named %s has null counterpart\n", first->Name() );
			}

			if( first->NameHash() != second->NameHash() || Q_stricmp( first->Name(), second->Name() ) ) {
				AI_FailWith( tag, "Item names mismatch: first item name is %s, second one is %s\n", first->Name(), second->Name() );
			}
		} else if( second ) {
			AI_FailWith( tag, "A second item named %s has null counterpart\n", second->Name() );
		}
#endif
	}

public:
	inline ZippedItemChainsIterator( T1 firstChainHead, T2 secondChainHead, const char *tag_ )
		: first( firstChainHead ),
		second( secondChainHead ),
		tag( tag_ ) {
		CheckMatch();
	}

	inline void Next() {
		first = first->Next();
		second = second->Next();
		CheckMatch();
	}

	inline bool HasNext() const { return first != nullptr; };

	inline T1 First() { return first; }
	inline T2 Second() { return second; }
};

template <typename T1, typename T2>
inline static ZippedItemChainsIterator<T1, T2> ZipItemChains( T1 firstChainHead, T2 secondChainHead, const char *tag ) {
	return ZippedItemChainsIterator<T1, T2>( firstChainHead, secondChainHead, tag );
};

#endif
