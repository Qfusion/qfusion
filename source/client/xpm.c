/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include "xpm.h"

/*
    Die Zustaende unseres Automaten.
*/
typedef enum xpm_state_enum {
	METADATA,
	COLOR,
	DATA
} xpm_state_t;

/*
    Eigentlich wollte ich das mit strtok machen, aber die Funktion
    hat irgendwas gegen mich...
*/
static int parse_xpm_meta_elem( const char *elem, int *start ) {
	/*
	    Buffer fuer den Substring, wir hoffen einfach
	    mal, dass keine Zahl groesser als 19 Stellen vorkommt.
	*/
	char buffer[20];
	/*
	    Aktuelle Position im String,
	    Laenge des Strings
	*/
	int i, len;

	len = (int)strlen( elem );
	/*
	    Wir suchen nach dem naechsten Leerzeichen in elem
	*/
	for( i = *start; i < len; i++ ) {
		if( elem[i] == ' ' ) {
			break;
		}
	}
	/*
	    Wenn wir ein Leerzeichen gefunden haben oder elem
	    zu Ende ist, zeigt i auf das erste Zeichen hinter dem
	    gesuchten Substring.
	    Wir kopieren unseren Substring aus elem raus in
	    den Buffer. elem+start ist ein Pointer, der auf das
	    Zeichen zeigt, bei dem wir angefangen haben zu parsen.
	    i-start ist die Anzahl der Zeichen, die wir geparst haben
	    (ohne das Leerzeichen).
	*/
	strncpy( buffer, elem + *start, i - *start );
	/*
	    Wir haengen eine 0 an unseren Substring, damit er von
	    atoi richtig verarbeitet werden kann.
	*/
	buffer[i - *start] = '\0';
	/*
	    Das Zeichen an Stelle i haben wir ja schon geparst
	    also fangen wir beim naechsten Aufruf ein Zeichen
	    weiter hinten an.
	*/
	*start = i + 1;
	/*
	    Wir wandeln den Substring in einen int um.
	*/
	return atoi( buffer );
}

/*
    Metadaten parsen.
    Parameter: Aktuelle Zeile aus warsow16x16_xpm
*/
static void parse_xpm_meta( const char *elem, xpm_state_t *xpm_state, int **data, int *num_colors, int *chars_per_color, char ***color_symbols, int **color_values ) {
	/*
	    Die Stelle in elem, an der wir beginnen zu suchen,
	    durch static suchen wir genau da weiter, wo wir
	    beim letzten Aufruf der Funktion aufgehoert haben.
	*/
	int start = 0;

	/*
	    MeineUltra-eins-drei-drei-sieben-1337-h4xx0r-Funktion macht
	    es uns sehr einfach, die einzelnen Zahlen aus den Metadaten zu
	    extrahieren.
	    Ein Aufruf von parse_xpm_meta_elem extrahiert den Substring
	    bis zum naechsten Leerzeichen (oder Stringende) und wandelt
	    diesen in eine Zahl um.
	*/
	int width = parse_xpm_meta_elem( elem, &start );
	int height = parse_xpm_meta_elem( elem, &start );
	*num_colors = parse_xpm_meta_elem( elem, &start );
	*chars_per_color = parse_xpm_meta_elem( elem, &start );

	/*
	    Anhand der Metadaten koennen wir nun berechnen, wie viel Speicherplatz
	    unsere Arrays benoetigen.
	*/
	*color_symbols = malloc( sizeof( **color_symbols ) * *num_colors );
	memset( *color_symbols, 0, sizeof( **color_symbols ) * *num_colors );
	*color_values = malloc( sizeof( **color_values ) * *num_colors );
	*data = malloc( sizeof( **data ) * ( width * height + 2 ) );
	( *data )[0] = height;
	( *data )[1] = width;

	/*
	    Da die Metadaten immer nur eine Zeile lang sind,
	    sollen die folgenden Zeilen als Farben geparst werden.
	*/
	*xpm_state = COLOR;
}

/*
    Gibt den passenden int-Wert fuer ein einzelnes Hex-Symbol
    zurueck.
*/
#define hex2int( elem ) ( ( ( elem ) >= '0' && ( elem ) <= '9' ) ? ( elem ) - '0' : ( ( ( elem ) >= 'A' && ( elem ) <= 'F' ) ? ( elem ) - 'A' + 10 : 0 ) )

/*
    Farben parsen.
    Parameter: Aktuelle Zeile aus warsow16x16_xpm,
            Index von color_symbols und color_values in den wir
            als naechstes Schreiben
*/
static void parse_xpm_color( const char *elem, xpm_state_t *xpm_state, int num_colors, int chars_per_color, char **color_symbols, int *color_values, int *color_pos ) {
	assert( color_symbols && color_values );

	/*
	    In parse_xpm_meta haben wir nur Speicher fuer die char-Pointer reserviert,
	    jetzt brauchen wir noch Speicher fuer die Daten an sich.
	    Wir reservieren den Speicher erstmal nur fuer das Element, das wir gleich
	    befuellen.
	*/
	color_symbols[*color_pos] = malloc( sizeof( char ) * ( chars_per_color + 1 ) );
	/*
	    Wir kopieren den Farbcode vom Anfang von elem in color_symbols.
	    Die Anzahl der Zeichen, die wir kopieren wollen entspricht natuerlich
	    der Anzahl der Zeichen pro Farbcode.
	*/
	strncpy( color_symbols[*color_pos], elem, chars_per_color );
	/*
	    Unser Farbcode ist ein String und muss deshalb mit 0 abgeschlossen werden.
	    Mit color_symbols[*color_pos] bekommen wir den Pointer auf das erste Zeichen
	    von unserem Farbcode, mit +chars_per_color gehen wir an die Speicheradresse
	    hinter dem Farbcode.
	*/
	*( color_symbols[*color_pos] + chars_per_color ) = '\0';
	/*
	    Die Position an der die Raute in elem stehen sollte.
	    Weil die Hex-Werte immer 6 Zeichen lang sind,
	    steht die Raute immer 7 Stellen vorm Stringende.
	*/
	int hex_sign_pos = strlen( elem ) - 7;
	/*
	    Wir testen ob an der berechneten Stelle tatsaechlich eine Raute ist
	*/
	if( elem[hex_sign_pos] == '#' ) {
		/*
		    Wir gehen die 6 Hex-Symbole von links nach rechts durch, jeweils 2 gehoeren zu einem rgb-Kanal.
		*/
		/*
		    Weil wir die Hex-Symbole unabhaengig ihrer Stelle umwandeln, stimmt der Wert natuerlich nicht.
		    Das rechte Symbol steht an der Stelle mit Wertigkeit 1, die multiplikation mit 1 koennen wir uns
		    natuerlich Sparen. Das linke Symbol steht an der Stelle mit Wertigkeit 16.

		    Stelle im Hexadezimalsystem:    4,   3,  2, 1
		    Wertigkeit im Dezimalsystem: 4096, 256, 16, 1
		*/
		int32_t r = hex2int( elem[hex_sign_pos + 1] ) * 16 + hex2int( elem[hex_sign_pos + 2] );
		int32_t g = hex2int( elem[hex_sign_pos + 3] ) * 16 + hex2int( elem[hex_sign_pos + 4] );
		int32_t b = hex2int( elem[hex_sign_pos + 5] ) * 16 + hex2int( elem[hex_sign_pos + 6] );
		int32_t a = 255;
		/*
		    Um die Kanaele in einen int zu quetschen, muessen wir die Werte an die passende Stelle
		    "verschieben".
		    a -> 7
		    r -> 5
		    g -> 3
		    b -> 1
		*/
		color_values[*color_pos] = a * 16777216 + r * 65536 + g * 256 + b;
	} else {
		/*
		    Wenn wir keine Raute finden, gehen wir davon aus, dass die
		    Farbdefinition None ist.
		*/
		color_values[*color_pos] = 0;
	}
	/*
	    Unser Index soll auf das naechste freie Element zeigen.
	*/
	( *color_pos )++;
	/*
	    Wir haben so viele Farben geparst wie in den Metadaten angegeben,
	    die folgenden Zeilen sollten also Daten enthalten.
	*/
	if( *color_pos == num_colors ) {
		*xpm_state = DATA;
	}
}

/*
    Pixeldaten parsen
    Parameter: Aktuelle Zeile aus warsow16x16_xpm,
            Index von data in den wir als naechstes Schreiben
*/
static void parse_xpm_data( const char *elem, int *data, int num_colors, int chars_per_color, char **color_symbols, int *color_values, int *data_pos ) {
	/*
	    Laenge der aktuellen Zeile
	*/
	int len = strlen( elem );
	/*
	    Position in der aktuellen Zeile
	    Position im color_symbols-Array
	*/
	int i, j;
	/*
	    Wir gehen die aktuelle Zeile von vorne bis hinten durch und
	    betrachten dabei immer eine Anzahl von chars_per_color
	    Zeichen gleichzeitig (einen Farbcode halt).
	*/

	assert( data && *data );
	assert( color_symbols && color_values );

	for( i = 0; i < len; i += chars_per_color ) {
		/*
		    Fuer jeden Farbcode gehen wir color_symbols von vorne
		    bis hinten durch, bis wir den Farbcode im Array gefunden
		    haben(-> Lineare Suche).
		*/
		for( j = 0; j < num_colors; j++ ) {
			/*
			    Wir vergleichen den String, der an der Speicheradresse
			    elem+i beginnt und chars_per_color Zeichen lang ist,
			    mit dem aktuellen Element aus color_symbols
			*/
			if( strncmp( elem + i, color_symbols[j], chars_per_color ) == 0 ) {
				/*
				    Die Farbcodes stimmen ueberein, der passende argb-Wert fuer das Farbsymbol
				    aus color_symbols steht an genau dem gleichen Index in color_values.
				    Wir schreiben den Farbwert in unsere Bitmap und gehen ein Pixel weiter.
				*/
				if( *data_pos < 2 + data[0] * data[1] ) {
					data[( *data_pos )++] = color_values[j];
				}
				/*
				    Wir haben unser Farbsymbol gefunden, der Rest von color_symbols interessiert
				    uns nicht mehr. Wir machen mit dem naechsten Farbsymbol in elem weiter.
				*/
				break;
			}
		}
	}
}
/*
    Wir parsen eine Zeile aus dem warsow16x16_xpm-Array
    Parameter: Aktuelle Zeile aus warsow16x16_xpm
*/
static void parse_xpm_elem( const char *elem, xpm_state_t *xpm_state, int **data, int *num_colors, int *chars_per_color, char ***color_symbols, int **color_values, int *color_pos, int *data_pos ) {
	/*
	    Wir pruefen, in welchem Zustand sich unser Automat befindet.
	*/
	switch( *xpm_state ) {
		case METADATA:
			/*
			    Wir parsen die Metadaten.
			*/
			parse_xpm_meta( elem, xpm_state, data, num_colors, chars_per_color, color_symbols, color_values );
			break;
		case COLOR:
			/*
			    Wir parsen Farben
			*/
			parse_xpm_color( elem, xpm_state, *num_colors, *chars_per_color, *color_symbols, *color_values, color_pos );
			break;
		case DATA:
			/*
			    Wir parsen die Bild-Daten
			*/
			parse_xpm_data( elem, *data, *num_colors, *chars_per_color,  *color_symbols, *color_values, data_pos );
			break;
	}
}

int *parse_xpm_icon( int num_xpm_elems, const char *xpm_data[] ) {
	int i;
	xpm_state_t xpm_state;
	/*
	    Breite des Bildes,
	    Hoehe des Bildes,
	    argb-Wert der Farben, (Array aus ints)
	    Das Bild umgewandelt in eine argb-Bitmap (Array aus ints)
	 */
	int *data = NULL;
	/*
	    Anzahl der Farben,
	    Anzahl Zeichen mit der eine Farbe codiert ist
	*/
	int num_colors = 0, chars_per_color = 0;
	char **color_symbols = NULL;
	int *color_values = NULL;
	/*
	    Die Anzahl der Farben, die wir schon gespeichert haben. Gibt an,
	    an welcher Position vom color_symbols-Array und color_values-Array
	    wir die naechste Farbe speichern koennen. static sorgt dafuer, dass
	    wir die Variable innerhalb der Funktion deklarieren koennen und bei
	    wiederholtem Aufruf der Funktion den Wert behalten.
	*/
	int color_pos = 0;
	/*
	    Das gleiche wie color_pos nur fuer den data-Array.
	*/
	int data_pos = 2;

	/*
	    Aktueller Zustand des Automaten, METADATA ist der Startzustand.
	*/
	xpm_state = METADATA;
	for( i = 0; i < num_xpm_elems; i++ )
		parse_xpm_elem( xpm_data[i], &xpm_state, &data, &num_colors, &chars_per_color, &color_symbols, &color_values, &color_pos, &data_pos );

	if( color_symbols ) {
		for( i = 0; i < num_colors; i++ ) {
			if( color_symbols[i] ) {
				free( color_symbols[i] );
			}
		}
		free( color_symbols );
		color_symbols = NULL;
	}

	if( color_values ) {
		free( color_values );
		color_values = NULL;
	}

	return data;
}

int *XPM_ParseIcon( int num_xpm_elems, const char *xpm_data[] ) {
	return parse_xpm_icon( num_xpm_elems, xpm_data );
}
