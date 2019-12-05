#include "Arduino.h"
#include "SerialLogger.h"
#include "Assert.h"

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