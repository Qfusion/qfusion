#pragma once
#ifndef __UI_OPTIONSFORM_H__
#define __UI_OPTIONSFORM_H__

#include <RmlUi/Controls/ElementForm.h>

namespace WSWUI
{
//================================================

class CvarStorage
{
	// ch : made the typedef public so OptionsForm can iterate the values
	// with clean types

public:
	// just the name and value as string
	typedef std::pair<std::string, std::string> CvarPair;
	typedef std::map<std::string, std::string> CvarMap;

	// ch : also added shortcut for the iterator type
	typedef CvarMap::const_iterator iterator;

private:
	CvarMap storedValues;

public:
	CvarStorage() {}
	~CvarStorage() {}

	// storage -> cvar
	void restoreValues();
	// cvar -> storage
	void storeValues();
	// drop storage
	void applyValues();
	void addCvar( const char *name );

	const CvarMap & getMap();
};

//================================================

class OptionsForm : public Rml::Controls::ElementForm
{
	CvarStorage cvars;
	Rml::Core::EventListener *cvarListener;

public:
	OptionsForm( const std::string &tag );
	~OptionsForm();

	// Rocket Form
	virtual void ProcessDefaultAction( Rml::Core::Event &ev );

	// move stored cvar values back to cvars (i.e. Cancel)
	void restoreOptions();

	// move cvar values to storage (i.e. onLoad/Show)
	void storeOptions();

	// drop the storage and use current cvar values (i.e. Submit)
	void applyOptions();
};
}

#endif
