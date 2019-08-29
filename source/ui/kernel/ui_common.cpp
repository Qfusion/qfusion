#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "../gameshared/q_shared.h"

namespace WSWUI
{

// Utility function (TODO: move to common area)
// (TODO 2: move this to .h or implementation file)
// container is of type that stores Element*, implements push_back and clear
// and its range can be used in std::for_each.
// predicate looks like bool(Element*) and should return true if it wants
// the element given as parameter to be included on the list
template<typename T, typename Function>
void collectChildren( Rml::Core::Element *elem, T &container, Function predicate ) {
	Rml::Core::Element *child = elem->GetFirstChild();
	while( child ) {
		if( predicate( child ) ) {
			container.push_back( child );
		}

		// recurse
		collectChildren( child, container );

		// and iterate
		child = child->GetNextSibling();
	}
}

}
