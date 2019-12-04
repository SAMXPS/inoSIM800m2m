#include "SerialLogger.h"

/**
 * This function should be defined on the arduino main and it
 * should reset the board
*/
void reset_ino(); 

void assert(bool val, bool rst = true) {
    if (!val) {
        sprintln(F("[!] assert failed"));
        if (rst) {
            sprintln(F("Reseting ino board..."));
            delay(500);
            reset_ino();
        }
    }
}