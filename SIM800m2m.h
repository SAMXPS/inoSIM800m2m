/**
 * SIM800L simple M2M library.
 * This library should be used for sending TCP data to a single server,
 * based on a M2M (Machine To Machine) system
*/
#ifndef SIM800LM2M_H
#define SIM800LM2M_H
#include "SoftwareSerial.h"

typedef u16 r_code;

class SIM800m2m {

private:
    SoftwareSerial sim_serial;
    u8 RX,TX,RST;
    unsigned int bprate;
    String       tcp_host;
    unsigned int tcp_port;
    u32 tcp_last_check = 0;
    void (*tcp_callback)(const char * data_rcv, const u8&len) = NULL;
    void (*tcp_error_callback)() = NULL;
    void (*gprs_error_callback)() = NULL;
    bool tcp_status = false;
    bool tcp_auto = true;
    bool tcp_ssl  = false;
    bool gprs_connected = false;
    int  _last_resolver = -1;
    #define _RBFLEN 128
    char _readbuff[_RBFLEN];    // Buffer for reading lines
    String apn, user, password;
    u8 tries = 0;

    bool reset();
    bool config();
    bool config_gprs() ;
    bool sendCommand(const String&command);
    bool sendData(const String&data, r_code resolvers);
    bool sendCommand(const String&command, r_code resolvers);
    void tcp_check_status();
    void soft_wait(u32 t);
    bool _processResolvers(r_code code);
    int  _serial_process_line(r_code resolvers = 0);
    bool _serial_rcln(u8 buffpos); // read char till new line to _readbuff at buffpos
    int  _rbidxof(const String&str);

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
    bool set_apn_config(const String&host, const String&user, const String&password);

    /* Set the function that will be called when the lib cannot connect to the GPRS network */
    bool set_gprs_error_callback(void (*callback)());

    /* This function should setup all pins, serial connection and initial AT commands */
    bool setup();

    /* Returns true if there is a TCP connection avaiable*/
    bool tcp_connected();

    /* Function to connect to the TCP server */
    bool tcp_connect();

    /* Function to set the TCP server to connect to */
    bool tcp_sethost(const String&host, unsigned int port);

    /* Should the module use SSL when TCP connecting? */
    bool tcp_set_ssl(bool active);

    /* Updates the certificate fale to be used in the TCP connection*/
    bool tcp_set_ssl_cert(const String&fname);

    /* Function to send data to the TCP server */
    bool tcp_send(const String&data);

    /* Function to set the callback when receiving data*/
    bool tcp_receiver(void (*callback)(const char * data_rcv, const u8&len));

    /* Function to set the callback when the module can't connect to the TCP server*/
    bool tcp_auto_connect_error(void (*callback)());

    /* Should the lib auto reconnect? */
    bool tcp_auto_connect(bool auto_connect);

    /* This function should be called in the main arduino loop */
    void loop();

    /* Serial connection to the SIM module */
    SoftwareSerial getSerial();

    /* SIM800L File System: */
    /* Create a file in the SIM800 internal storage */
    bool fs_create(const String&fname);
    
    /* Reads data from a file */
    bool fs_read(const String& fname, int start_pos, int end_pos, String* read);

    /* Writes data to file */
    bool fs_write(const String& fname, bool tail, const String&data);

    /* Deletes a file*/
    bool fs_del(const String& fname);

};

#endif //SIM800LM2M_H