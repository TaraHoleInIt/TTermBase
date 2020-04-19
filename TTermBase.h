#ifndef _TTERM_BASE_H_
#define _TTERM_BASE_H_

#include <Arduino.h>
#include <stdint.h>

#define Attrib_Dirty bit( 15 )
#define Attrib_Hidden bit( 14 )
#define Attrib_Reverse bit( 13 )
#define Attrib_Blink bit( 12 )
#define Attrib_Strike bit( 11 )
#define Attrib_Underline bit( 10 )
#define Attrib_Conceal bit( 9 )
#define Attrib_Slowblink bit( 8 )
#define Attrib_Fastblink bit( 7 )

#define Attrib_FR bit( 0 )
#define Attrib_FG bit( 1 )
#define Attrib_FB bit( 2 )

#define Attrib_BR bit( 3 )
#define Attrib_BG bit( 4 )
#define Attrib_BB bit( 5 )

#define Attrib_I bit( 6 )

#define Clear_CursorToEOS 0
#define Clear_CursorToBOS 1
#define Clear_FullScreen 2

class TTermbase {
protected:
    uint8_t* Screen;
    uint16_t* Attrib;

    int x;
    int y;

    int TermLength;
    int TermWidth;
    int TermHeight;

    int _FontWidth;
    int _FontHeight;

    uint16_t DefaultAttrib;
    uint16_t AttribMask;

    uint32_t NextSlowBlink;
    uint32_t NextFastBlink;

    char EscapeString[ 64 ];
    int EscapeLength;
    bool IsInEscape;

    void MarkAsDirty( int Offset, int Length );

    int GetCursorOffset( void );
    void ScrollUp( void );

    virtual size_t DrawGlyph( int x, int y, uint8_t Character, uint16_t Attrib ) = 0;

    virtual void Escape_Clear( void );
    virtual void Escape_SGR( void );
public:
    TTermbase( int DisplayWidth, int DisplayHeight, int FontWidth, int FontHeight );

    virtual size_t write( uint8_t Data );
    //size_t write( const char* StringToWrite );

    void Clear( int Mode );
    void Update( void );
};

#endif
