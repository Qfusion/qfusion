#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"

#include "as/asui.h"
#include "as/asui_local.h"

#include <RmlUi/Controls/DataSource.h>

namespace ASUI
{

typedef Rml::Controls::DataSource DataSource;

//============================================
/*
    common DataSource methods
 */

// TODO: proper DataSource "handle" class with constructor calling GetDataSource

// global function

// methods
static int DataSource_GetNumRows( DataSource *ds, const asstring_t &table ) {
	return ds->GetNumRows( ASSTR( table ) );
}

static asstring_t *DataSource_GetName( DataSource *ds ) {
	return ASSTR( ds->GetDataSourceName() );
}

static asstring_t *DataSource_GetField( DataSource *ds, const asstring_t &table, int idx, const asstring_t &field ) {
	StringList row;
	StringList fields;

	fields.push_back( field.buffer );
	ds->GetRow( row, ASSTR( table ), idx, fields );

	if( row.size() > 0 ) {
		return ASSTR( row[0] );
	}
	return ASSTR( "" );
}

static DataSource *DataSource_GetDataSource( const asstring_t &name ) {
	return Rml::Controls::DataSource::GetDataSource( ASSTR( name ) );
}

static int DataSource_FindRow( DataSource *ds, const asstring_t &table, const asstring_t &field, const asstring_t &value, int start = 0 ) {
	int numRows;
	StringList fields;

	fields.push_back( field.buffer );

	numRows = ds->GetNumRows( ASSTR( table ) );
	for( int i = start; i < numRows; i++ ) {
		StringList row;

		ds->GetRow( row, ASSTR( table ), i, fields );
		if( !strcmp( row[0].c_str(), value.buffer ) ) {
			return i;
		}
	}

	return -1;
}

void PrebindDataSource( ASInterface *as ) {
	ASBind::Class<Rml::Controls::DataSource, ASBind::class_ref>( as->getEngine() );
}

void dummy( DataSource *ds ) {
}


void BindDataSource( ASInterface *as ) {
	ASBind::GetClass<Rml::Controls::DataSource>( as->getEngine() )
	.refs( &dummy, &dummy )

	.constmethod( &DataSource_GetName, "get_name", true )
	.constmethod( &DataSource_GetNumRows, "numRows", true )
	.constmethod( &DataSource_GetField, "getField", true )
	.method2( &DataSource_FindRow, "int findRow( const String &table, const String &field, const String &value, int start = 0 ) const", true )
	;

	// FIXME: need singleton binding in the DataSource class
	ASBind::Global( as->getEngine() )
	.function( DataSource_GetDataSource, "getDataSource" )
	;
}

}

ASBIND_TYPE( Rml::Controls::DataSource, DataSource )
