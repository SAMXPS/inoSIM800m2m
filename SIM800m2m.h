#ifndef SIM800LM2M_H
#define SIM800LM2M_H
#if 0
#include "SoftwareSerial.h"

/**
 * SIM800L simple M2M library.
 * This library should be used for sending TCP data to a single server,
 * based on a M2M (Machine To Machine) system
*/
class SIM800m2m {
    public:
    /**
     * Class constructor for the library has the following params
     * RX       -> Serial PIN for receiving data.
     * TX       -> Serial PIN for sending data.
     * RST      -> PIN used to RESET the module.
     * bprate   -> Baud rate of the serial connection.
    */
    SIM800m2m (u8 RX, u8 TX, u8 RST, unsigned int bprate);

    /* Updates information about the APN that will be used to connect to the GPRS network */
    bool set_apn_config(String host, String user, String password);

    /* This function should setup all pins, serial connection and initial AT commands */
    bool setup();

    /* Returns true if there is a TCP connection avaiable*/
    bool tcp_connected();

    /* Function to connect to the TCP server */
    bool tcp_connect();

    /* Function to set the TCP server to connect to */
    bool tcp_sethost(String host, unsigned int port);

    /* Should the module use SSL when TCP connecting? */
    bool tcp_set_ssl(bool active);

    /* Function to send data to the TCP server */
    bool tcp_send(String data);

    /* Function to set the callback when receiving data*/
    bool tcp_receiver(void (*callback)(String data_rcv));

    /* Function to set the callback when the module can't connect to the TCP server*/
    bool tcp_auto_connect_error(void (*callback)());

    /* Should the lib auto reconnect? */
    bool tcp_auto_connect(bool auto_connect);

    /* This function should be called in the main arduino loop */
    void loop();

    /* Serial connection to the SIM module */
    SoftwareSerial getSerial();

};
#endif
#endif //SIM800LM2M_H