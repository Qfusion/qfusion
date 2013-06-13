/*
 * KeyConverter.h
 *
 *  Created on: 26.6.2011
 *      Author: hc
 */

#ifndef KEY_CONVERTER_H_
#define KEY_CONVERTER_H_

// Rocket keys used by mouse event
enum MouseKeyIdentifier {
	KI_MOUSE1 = 0,
	KI_MOUSE2 = 1,
	KI_MOUSE3 = 2,
	KI_MOUSE4 = 3,
	KI_MOUSE5 = 4,
	KI_MOUSE6 = 5,
	KI_MOUSE7 = 6,
	KI_MOUSE8 = 7,
	KI_MWHEELUP = -1,
	KI_MWHEELDOWN = +1
};

class KeyConverter
{
public:
	KeyConverter();
	virtual ~KeyConverter();

	int getModifiers();
	int toRocketKey( int key );
	int fromRocketKey( int key );
	int toRocketMouse( int btn );
	int fromRocketMouse( int btn );
	int toRocketWheel( int wheel );
	int fromRocketWheel( int wheel );

	int specialChar( int c );
};

#endif /* KEY_CONVERTER_H_ */
