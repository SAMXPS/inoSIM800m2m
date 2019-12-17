#include <Arduino.h>
#include <SoftwareSerial.h>
#include "SIM800m2m.h"

    class CommandResolver {
        public:
        String match;
        bool returner;

        CommandResolver(String match, bool returner) {
            this->match = match;
            this->returner = returner;
        };
    };

    #define R(x) (r_code)(1 << x)

    const CommandResolver _resolvers[] = {
        CommandResolver("OK", true), 
        #define _OK R(0x0)

        CommandResolver("\n", true),   
        #define _NEWLINE R(0x1) 

        CommandResolver("", false), 
        #define _EMPTY R(0x2)
        
        CommandResolver("+CME ERROR", false),
        #define _CME_ERROR R(0x3) 

        CommandResolver("CONNECT OK", true),
        #define _CONNECT_OK R(0x4)  

        CommandResolver("ALREADY CONNECT", false),
        #define _ALREADY_CONNECT R(0x5)  

        CommandResolver("CONNECT FAIL", false),
        #define _CONNECT_FAIL R(0x6)  
        
        CommandResolver("DATA ACCEPT", true),
        #define _DATA_ACCEPT R(0x7)  

        CommandResolver("SEND OK", true),
        #define _SEND_OK R(0x8)  

        CommandResolver("SEND FAIL", false),
        #define _SEND_FAIL R(0x9)  

        CommandResolver("ERROR", false),
        #define _ERROR R(0xA)  

        CommandResolver(">", true),
        #define _DATA_BEGIN R(0xB)  

        CommandResolver("STATE", true),
        #define _STATE R(0xC)

        CommandResolver("+IPD,", true),
        #define _DATA_RECEIVED R(0xD)
    };

    int _2bas(r_code x) {
        int i = 0;
        while (x>1) {
            x = x >> 1;
        }
        return i;
    }

    SIM800m2m::SIM800m2m (u8 RX, u8 TX, u8 RST, unsigned int bprate) : sim_serial(RX, TX){
        this->RX = RX;
        this->TX = TX;
        this->RST = RST;
        this->bprate = bprate;
    }

    SoftwareSerial SIM800m2m::getSerial() {
        return this->sim_serial;
    }

    bool SIM800m2m::set_apn_config(const String&apn, const String&user, const String&password) {
        this->apn = apn;
        this->user = user;
        this->password = password;
    }

    bool SIM800m2m::tcp_auto_connect(bool auto_connect) {
        this->tcp_auto = auto_connect;
    }

    bool SIM800m2m::tcp_sethost(const String&host, unsigned int port) {
        this->tcp_host = host;
        this->tcp_port = port;
        return true;
    }

    bool SIM800m2m::tcp_receiver(void (*callback)(const char * data_rcv, const u8&len)) {
        this->tcp_callback = callback;
    }

    bool SIM800m2m::tcp_auto_connect_error(void (*callback)()) {
        this->tcp_error_callback = callback;
    }

    bool SIM800m2m::set_gprs_error_callback(void (*callback)()) {
        this->gprs_error_callback = callback;
    }

    /**
     * Setup function: sets up the serial communication and used pins
     * for the SIM800L to work properly.
    */
    bool SIM800m2m::setup() {
        // Setup serial connection to the module
        sim_serial.begin(bprate);
        sim_serial.setTimeout(1500);

        // Config reset and dtr pins
        pinMode(RST, OUTPUT);

        // Pull up the pins from ground (default mode)
        digitalWrite(RST, HIGH);

        // Reset the module
        if (!reset()) return false;

        // Configure the module
        if (!config()) return false;

        return true;
    }

    /**
     * Reset function: sets the RST PIN into low mode for 2 seconds,
     * wait for the module to reset and sends AT test command.
    */
    bool SIM800m2m::reset() {
        Serial.println(F("Reseting..."));
        digitalWrite(RST, LOW);
        delay(2000);
        digitalWrite(RST, HIGH);
        delay(7000);

        /* reseting private state variables */
        tries = 0;
        tcp_status = false;
        tcp_ssl  = false;
        gprs_connected = false;
        _last_resolver = -1;

        Serial.println(F("Testing..."));
        return sendCommand(F("AT"));
    }

    
    bool SIM800m2m::config() {
        if (!sendCommand("AT")) return false; // Testa o handshake
        sendCommand(F("AT+CSQ"));         // Testa o nível do sinal da rede
        sendCommand(F("AT+CCID"));        // Verifica as informações do cartão SIM (chip)
        sendCommand(F("AT+CREG?"));       // Verifica se está registrado na REDE
        sendCommand(F("AT&D2"));          // Sets the funcion of DTR pin
        sendCommand(F("AT+IFC=0,0"));     // Sets no flow control
        config_gprs();

        return true;
    }

    bool SIM800m2m::config_gprs() {
        /* +CSTT is used to configure the APN used in the GPRS context */
        sendCommand("AT+CSTT=\"" + apn + "\",\"" + user + "\",\"" + password + "\"");
        Serial.println("Wait...");
        soft_wait(3000);
        sendCommand(F("AT+CIICR"));        // Turn on GPRS circuit
        sendCommand(F("AT+CIFSR"));        // Check received IP address
        /* +CIPQSEND=1: Quick send mode – when the data is sent to module, it will
        respond DATA ACCEPT:<n>,<length>, not responding SEND OK.*/
        sendCommand(F("AT+CIPQSEND=1"));        
        //sendCommand(F("AT+CIPSRIP=1"));  // Show Remote IP Address and Port When Received Data 
        sendCommand(F("AT+CIPHEAD=1"));

        gprs_connected = true;
        tcp_check_status();
        return true;
    }

    bool SIM800m2m::sendCommand(const String&command) {
        return sendCommand(command, _OK | _CME_ERROR | _ERROR);
    }

    bool SIM800m2m::tcp_connected() {
        return tcp_status;
    }

    bool SIM800m2m::sendData(const String&data, r_code resolvers) {
        sim_serial.print(data + "\r\n");
        delay(200L);
        bool r = _processResolvers(resolvers);
        return r;
    }

    bool SIM800m2m::sendCommand(const String&command, r_code resolvers) {
        sim_serial.print(command + "\r\n");
        delay(100L);
        return _processResolvers(resolvers);
    }

    bool SIM800m2m::tcp_set_ssl(bool active) {
        if (sendCommand("AT+CIPSSL=" + String((u8) active))) {
            this->tcp_ssl = active;
            return true;
        }
        return false;
    }

    void SIM800m2m::tcp_check_status() {
        tcp_last_check = millis();

        if (sendCommand(F("AT+CIPSTATUS"), _OK | _ERROR | _CME_ERROR) && _processResolvers(_STATE)) {
            if (_rbidxof(F("CONNECT OK")) != -1) {
                gprs_connected = true;
                tcp_status = true;
                Serial.println(F("[INFO] TCP IS CONNECTED"));
                return;
            } else if (_rbidxof(F("PDP DEACT")) != -1) {
                // No connection to the GPRS network
                gprs_connected = false;
                tcp_status = false;
                Serial.println(F("[INFO] GPRS IS NOT CONNECTED"));
                return;
            }
        }
        
        tcp_status = false;
        Serial.println(F("[INFO] TCP IS DISCONNECTED"));
    }

    bool SIM800m2m::tcp_connect() {
        bool r = sendCommand("AT+CIPSTART=\"TCP\",\"" + tcp_host + "\"," + String(tcp_port), 
            _CONNECT_FAIL | _CONNECT_OK | _CME_ERROR | _ALREADY_CONNECT | _ERROR
        );
        return (this->tcp_status = (r || _last_resolver == _2bas(_ALREADY_CONNECT))); 
    }

    bool SIM800m2m::tcp_send(const String&data) {
        if (!tcp_connected()) return false;

        if (!sendCommand("AT+CIPSEND=" + String(data.length()), 
            _DATA_BEGIN | _ERROR | _CME_ERROR
        )) return false;

        delay(100);

        return sendData(data, 
            _DATA_ACCEPT | _SEND_OK | _SEND_FAIL | _CME_ERROR | _ERROR
        );
    }

    bool SIM800m2m::_processResolvers(r_code code) {
        u32 start = millis();
        while (millis() - start < 5000) {
            while (sim_serial.available()) {
                _serial_process_line(code);
                if (_last_resolver >= 0) {
                    return _resolvers[_last_resolver].returner;
                }
            }
            delay(100);
        }
        return false;
    }

    // buffer index of
    int _bidxof(char*line, const String& str) {
        if (strlen(line) >= str.length()) {
            const char *found = strstr(line, str.c_str());
            if (found) 
                return (found - line);
        }
        return -1;
    }

    int _match(char* line, r_code code, u8*pos = NULL) {
        int p;
        for (u8 i = 0; i < 16; i++) {
            if ((code >> i) % 2){
                if ((p = _bidxof(line, _resolvers[i].match)) != -1) {
                    if (pos) *pos = p;
                    return i;    
                }
            }
        }
        return -1; // No match
    }

    int SIM800m2m::_serial_process_line(r_code resolvers) {
        int m = -1;
        u8 j;
        // read into the buffer up to newline char or memory limit of _RBFLEN
        for (j = 0; !_serial_rcln(j) && j+1 < _RBFLEN; j++) {
            if (j==4) {
                _readbuff[5] = 0;
                if (_match(_readbuff, _DATA_RECEIVED)) {
                    // _readbuff is now "+IPD,"
                    String len_ = "";
                    while(sim_serial.available()) {
                        char c;
                        len_ += c = sim_serial.read();
                        if (c == ':') break; // break beause of +IPD,<LEN>:
                    }

                    u16 len = len_.toInt();
                    u16 k;
                    for (k = 0; k<len; k++) {
                        if (k == _RBFLEN && tcp_callback){
                            (*tcp_callback)(_readbuff, _RBFLEN);
                        }
                        _readbuff[k%_RBFLEN] = sim_serial.read();
                    }
                    if (tcp_callback && k%_RBFLEN) 
                        (*tcp_callback)(_readbuff, k%_RBFLEN);

                    return _serial_process_line(resolvers);
                }
            }
        }
        if (j+2 == _RBFLEN) {
            _readbuff[_RBFLEN-1] = 0;
        }

        return m;
    }

    // read buffer index of
    int SIM800m2m::_rbidxof(const String&str) {
        return _bidxof(_readbuff, str);    
    }

    bool SIM800m2m::_serial_rcln(u8 buffpos) {
        char c = sim_serial.read();
        if (c == '\r') return _serial_rcln(buffpos + 1); // skipped
        if (c == '\n') {
            _readbuff[buffpos] = 0;
            return true;
        }
        _readbuff[buffpos] = c;
        return false;
    }

    void SIM800m2m::soft_wait(u32 t) {
        u32 start = millis();
        while (millis() - start < t) {
            while (sim_serial.available()) {
                _serial_process_line();
            }
            delay(100);
        }
    }

    void SIM800m2m::loop() {
        while (sim_serial.available()) {
            _serial_process_line();
        }

        // Checking the TCP connection
        if (millis() - tcp_last_check > 10000) {
            tcp_check_status();
        }
        
        // Auto connect to the GPRS network
        while (!gprs_connected) {
            tries++;
            config_gprs();
            delay(100);
            if (tries > 15) {
                tries = 0;
                Serial.println(F("Failed to connect to the GPRS network"));
                if (gprs_error_callback) 
                    (*gprs_error_callback)();
                return;
            }
        }

        // Auto connect to the TCP server
        if (!tcp_status) {
            if (tcp_auto) {
                while (tries < 15) {
                    tries++;
                    Serial.println(F("[INFO] Trying to connect.."));
                    if (tcp_connect()) break;
                    delay(100);
                }
                if (!tcp_status && tcp_error_callback) 
                    (*tcp_error_callback)();
            }
        } else {
            tries = 0;
        }
    }