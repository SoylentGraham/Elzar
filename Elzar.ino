#include <SPI.h>
#include <Ethernet.h>
#include "Common.h"

//  pin 9 on ethernet board is LED. it's also TX
int LedPin = 9;

//    hard-coded ethernet stuff
byte MacAddress[] = {    0x90,0xA2,0xDA,0x0D,0x74,0x6E    };

//    Initialize the Ethernet server library on a port
EthernetServer HttpServer(80);


void InitLed()
{
    //    initialize the digital pin as an output.
    pinMode(LedPin, OUTPUT);
    
    //    turn it off by default. seems to be on at first...
    EnableLed(false);
}

void EnableLed(boolean Enable)
{
    digitalWrite( LedPin, Enable ? HIGH : LOW );
}

void InitDebugSerial()
{
    //    the debug serial
    Serial.begin(9600);
    
    //    wait for serial port to connect ("Needed for leonardo only")
    while ( !Serial )
    {
            ;
    }
}


void InitHttpServer()
{
    HOST_NAME = "Elzar";
    
    //    init the Ethernet lib
    Ethernet.begin( MacAddress );
    
    //    start the server
    HttpServer.begin();
    
    //    debug 
    String Debug;
    Debug += "Http server at ";
    Debug += IPToString( Ethernet.localIP() );
    DebugPrint( Debug );
}


boolean OnHttpClient(EthernetClient& Client)
{
    HttpRequest Request;
    if ( !ReadHttp( Client, Request ) )
    {
        DebugPrint("Failed to read HTTP from client");
        return false;
    }

    //    looking for a file? don't think so (favicon.ico probably...)
    if ( IsFileUrl( Request.mUrl ) )
    {
        SendHttp( Client, Request, 404, Request.mUrl + " not found." );
        return false;
    }
  
    //    show content
    DebugPrint( String(" ** Print this; ") + Request.mUrl );
    DebugPrint( Request.mContent );
    DebugPrint( " ** Done ** ");

    //    generate response
    int ResultCode = 200;
    String ResultContent = "Thaaayyynnnkks.";

    //    send response
    if ( !SendHttp( Client, Request, ResultCode, ResultContent ) )
        return false;

    //    close client regardless. we don't have a need for a lot of bandwidth re-use :P
    //    gr: let connection-close in our response close the connection and wait for client to do so?
    Client.stop();
    return true;
} 
  

// the setup routine runs once when you press reset:
void setup()
{
    InitDebugSerial();
    InitLed();
    InitHttpServer();
}

// the loop routine runs over and over again forever:
void loop() 
{
    Sleep();
    
    //    listen for incoming clients
    EthernetClient Client = HttpServer.available();
    if (Client) 
    {
        EnableLed(true);
        OnHttpClient( Client );
        //    kick em off again
        if ( Client.connected() )
        {
            Client.stop();
            DebugPrint("kicked off client");
        }
        else
        {
            DebugPrint("Client disconnected");
        }            
        EnableLed(false);
    }
}
