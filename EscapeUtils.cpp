#include "Arduino.h"   

/**
 * bsl_scape : Back Slash Escape
 * This function returns the escape character
 * works for \n \r and \t
*/
char escapeChar(char code) {
    if (code == 'n') return '\n';       // newline feed
    if (code == 'z') return (char) 26;  // CTRL+Z char code from ASCII table
    if (code == 'r') return '\r';       // carrier return
    if (code == 't') return '\t';       // tabulation character
    return code;
}

String escapeString(String str) {
    String proc = "";
    const char* cs = str.c_str();
    for(u16 i = 0; cs[i]; i++) {
        if (cs[i] == '\\') {
            i++;
            proc += escapeChar(cs[i]);
        } else proc += cs[i];
    }
    return proc;
}