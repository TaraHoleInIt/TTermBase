// Copyright (c) 2020 Tara Keeling
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT
#include <Arduino.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <TTermBase.h>

#define MakeColorF( r, g, b ) ( ( r == 1 ? Attrib_FR : 0 ) | ( g == 1 ? Attrib_FG : 0 ) | ( b == 1 ? Attrib_FB : 0 ) )
#define MakeColorB( r, g, b ) ( ( r == 1 ? Attrib_BR : 0 ) | ( g == 1 ? Attrib_BG : 0 ) | ( b == 1 ? Attrib_BB : 0 ) )

TTermbase::TTermbase( int DisplayWidth, int DisplayHeight, int FontWidth, int FontHeight ) {
    _FontWidth = FontWidth;
    _FontHeight = FontHeight;

    TermWidth = ( DisplayWidth / FontWidth );
    TermHeight = ( DisplayHeight / FontHeight );
    TermLength = TermWidth * TermHeight;

    DefaultAttrib = Attrib_Dirty | MakeColorF( 1, 1, 1 ) | MakeColorB( 0, 0, 0 );
    AttribMask = DefaultAttrib;

    memset( EscapeString, 0, sizeof( EscapeString ) );
    IsInEscape = false;
    EscapeLength = 0;

    NextSlowBlink = 0;
    NextFastBlink = 0;

    Screen = new uint8_t[ TermLength ];
    Attrib = new uint16_t[ TermLength ];

    Clear( Clear_FullScreen );
}

void TTermbase::Clear( int Mode ) {
    int Offset = x + ( y * TermWidth );
    int Length = 0;
    int i = 0;

    Serial.print( "Clear: " );
    Serial.println( Mode );

    switch ( Mode ) {
        case Clear_CursorToBOS: {
            memset( Screen, 0, Offset );
            memset( Attrib, 0, Offset * 2 );

            MarkAsDirty( 0, Offset );
            break;
        }
        case Clear_FullScreen: {
            x = 0;
            y = 0;

            memset( Screen, 0, TermLength );
            memset( Attrib, 0, TermLength * 2 );

            MarkAsDirty( 0, TermLength );
            break;
        }
        case Clear_CursorToEOS: 
        default: {
            memset( &Screen[ Offset ], 0, TermLength - Offset );
            memset( &Attrib[ Offset ], 0, ( TermLength - Offset ) * 2 );

            MarkAsDirty( Offset, ( TermLength - Offset ) );
            break;
        }
    };
}

size_t TTermbase::write( uint8_t Data ) {
    int Offset = GetCursorOffset( );

    if ( IsInEscape == true ) {
        switch ( Data ) {
            case '[': break;
            case 'a' ... 'z':
            case 'A' ... 'Z': {
                EscapeString[ EscapeLength ] = 0;
                IsInEscape = false;

                switch ( Data ) {
                    case 'H':
                    case 'h': {
                        Escape_CursorPosition( );
                        break;
                    }
                    case 'J':
                    case 'j': {
                        Escape_Clear( );
                        break;
                    }
                    case 'M':
                    case 'm': {
                        Escape_SGR( );
                        break;
                    }
                    default: {
                        Serial.print( "Unknown code (" );
                        Serial.print( ( char ) Data );
                        Serial.print( ") / " );
                        Serial.println( EscapeString );

                        break;
                    }
                };

                break;
            }
            default: {
                EscapeString[ EscapeLength++ ] = Data;
                break;
            }
        };
    } else {
        // Should never be below zero
        x = x < 0 ? 0 : x;
        y = y < 0 ? 0 : y;
        
        switch ( Data ) {
            case '\x1b': {
                IsInEscape = true;
                EscapeLength = 0;

                break;
            }
            case '\r': {
                x = 0;
                break;
            }
            case '\n': {
                x = 0;
                y++;

                break;
            }
            default: {
                Attrib[ Offset ] = AttribMask;
                Screen[ Offset ] = Data;
                x++;

                break;
            }
        };

        if ( x >= TermWidth ) {
            x = 0;
            y++;
        }

        if ( y >= TermHeight ) {
            y = TermHeight - 1;
            x = 0;

            ScrollUp( );
        }
    }

    return 1;
}

/*
size_t TTermbase::write( const char* StringToWrite ) {
    size_t Length = 0;

    while ( *StringToWrite ) {
        Length+= write( *StringToWrite++ );
    }

    return Length;
}
*/

int TTermbase::GetCursorOffset( void ) {
    return x + ( y * TermWidth );
}

void TTermbase::Update( void ) {
    uint32_t Now = millis( );
    int Offset = 0;
    int Row = 0;
    int Col = 0;

    for ( Row = 0; Row < TermHeight; Row++ ) {
        for ( Col = 0; Col < TermWidth; Col++ ) {
            Offset = Col + ( Row * TermWidth );

            if ( Attrib[ Offset ] & Attrib_Slowblink && ! ( Attrib[ Offset ] & Attrib_Fastblink ) ) {
                if ( Now >= NextSlowBlink ) {
                    Attrib[ Offset ] ^= Attrib_Conceal;
                    Attrib[ Offset ] |= Attrib_Dirty;
                }
            }

            if ( Attrib[ Offset ] & Attrib_Fastblink && ! ( Attrib[ Offset ] & Attrib_Slowblink ) ) {
                if ( Now >= NextFastBlink ) {
                    Attrib[ Offset ] ^= Attrib_Conceal;
                    Attrib[ Offset ] |= Attrib_Dirty;
                }
            }

            if ( Attrib[ Offset ] & Attrib_Dirty ) {
                DrawGlyph( Col * _FontWidth, Row * _FontHeight, Screen[ Offset ], Attrib[ Offset ] );
                Attrib[ Offset ] &= ~Attrib_Dirty;
            }
        }
    }

    if ( Now >= NextSlowBlink ) {
        NextSlowBlink = Now + 1000;
    }

    if ( Now >= NextFastBlink ) {
        NextFastBlink = Now + 400;
    }
}

void TTermbase::ScrollUp( void ) {
    int Row = 0;

    for ( Row = 1; Row < TermHeight; Row++ ) {
        memcpy( &Screen[ ( Row - 1 ) * TermWidth ], &Screen[ Row * TermWidth ], TermWidth );
        memcpy( &Attrib[ ( Row - 1 ) * TermWidth ], &Attrib[ Row * TermWidth ], TermWidth * 2 );
    }

    memset( &Screen[ ( TermHeight - 1 ) * TermWidth ], 0, TermWidth );
    memset( &Attrib[ ( TermHeight - 1 ) * TermWidth ], 0, TermWidth * 2 );

    // mark entire screen as dirty
    MarkAsDirty( 0, TermLength );
}

void TTermbase::MarkAsDirty( int Offset, int Length ) {
    int i = 0;

    for ( i = Offset; i < Length; i++ ) {
        Attrib[ i ] |= Attrib_Dirty;
    }
}

void TTermbase::Escape_Clear( void ) {
    int Mode = strtol( EscapeString, NULL, 10 );

    // If mode doesn't parse correctly just clear to the end of the screen
    Mode = ( Mode == LONG_MIN || Mode == LONG_MAX ) ? 0 : Mode;

    Clear( Mode );
}

// fg color - 30
// 3 bits B,G,R
//        3 2 1
void TTermbase::Escape_SGR( void ) {
    uint16_t Temp = AttribMask;
    char* Tok = NULL;
    int Value = 0;

    Tok = strtok( EscapeString, ";" );

    while ( Tok ) {
        switch ( ( Value = atoi( Tok ) ) ) {
            // defaults
            case 0: {
                Temp = DefaultAttrib;
                break;
            }
            // bold
            case 1: {
                Temp |= Attrib_I;
                break;
            }
            // not bold nor dim
            case 22: {
                Temp &= ~Attrib_I;
                break;
            }
            // underline
            case 4: {
                Temp |= Attrib_Underline;
                break;
            }
            // slow blink
            case 5: {
                Temp |= Attrib_Slowblink;
                break;
            }
            // fast blink
            case 6: {
                Temp |= Attrib_Fastblink;
                break;
            }
            // reverse
            case 7: {
                Temp |= Attrib_Reverse;
                break;
            }
            // conceal
            case 8: {
                Temp |= Attrib_Conceal;
                break;
            }
            // strike
            case 9: {
                Temp |= Attrib_Strike;
                break;
            }
            // no underline
            case 24: {
                Temp &= ~Attrib_Underline;
                break;
            }
            // no blink
            case 25: {
                Temp &= ~Attrib_Slowblink;
                Temp &= ~Attrib_Fastblink;

                break;
            }
            // no conceal
            case 28: {
                Temp &= ~Attrib_Conceal;
                break;
            }
            // no strike
            case 29: {
                Temp &= ~Attrib_Strike;
                break;
            }
            // foreground color
            case 30 ... 37: {
                Value-= 30;

                // clear all color bits
                Temp &= ~( Attrib_FR | Attrib_FG | Attrib_FB );

                // test each bit
                Temp |= ( Value & 0x01 ) ? Attrib_FR : 0; // red
                Temp |= ( Value & 0x02 ) ? Attrib_FG : 0; // green
                Temp |= ( Value & 0x04 ) ? Attrib_FB : 0; // blue

                break;
            }
            // background color
            case 40 ... 47: {
                Value-= 40;

                // clear all color bits
                Temp &= ~( Attrib_BR | Attrib_BG | Attrib_BB );

                // test each bit
                Temp |= ( Value & 0x01 ) ? Attrib_BR : 0; // red
                Temp |= ( Value & 0x02 ) ? Attrib_BG : 0; // green
                Temp |= ( Value & 0x04 ) ? Attrib_BB : 0; // blue

                break;
            }       
            // remove reverse
            case 27: {
                Temp &= ~Attrib_Reverse;
                break;
            }
            default: break;
        };

        Tok = strtok( NULL, ";" );
    }

    AttribMask = Temp;
}

void TTermbase::Escape_CursorPosition( void ) {
    char* Tok = NULL;
    int NewX = 0;
    int NewY = 0;

    // check for the seperator character
    Tok = strstr( EscapeString, ";" );

    // apparently this escape sequence must have a semicolon
    if ( Tok ) {
        // if it exists, set it to a null terminator
        // this way EscapeString will contain the first parameter
        // and Tok will point to the second parameter
        *Tok++ = 0;

        NewY = strtol( EscapeString, NULL, 10 );
        NewX = strtol( Tok, NULL, 10 );

        // if the parameter is omitted (or there is a parse error) strtol should
        // return LONG_MIN or LONG_MAX.
        // the expected behaviour of an omitted position is to set it to 1
        NewY = ( NewY == LONG_MIN || NewY == LONG_MAX ) ? 1 : NewY;
        NewX = ( NewX == LONG_MIN || NewY == LONG_MAX ) ? 1 : NewX;

        // coordinates are based on 1,1 being the top left
        NewY = ( NewY < 1 ) ? 1 : NewY;
        NewY = ( NewY > TermHeight ) ? TermHeight - 1 : NewY - 1;

        NewX = ( NewX < 0 ) ? 1 : NewX;
        NewX = ( NewX > TermWidth ) ? TermWidth - 1 : NewX - 1;

        x = NewX;
        y = NewY;
    }
}
