/*
 * KeyConverter.h
 *
 *  Created on: 26.6.2011
 *      Author: hc
 */

#ifndef KEY_CONVERTER_H_
#define KEY_CONVERTER_H_

namespace WSWUI {
namespace KeyConverter {

	int getModifiers();
	int toRocketKey( int key );
	int fromRocketKey( int key );
	int toRocketWheel( int wheel );
	int fromRocketWheel( int wheel );
	int specialChar( int c );

}
}

#endif /* KEY_CONVERTER_H_ */
