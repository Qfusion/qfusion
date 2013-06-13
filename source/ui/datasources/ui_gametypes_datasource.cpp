#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_utils.h"
#include "datasources/ui_gametypes_datasource.h"

namespace
{
	struct cmp_gametypes_by_id
	{
		typedef std::pair<std::string, std::string> first_argument_type ;
		typedef std::string second_argument_type;
		typedef bool result_type;

		result_type operator()(first_argument_type lhs, second_argument_type rhs) const
		{
			return lhs.first == rhs;
		}
	};
}
namespace WSWUI
{
	GameTypesDataSource::GameTypesDataSource():Rocket::Controls::DataSource("gametypes_source")
	{
		// Gametypes will appear in the list in the same order they appear in the
		// `gameTypes' vector. We want the "stock" gametypes to appear BEFORE any
		// other gametypes, so we'll prepopulate `gameTypes' with them
		gameTypes.push_back(std::make_pair("dm", "Deathmatch"));
		gameTypes.push_back(std::make_pair("ffa", "Free for All"));
		gameTypes.push_back(std::make_pair("duel", "Duel"));
		gameTypes.push_back(std::make_pair("tdm", "Team Deathmatch"));
		gameTypes.push_back(std::make_pair("ctf", "Capture the Flag"));
		gameTypes.push_back(std::make_pair("race", "Race"));
		gameTypes.push_back(std::make_pair("ca", "Clan Arena"));
		gameTypes.push_back(std::make_pair("bomb", "Bomb and Defuse"));
		gameTypes.push_back(std::make_pair("ctftactics", "CTF: Tactics"));
		gameTypes.push_back(std::make_pair("da", "Duel Arena"));
		gameTypes.push_back(std::make_pair("headhunt", "Headhunt Deathmatch"));
		gameTypes.push_back(std::make_pair("tdo", "Team Domination"));

		std::vector<std::string> listedGameTypes;
		getFileList(listedGameTypes, "progs/gametypes", ".gt");

		for(std::vector<std::string>::const_iterator it = listedGameTypes.begin();
		    it != listedGameTypes.end(); ++it)
		{
			if(std::find_if(gameTypes.begin(), gameTypes.end(), std::bind2nd(::cmp_gametypes_by_id(), *it)) == gameTypes.end())
				gameTypes.push_back(std::make_pair(*it, *it));
		}
	}

	void GameTypesDataSource::GetRow(Rocket::Core::StringList &row, const Rocket::Core::String&, int row_index, const Rocket::Core::StringList& cols)
	{
		if(row_index < 0 || (size_t)row_index >= gameTypes.size())
		{
			return;
		}

		for(Rocket::Core::StringList::const_iterator it = cols.begin();
			 it != cols.end();
			 ++it){
			if(*it == "val") row.push_back(gameTypes[row_index].first.c_str());
			else if(*it ==  "name") row.push_back(gameTypes[row_index].second.c_str());
			else row.push_back("");
		}
	}

	int GameTypesDataSource::GetNumRows(const Rocket::Core::String &)
	{
		return gameTypes.size();
	}
}
