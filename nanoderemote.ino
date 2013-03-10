#include <EtherCard.h>
#include <NanodeUNIO.h>
 
//
// remote by Mark VandeWettering
//
// a reimplementation of the "RESTduino" idea, first published at:
//     http://jasongullickson.posterous.com/restduino-arduino-hacking-for-the-rest-of-us
//
// I decided to rewrite this application because:
//   1. originally, it was written for the Wiznet based Ethernet Shield, but I have
//      mostly Nanodes, based upon the ENC28J60
//   2. It was ported to the Ethershield library by Andrew Lindsay, but Andy has decided
//      that he's decided not to continue development of that library, deferring to the
//      EtherCard library being developed by JeeLabs
//   3. I thought there was a lot of cool additions that could be made!  Okay, I haven't
//      implemented any at the moment, but I will!
//
// modifications by Leon Wright 2013-02-26 - changed how it controls analog pins and
// how information is returned.

 
#define DEBUG       1
#define BUFFER_SIZE     (900)
 
byte Ethernet::buffer[BUFFER_SIZE] ;
BufferFiller bfill;
 
char okHeader[] PROGMEM =
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Pragma: no-cache\r\n"
    "\r\n" ;
     
char responseHeader[] PROGMEM =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "\r\n" ;
 
void
homepage(BufferFiller& buf)
{
  buf.emit_p(PSTR(
    "$F"
    "<html>"
    "<head>"
    "<title>webserver remote</title>"
    "<style type=\"text/css\">"
    "body   { width: 640px ; }"
    "</style>"
    "</head>"
    "<body>"
    "<h1>webserver remote</h1>"
    "<p>"
    "This webserver is running on a <a href=\"http://nanode.eu\">Nanode</a>, an "
    "Arduino-compatible microcontroller with web connectivity. It uses the <a"
    "href=\"https://github.com/jcw/ethercard\">EtherCard library</a> written by "
    "<a href=\"http://jeelabs.net/projects/cafe/wiki/EtherCard\">JeeLabs</a>.  "
        "Inspired by Jason Gullickson's <a href=\"http://www.youtube.com/watch?v=X-s2se-34-g\">RESTduino</a>."
    "</p>"
    "<hr/>"
    "<p style=\"text-align: right;\">Written by Mark VandeWettering</p>"
    "<p style=\"text-align: right;\">Minor modifications by Leon Wright - 2013-02-26</p>"
    "</body>"
    "</html>"
    ), okHeader) ;
}
 
#define CODE_ERROR          (-1)
#define CODE_WRITE          (0)
#define CODE_READ           (1)
#define CODE_ANALOG_WRITE   (2)
#define CODE_INDEX          (3)
 
int
cmdparse(const char *cmd, int &pin, int &val)
{
    const char *cp = cmd+4 ;
 
    if (*cp++ == '/') {
    if (*cp == ' ') {
        return CODE_INDEX ;
    } else if (isdigit(*cp)) {
        pin = atoi(cp) ;
        while (isdigit(*cp)) cp++ ;
        if (*cp == ' ') {
        return CODE_READ ;
        } else if (*cp == '/') {
        cp++ ;
        if (isdigit(*cp)) {
            val = atoi(cp) ;
            while (isdigit(*cp)) cp++ ;
            if (*cp == ' ')
            return CODE_ANALOG_WRITE ;
            else
            return CODE_ERROR ;
        } else if (strncmp(cp, "HIGH", 4) == 0) {
            cp += 4 ;
            if (*cp == ' ') {
            val = 1 ;
            return CODE_WRITE ;
            } else
            return CODE_ERROR ;
        } else if (strncmp(cp, "LOW", 3) == 0) {
            cp += 3 ;
            if (*cp == ' ') {
            val = 0 ;
            return CODE_WRITE ;
            } else
            return CODE_ERROR ;
        }
        } else
        return CODE_ERROR ;
    } else {
        return CODE_ERROR ;
    }
    } else {
    return CODE_ERROR ;
    }
}
 
 
void
setup()
{
    byte mac[6] ;
 
    Serial.begin(19200) ;
    Serial.println("\nbrainwagon remote webserver\n") ;
 
    NanodeUNIO unio(NANODE_MAC_DEVICE) ;
    unio.read(mac, NANODE_MAC_ADDRESS, 6) ;
 
    if (ether.begin(sizeof Ethernet::buffer, mac) == 0) {
    Serial.println("Failed to access Ethernet controller.") ;
    for (;;) ;
    }
 
    if (ether.dhcpSetup()) {
    ether.printIp("IP:  ", ether.myip) ;
    ether.printIp("GW:  ", ether.gwip) ;
    ether.printIp("DNS: ", ether.dnsip) ;
    } else {
    Serial.println("DHCP failed.\n") ;
    for (;;) ;
    }
}
 
void
loop()
{
  word len = ether.packetReceive() ;
  word pos = ether.packetLoop(len) ;
  int pin, val, inValue ;
   
  if (pos) {
    bfill = ether.tcpOffset() ;
    char *data = (char *) Ethernet::buffer + pos ;
    Serial.println(data) ;
    switch (cmdparse(data, pin, val)) {
    case CODE_READ:
        inValue = digitalRead(pin) ;
        Serial.println(inValue);
        if(inValue == 0){
          bfill.emit_p(PSTR(
          "$F"
          "{\"$S\" : \"$D\",\"$S\" : \"$S\"}"),
          responseHeader,
          "pin", pin, "value", "LOW" ) ;
        }
              
        if(inValue == 1){
          bfill.emit_p(PSTR(
          "$F"
          "{\"$S\" : \"$D\",\"$S\" : \"$S\"}"),
          responseHeader,
          "pin", pin, "value", "HIGH" ) ;
        }
        break ;
    case CODE_WRITE:
        pinMode(pin, OUTPUT) ;
        digitalWrite(pin, val ? HIGH : LOW) ;
        bfill.emit_p(PSTR(
         "$F"
          "{\"$S\" : \"$S\"}"),
          responseHeader,
          "result", "success") ;
        break ;
    case CODE_ANALOG_WRITE:
        pinMode(pin, OUTPUT) ;
        analogWrite(pin, val) ;
        bfill.emit_p(PSTR(
            "$F"
            "{\"$S\" : \"$S\"}"),
          responseHeader,
          "result", "success") ;
        break ;
    case CODE_INDEX:
        homepage(bfill) ;
        break ;
    case CODE_ERROR:
    default:
        bfill.emit_p(PSTR(
            "HTTP/1.0 404 Not Found\r\n"
            "Content-Type: text/html\r\n"
            "\r\n"
            "<html><body><h1>404 Not Found</h1></body></html>")) ;
        break ;
    }
    ether.httpServerReply(bfill.position()) ;
  }
}
