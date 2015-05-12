/*
Copyright (C) 2011 Cervesato Andrea ("koochi")

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

#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_utils.h"
#include "widgets/ui_widgets.h"

#include <Rocket/Controls.h>

namespace WSWUI
{
using namespace Rocket::Core;
using namespace Rocket::Controls;

class SelectableDataGrid : public ElementDataGrid
{
public:
	SelectableDataGrid( const String& tag ) : 
		ElementDataGrid(tag), lastSelectedRow( NULL ), lastSelectedRowIndex( -1 )
	{
		SetProperty( "selected-row", "-1" );
	}

	~SelectableDataGrid()
	{
		if( lastSelectedRow != NULL ) {
			lastSelectedRow->SetPseudoClass( "selected", false );
			lastSelectedRow->RemoveReference();
		}
	}

	/// Called for every event sent to this element or one of its descendants.
	/// @param[in] event The event to process.
	void ProcessEvent( Rocket::Core::Event& evt )
	{
		ElementDataGrid::ProcessEvent( evt );

		if( evt == "click" || evt == "dblclick" )
		{
			Element* elem;
			int column = -1;

			// get the column index
			elem = evt.GetTargetElement();
			while( elem && elem->GetTagName() != "datagridcell" && elem->GetTagName() != "datagridcolumn" ) {
				elem = elem->GetParentNode();
			}
			if( elem ) {
				// "datagridcolumn" points to just Element, so figure out the index by iterating
				// FIXME: We could be little smarter with this and get the column definition here too
				// and use colselect or colactivate events
				if( elem->GetTagName() == "datagridcolumn" ) {
					Element* child = elem->GetParentNode()->GetFirstChild();
					column = 0;
					while( child && child != elem ) {
						child = child->GetNextSibling();
						column++;
					}
				}
				else {
					column = static_cast<ElementDataGridCell *>( elem )->GetColumn();
				}
			}

			// get the row element
			elem = evt.GetTargetElement();
			while( elem && elem->GetTagName() != "datagridrow" && elem->GetTagName() != "datagridheader" ) {
				elem = elem->GetParentNode();
			}

			if( elem )
			{
				ElementDataGridRow *row = static_cast<ElementDataGridRow*>( elem );
				int index = row->GetTableRelativeIndex();
				Rocket::Core::String indexStr(toString( index ).c_str());
					
				// this should never happen
				if( index >= this->GetNumRows() )
					return;
				if( index >= 0 )
				{
					// deselect last selected row
					if( lastSelectedRow != row )
					{
						if( lastSelectedRow ) {
							lastSelectedRow->SetPseudoClass( "selected", false );
							lastSelectedRow->RemoveReference();
						}
					}

					// select clicked row
					lastSelectedRow = row;
					lastSelectedRowIndex = index;

					this->SetProperty( "selected-row", indexStr );

					row->SetPseudoClass( "selected", true );
					row->AddReference();
				}
				
				Rocket::Core::Dictionary parameters;
				parameters.Set( "index", indexStr );
				parameters.Set( "column_index", column );
				if( evt == "click" )
					DispatchEvent( "rowselect", parameters );
				else
					DispatchEvent( "rowactivate", parameters );
			}
		}
		else if( evt == "rowadd" )
		{
			if( lastSelectedRowIndex < 0 ) {
				return;
			}

			int numRowsAdded = evt.GetParameter< int >( "num_rows_added", 0 );
			if( !numRowsAdded ) {
				return;
			}

			int firstRowAdded = evt.GetParameter< int >( "first_row_added", 0 );
			if( lastSelectedRowIndex >= firstRowAdded ) {
				lastSelectedRowIndex += numRowsAdded;
				Rocket::Core::String indexStr( toString( lastSelectedRowIndex ).c_str() );
				this->SetProperty( "selected-row", indexStr );
			}
		}
		else if( evt == "rowremove" )
		{
			if( lastSelectedRowIndex < 0 ) {
				return;
			}

			int numRowsRemoved = evt.GetParameter< int >( "num_rows_removed", 0 );
			if( !numRowsRemoved ) {
				return;
			}

			int firstRowRemoved = evt.GetParameter< int >( "first_row_removed", 0 );
			if( lastSelectedRowIndex >= firstRowRemoved && lastSelectedRowIndex < firstRowRemoved + numRowsRemoved ) {
				lastSelectedRow->RemoveReference();
				lastSelectedRow = NULL;

				lastSelectedRowIndex = -1;
				this->SetProperty( "selected-row", "-1" );
			}
		}
	}

private:
	Element *lastSelectedRow;
	int lastSelectedRowIndex;
};

//=====================

Rocket::Core::ElementInstancer *GetSelectableDataGridInstancer(void)
{
	return __new__(GenericElementInstancer<SelectableDataGrid>)();
}
}
