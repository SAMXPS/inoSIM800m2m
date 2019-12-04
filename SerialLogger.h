/**
 * Forma de definir um sprintln para o Serial fácil de ser desabilitado a nível de compilação
*/
#ifndef SERIAL_LOGGER_H
#define SERIAL_LOGGER_H
#if VERBOSE
#define sprintln(x) Serial.println(x)
#else
#define sprintln(x)
#endif
#endif