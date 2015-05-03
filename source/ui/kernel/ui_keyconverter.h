/*
 * KeyConverter.h
 *
 *  Created on: 26.6.2011
 *      Author: hc
 */

#ifndef KEY_CONVERTER_H_
#define KEY_CONVERTER_H_

namespace WSWUI {

class KeyConverter {
public:
	static int fromRocketKey( int key );
	static int fromRocketWheel( int wheel );
	static int getModifiers();
	static int toRocketKey( int key );
	static int toRocketWheel( int wheel );

private:
	static int specialChar( int c );

	/* Special punctuation characters */
	static const char *oem_keys;
};

}

#endif /* KEY_CONVERTER_H_ */
