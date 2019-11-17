#include "Arduino.h"
#include <SoftwareSerial.h>
#define VERBOSE 1

#if VERBOSE
#define sprintln(x) Serial.println(x)
#else
#define sprintln(x)
#endif

void reset_ino();

enum SerialMode {
    DATA_MODE,
    COMMAND_MODE,
};

enum TCPMode {
    DISABLED,
    DEFAULT_MODE,
    TRANSPARENT_MODE,
};

class SIM800L {        
  public:
    SoftwareSerial  sim_serial;     // Emulated Serial connection using digital pins
    SerialMode      serial_mode;    // 
    TCPMode         tcp_mode;       // 
    unsigned int    bprate;         // 
    u8 RX, TX, RST, DTR;            // PINs used for the SIM800L
    bool            tcp_connected;     // false: no connection, true: connected
    const String    _ok = "OK";

    /**
     * RX       -> Serial PIN for receiving data.
     * TX       -> Serial PIN for sending data.
     * RST      -> PIN used to RESET the module.
     * DTR      -> PIN used to SLEEP mode and switching between data/command modes. 
     * bprate   -> Baud rate of the serial connection.
    */
    SIM800L (u8 RX, u8 TX, u8 RST, u8 DTR, unsigned int bprate) : sim_serial(TX,RX) {
        this->bprate = bprate;
        this->RX = RX;
        this->TX = TX;
        this->RST = RST;
        this->DTR = DTR;
        this->serial_mode = COMMAND_MODE;
        this->tcp_mode    = DISABLED;
        this->tcp_connected  = false;
    }
    
    /**
     * bsl_scape : Back Slash Escape
     * This function returns the escape character
     * works for \n \r and \t
    */
    char b_escape(char code) {
        if (code == 'n') return '\n';       // newline feed
        if (code == 'z') return (char) 26;  // CTRL+Z char code from ASCII table
        if (code == 'r') return '\r';       // carrier return
        if (code == 't') return '\t';       // tabulation character
        return code;
    }

    /**
     * Setup function: sets up the serial communication and used pins
     * for the SIM800L to work properly.
    */
    void setup() {
        // Setup serial connection to the module
        sim_serial.begin(bprate);
        sim_serial.setTimeout(3000);

        // Config reset and dtr pins
        pinMode(RST, OUTPUT);
        pinMode(DTR, OUTPUT);

        // Pull up the pins from ground (default mode)
        digitalWrite(RST, HIGH);
        digitalWrite(DTR, HIGH);
    }

    void assert(bool val, bool rst = true) {
        if (!val) {
            sprintln("INVALID PROGRAM FLOW");
            if (rst) {
                sprintln("RESETING");
                delay(500);
                reset_ino();
            }
        }
    }

    /**
     * Reset function: sets the RST PIN into low mode for 2 seconds,
     * wait for the module to reset and sends AT test command.
    */
    void reset() {
        sprintln(F("Reseting..."));
        digitalWrite(RST, LOW);
        delay(2000);
        digitalWrite(RST, HIGH);
        delay(7000);
        sprintln(F("Testing..."));
        sprintln("Recebi: " + sendCommand("AT", _ok));
    }

    #define ok_or_rst(cmd) assert(sendCommand((cmd), _ok))
   
    void setupConfig() {
        ok_or_rst("AT");                    // Testa o handshake
        sendCommand("AT+CSQ", _ok);         // Testa o nível do sinal da rede
        sendCommand("AT+CCID", _ok);        // Verifica as informações do cartão SIM (chip)
        sendCommand("AT+CREG?", _ok);       // Verifica se está registrado na REDE
        sendCommand("AT&D2", _ok);          // Sets the funcion of DTR pin
        sendCommand("AT+IFC=1,1", _ok);     // Sets no flow control
        //setupGSM();
        setupGPRS();
    }

    void setupGSM() {
        sendCommand("AT+CMGF=1", _ok);         //
        sendCommand("AT+CNMI=2,2,0,0,0", _ok); //
        sendCommand("ATX4", _ok);              //
        sendCommand("AT+COLP=1", _ok);         //
    }

    void setupGPRS() {
        // Configurando APN
        sendCommand("AT+CSTT=\"gprs.oi.com.br\",\"guest\",\"guest\"", _ok);
        sendCommand("AT+CIICR", _ok);       // Habilitando o circuito do GPRS
        sendCommand("AT+CIFSR");            // Verifica o IP recebido
        sendCommand("AT+SAPBR=1,1", _ok);   // 
        sendCommand("AT+SAPBR=2,1", _ok);   //
        if (!setTCPMode(TRANSPARENT_MODE)) setTCPMode(DEFAULT_MODE);
    }

    bool setTCPMode(TCPMode mode) {
        if (mode == DISABLED) {
            // TODO
            this->tcp_mode = mode;
            sprintln(F("[INFO] TCP mode set to disabled"));
        }
        if (mode == DEFAULT_MODE) {
            this->tcp_mode = mode;
            sprintln(F("[INFO] TCP mode set to default mode"));
        }
        if (mode == TRANSPARENT_MODE) {
            if (sendCommand("AT+CIPMODE=1" , _ok)) {
                this->tcp_mode = mode;
                sprintln(F("[INFO] TCP mode set to transparent mode"));
                return true;
            }
        }
        return false;
    }

    bool TCPconnect(String host, int port) {
        if(sendCommand("AT+CIPSTART=\"TCP\",\"" + host + "\"," + String(port), "CONNECT OK")) {
            this->tcp_connected = true;
            return true;
        }
        if (!this->tcp_connected) sendCommand("AT+CIPCLOSE", _ok);
        return false;
    }

    bool TCPsend(String data) {
        if (this->tcp_connected && sendCommand("AT+CIPSEND=" + String(data.length()), ">")) {
            delay(200);
            sim_serial.print(data.c_str());
            while(sim_serial.available()) {
                String s = serialReadLine();
                onReceive(s);
                if (s.startsWith("SEND OK")) return true;
            }
        }
        return false;
    }

    void enviaSMS(String telefone, String mensagem) {
        delay(500);
        sim_serial.print("AT+CMGS=\"" + telefone + "\"");
        sim_serial.print((char)13); 
        delay(500);
        sim_serial.print(mensagem + "\n");
        delay(1000);
        sim_serial.print((char)26); 
    }

    String requestLocation() {
        String resp = "FAIL";
        sendCommand("AT+CIPGSMLOC=1,1", _ok, &resp);
        return resp;
    }

    void onReceive(String str) {
        sprintln("<?-- " + str);
        if (str.startsWith("CLOSED")){
            this->tcp_connected = false;
        }
    }

    void SERIALcommand(String str) {
        if (str.startsWith("setup")) {
            setupConfig();
        } else if (str.startsWith("reset")) {
            reset();
        }
    }

    void loop() {
        while (sim_serial.available()) {
            onReceive(serialReadLine());
        }
        if (Serial.available()) {
            String str = Serial.readString();
            if (str.startsWith("AT")) {
                Serial.println("[CMD OUT] " + sendCommand(str));
            } else if (str.startsWith(">")) {
                sim_serial.println(str.substring(1));
            } else if (str.startsWith("/")) {
                SERIALcommand(str.substring(1));
            }
        }

    }

    String processCommand(String cmd) {
        String proc = "";
        const char* cs = cmd.c_str();
        for(unsigned char i=0; i<cmd.length(); i++) {
            if (cs[i] == '\\') {
                i++;
                proc += b_escape(cs[i]);
            } else if (cs[i] == '\n' || cs[i] == '\r') {
                break;
            } else proc += cs[i];
        }
        return proc;
    }


    bool checkCommand(String rcv, String expect) {
        u8 l1, l2, i;
        rcv.replace("\r\n", "\n");
        expect.replace("\r\n", "\n");
        if (abs((l1=rcv.length()) - (l2=expect.length())) <= 2) {
            for (i=0;i<min(l1,l2);i++) {
                if (rcv.charAt(i) != expect.charAt(i)){
                    sprintln("[!] " + rcv + " != " + expect);
                    sprintln("Diff:" + String((int) rcv.charAt(i)) + " and " + String((int) expect.charAt(i)));
                    return false;
                }
            }
            return true;
        }
        sprintln("[!] " + rcv + " !+ " + expect);
        return false;
    }

    /**
     * Sends the command via serial to the SIM800L
     * and waits <timeout> seconds.
    */
    bool sendCommand(String cmd, String expect="", String* rsp = NULL, u8 timeout=15) {
        String pc = processCommand(cmd);
        sim_serial.setTimeout(1000);
        sim_serial.println(pc);
        sprintln("--> " + pc);
        String rcv;
        String ret;
        bool j = false;
        for(u8 i = 0; i<timeout*10; i++) {
            delay(100);
            if (sim_serial.available()) {
                rcv = serialReadLine();
                if (!j) {
                    if (j = checkCommand(rcv, pc)) continue;
                    onReceive(rcv);
                } else {
                    sprintln("<-- " + rcv);
                    ret += "\n" + rcv;
                    if (!expect.length() || rcv.indexOf(expect) != -1){
                        if (rsp) *rsp = ret;
                        return true;
                    }
                }
            }
        }
        delay(100);
        if(rsp) *rsp = ret;
        return false;
    }

    /**
     * Reads a line from the SIM800 serial connection.
    */
    String serialReadLine() {
        String s = ""; char c;
        while(sim_serial.available()) {
            c = sim_serial.read();
            if (c == '\r') continue;
            if (c == '\n') break;
            s+= c;
        }
        return s;
    }

    /**
     *  Clears the input buffer from the SIM800L serial connection.
     */
    void serialFlush(){
        while(sim_serial.available() > 0) {
            char t = sim_serial.read();
        }
    }   

    /**
     * Sets the serial mode to command mode or data mode.
    */
    bool setSerialMode(SerialMode new_mode) {
        if (this->serial_mode == new_mode) return true;

        if (new_mode == COMMAND_MODE) {
            serialFlush();
            digitalWrite(DTR, LOW);
            delay(1000);
            digitalWrite(DTR, HIGH);

            if (sim_serial.available() && serialReadLine().equals(_ok)) {
                return true;
            }
            return false;
        } else if (new_mode == DATA_MODE) {
            if (this->tcp_mode == TRANSPARENT_MODE) {
                sendCommand("ATO", _ok);
                return true;
            }
        }

        return false;
    }

};
