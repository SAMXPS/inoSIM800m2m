#ifndef ESCAPE_UTILS_H
#define ESCAPE_UTILS_H

#include "Arduino.h"   

/**
 * bsl_scape : Back Slash Escape
 * This function returns the escape character
 * works for \n \r and \t
*/
char escapeChar(char code);
String escapeString(String str);

#endif //ESCAPE_UTILS_H