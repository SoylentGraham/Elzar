#define limit(x,_min,_max)    max( _min, min( x, _max ) )
#define null                  NULL


void Sleep()
{
    delay(1);
}


void DebugPrint(const String& Text)
{
    //    we'll prefix any debug stuff with #
    Serial.print("# ");
    Serial.println( Text );
}


String UrlDecode(const String& Encoded)
{
    //    decode me!
    return Encoded;
}

template<typename T,int MAXSIZE>
class BufferArray
{
public:
    BufferArray() :
        mSize    ( 0 )
    {
    }
    
    void  SetSize(int Size)    {    mSize = limit( Size, 0, MAXSIZE );    }
    void  Clear()              {    SetSize(0);    }
    int   GetSize() const      {    return mSize;    }
    int   MaxSize() const      {    return MAXSIZE;    }
    
    T&    PushBack(const T& Item) 
    {
        T& Tail = Alloc();    
        Tail = Item;    
        return Tail;    
    }
    T&    PushBack()           {    return Alloc();    }
    T&    GetTail()            {    return mData[ GetSize()-1 ];    }
    void  PopBack(T& Item)     {    Item = GetTail();    mSize--;    }
    
    T&    operator[](int Index)    {    return mData[Index];    }
    
    template<typename MATCH>
    const T*    Find(const MATCH& Match) const
    {
        for ( int i=0;    i<mSize;    i++ )
        {
            if ( mData[i] == Match )
                return &mData[i];
        }
        return null;
    }

private:
    //    when we run out of space... we just overwrite the last item. yikes!
    T&    Alloc()
    {
        if ( GetSize() >= MaxSize() )
        {
            DebugPrint( String("BufferArray<") + String(MAXSIZE) + String("> has overflowed") );
            SetSize( MaxSize() );
            return GetTail();
        }
        return mData[mSize++];
    }
    
private:
    int    mSize;
    T      mData[MAXSIZE];
};
   

class HttpVar
{
public:
    String    mName;
    String    mValue;
    
    boolean   operator==(const String& Name) const    {    return mName == Name;    }
};

class HttpRequest
{
public:
    HttpRequest() :
        mHeaderComplete   ( false ),
        mContentComplete  ( false ),
        mGet              ( true ),
        mKeepAlive        ( true ),    //    1.1 default
        mContentLength    ( 0 ),
        mClientIP         ( 1,2,3,4 )
    {
    }

    boolean           IsComplete() const                       {    return mHeaderComplete && mContentComplete;    }
    const HttpVar*    GetVariable(const String& Name) const    {    return mVariables.Find( Name );    }
    void              SetUrl(const String& Url);

public:
    bool      mHeaderComplete;    //    true when request is completed
    bool      mContentComplete;    //    true when request is completed
    bool      mGet;
    String    mUrl;
    bool      mKeepAlive;
    int       mContentLength;    
    String    mContent;         //    url decoded content
    IPAddress mClientIP;
    BufferArray<HttpVar,10>    mVariables;    //    GET & POST vars all merged together
};



template<int BUFFERSIZE>
void ParseHttpVariables(const String& VarString,BufferArray<HttpVar,BUFFERSIZE>& Variables)
{
    int i = 0;
    HttpVar* pVariable = null;

    //    examples;    
    //    X=Y&
    //    X&
    //    X=&
    //    walk through string
    while ( i < (int)VarString.length() )
    {
        int NextEquals = VarString.indexOf('=',i);
        int NextAmp = VarString.indexOf('&',i);
        
        //    ignore the next equals if next amp comes first
        if ( NextAmp != -1 && NextAmp < NextEquals )
            NextEquals = -1;
        
        //    no more, finish current variable (and make one if we haven't got one!)
        if ( NextEquals == -1 && NextAmp == -1 )
        {
            if ( pVariable )
            {
                pVariable->mValue = UrlDecode(VarString.substring( i ));
                pVariable = NULL;
            }
            else
            {
                pVariable = &Variables.PushBack();
                pVariable->mName = UrlDecode(VarString.substring( i ));
                pVariable->mValue = "";
            }
            break;
        }
        else if ( NextEquals != -1 )
        {
            //    new variable
            pVariable = &Variables.PushBack();
            pVariable->mName = UrlDecode(VarString.substring( i,NextEquals ));
            pVariable->mValue = "";
            i = NextEquals+1;
        }
        else if ( NextAmp != -1 )
        {
            //    end of a var
            //    safety!
            if ( !pVariable )
            {
                pVariable = &Variables.PushBack();
                pVariable->mName = UrlDecode(VarString.substring( i,NextAmp ));
                pVariable->mValue = "";
            }
            else
            {
                pVariable->mValue = UrlDecode(VarString.substring( i,NextAmp ));
            }
            pVariable = NULL;
            i = NextAmp+1;
        }
        else
        {
            //    error!
            String Debug;
            Debug += "Reached unexpected point in ParseHttpVariables; ";
            Debug += "NextEquals: " + String(NextEquals);
            Debug += "NextAmp: " + String(NextAmp);
            DebugPrint(Debug);
            break;
        }
    }
}

void HttpRequest::SetUrl(const String& Url)
{
        //    extract any variables in the url; find a question mark...
        int VariableStart = Url.indexOf('?');
        if ( VariableStart >= 0 )
        {
            ParseHttpVariables( Url.substring( VariableStart+1 ), mVariables );
            mUrl = Url.substring( 0, VariableStart );
        }
        else
        {
            mUrl = Url;
        }
        
        //    URI decode the url
        mUrl = UrlDecode( mUrl );
        
        //    tidy url
        //    modify the url, if it's NOT the root, remove the prefix slash
        //    for readable code, we leave the slash if the request is just "/"
        if ( mUrl.length() > 1 && mUrl[0] == '/' )
        {
            //    more efficent!
            //Request.mUrl = Request.mUrl.substring( 1 );
            mUrl[0] = ' ';
            mUrl.trim();
        }
}

boolean IsFileUrl(const String& Url)
{
    int DotPos = Url.lastIndexOf('.');
    if ( DotPos == -1 )
        return false;
        
    int SlashPos = Url.lastIndexOf('/',DotPos);
    
    //    if there is a dot AFTER the last slash (if any) then assume it's a file.name
    return ( DotPos > SlashPos );
}

IPAddress GetClientIP(const EthernetClient& Client)
{
    IPAddress ip(1,2,3,4);
//    ip[0] = IINCHIP_READ(Sn_DIPR0(Client._sock));
//    ip[1] = IINCHIP_READ(Sn_DIPR0(Client._sock)+1);
//    ip[2] = IINCHIP_READ(Sn_DIPR0(Client._sock)+2);
//    ip[3] = IINCHIP_READ(Sn_DIPR0(Client._sock)+3);
    return ip;
}

String IPToString(const IPAddress& Ip)
{
    String Text;
    Text += String( Ip[0], DEC );
    Text += ".";
    Text += String( Ip[1], DEC );
    Text += ".";
    Text += String( Ip[2], DEC );
    Text += ".";
    Text += String( Ip[3], DEC );
    return Text;
}

int StringToInt(const String& s)
{
    char buffer[s.length() + 1];
    s.toCharArray(buffer, sizeof(buffer) );
    return atoi( buffer ); 
}


//    read all the data from a client (don't seperate by line)
boolean ReadHttpContent(EthernetClient& Client,HttpRequest& Request)
{
    //    nowt to read
    if ( Request.mContentLength == 0 )
        return Client.connected();
    
    int DataRead = 0;
    //    gr: need to url-decode these
    while ( Client.connected() && DataRead < Request.mContentLength )
    {
        //    todo; decode
        if (Client.available()) 
        {
            char c = Client.read();
            DataRead++;
            Request.mContent += c;
        }
    }
    return false;
}


    
//    read a line from a client (ie, a header)
boolean ReadHttpLine(EthernetClient& Client,String& Line)
{
    Line = "";
    while (Client.connected()) 
    {
        //    keep reading data until we hit a line return (\r on it's own is a valid request)
        if (Client.available()) 
        {
            char c = Client.read();

            if ( c == '\n' )
            {
                //DebugPrint( String("Read line [")+Line+"]" );
                return true;
            }
                
            //    cut out \n's - as we're seperating at \r's the first char of a 2nd line will be a \n
            if ( c == '\r' )    
            {
                c = c;    //    discard this char
                //Line += "\\r";
            }
            else if ( c == '\n' )    
                Line += "\\n";
            else 
                Line += c;
        }
    }
    //    will be incomplete?
    return false;
}


boolean ReadHttpHeader(EthernetClient& Client,HttpRequest& Request)
{
    //    read a line...
    String Header;
    if ( !ReadHttpLine( Client, Header ) )
    {
        DebugPrint("Error reading line");
        return false;
    }
    
    //    empty line is end of headers (\n \n)
    if ( Header.length() == 0 )
    {
        DebugPrint("Header complete.");
        Request.mHeaderComplete = true;
        return true;
    }
     
    //    parse lines that we care about...
    //    Host:    //    me, I know that.
    //    Connection:    //    keep-alive? tough luck
    //    Cache-Control:    //    cache irrelevent
    //    User-Agent:    //    dont care
    if ( Header.startsWith("Content-Length: ") )
    {
        String LengthString = Header.substring( strlen("Content-Length: ") );
        Request.mContentLength = StringToInt( LengthString );
        return true;
    }
    
    //    discarded header...
    String Debug;
    Debug += "Discarded header; " + Header;
    DebugPrint( Debug );
    
    return true;
}
    


boolean SendHttp(EthernetClient& Client,HttpRequest& ClientRequest,const int Code,const String& Content)
{
    if ( !Client.connected() )
        return false;
    
    Client.print( "HTTP/1.1 " );
    Client.print( Code );
    Client.println(" OKMAYBE");
    Client.println("Content-Type: text/html");
    Client.println("Connnection: close");
    Client.println();
    Client.println( Content );
    return true;
}


boolean ReadHttp(EthernetClient& Client,HttpRequest& Request)
{
    Request.mClientIP = GetClientIP( Client );
    
    //    read first line
    String RequestLine;
    if ( !ReadHttpLine( Client, RequestLine ) )
        return false;

    //    verify end of the line
    if ( !RequestLine.endsWith(" HTTP/1.1") && !RequestLine.endsWith(" HTTP/1.0") )
    {
        DebugPrint("HTTP Request line doesn't end with HTTP/1.X");
        return false;
    }
    RequestLine = RequestLine.substring( 0, RequestLine.length()-strlen(" HTTP/1.X") );

    //    pull out the GET/POST request, and clip it out. leaving just the request url.
    Request.mGet = RequestLine.startsWith("GET ");
    if ( Request.mGet )
        RequestLine = RequestLine.substring( strlen("GET ") );
    else if ( RequestLine.startsWith("POST ") )
        RequestLine = RequestLine.substring( strlen("POST ") );
    else
        return false;    //    not GET or POST!

    Request.SetUrl( RequestLine );

    String Debug;
    Debug += "HttpRequest; " + Request.mUrl + " (";
    Debug += Request.mGet ? "get" : "post";
    Debug += ")";
    DebugPrint( Debug );
    
    //    read (and parse) http headers
    while ( !Request.mHeaderComplete )
    {
        if ( !ReadHttpHeader( Client, Request ) )
            return false;
    }
    
    //    read content
    if ( !ReadHttpContent( Client, Request ) )
        return false;
        
    //    debug all our vars
    for ( int i=0;    i<Request.mVariables.GetSize();    i++ )
    {
        Debug = "Var: " + Request.mVariables[i].mName + "= [" + Request.mVariables[i].mValue + "]";
        DebugPrint( Debug );
    }
  
    return true;
}



