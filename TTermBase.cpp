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

void TTermbase::Begin( int DisplayWidth, int DisplayHeight, int FontWidth, int FontHeight ) {
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

    CursorBlinkState = true;
    NextCursorBlink = 0;
    NextSlowBlink = 0;
    NextFastBlink = 0;

    LastAttribAtCursorPos = DefaultAttrib;
    LastCharAtCursorPos = 'X';

    LastCursorX = 0;
    LastCursorY = 0;

    CursorX = 0;
    CursorY = 0;

    Screen = new char[ TermLength ];
    Attrib = new uint16_t[ TermLength ];

    memset( Screen, 0, TermLength );
    memset( Attrib, 0, TermLength * 2 );
    MarkAsDirty( 0, TermLength );
}

void TTermbase::Clear( int Offset, int Length ) {
}

size_t TTermbase::write( uint8_t Data ) {
    int Offset = GetCursorOffset( CursorX, CursorY );

    // Don't let the cursor go outside the screen
    CursorX = ( CursorX < 0 ) ? 0 : CursorX;
    CursorY = ( CursorY < 0 ) ? 0 : CursorY;

    if ( IsInEscape == false ) {
        switch ( Data ) {
            case '\x1b': {
                IsInEscape = true;
                return 1;
            }
            case '\r': {
                MoveCursor( 0, CursorY );
                break;
            }
            case '\n': {
                MoveCursor( 0, CursorY + 1 );
                break;
            }
            default: {
                Screen[ Offset ] = Data;
                Attrib[ Offset ] = AttribMask;

                MoveCursor( CursorX + 1, CursorY );
                break;
            }
        };
    } else {
        switch ( Data ) {
            case '[': break;
            case 'A' ... 'Z':
            case 'a' ... 'z': {
                EscapeString[ EscapeLength ] = 0;
                EscapeLength = 0;
                IsInEscape = false;

                HandleEscapeSequence( Data );
                break;
            }
            default: {
                if ( AddCharacterToEscapeString( Data ) == false ) {
                    IsInEscape = false;
                    EscapeLength = 0;
                }

                break;
            }
        };
    }

    return 1;
}

void TTermbase::Escape_MoveCursor( char Mode ) {
    int Amount = 0;
    int NewX = 0;
    int NewY = 0;
    int dx = 0;
    int dy = 0;

    Amount = ( int ) strtol( EscapeString, NULL, 10 );
    Amount = ( Amount == LONG_MIN || Amount == LONG_MAX || Amount == 0 ) ? 1 : Amount;

    switch ( Mode ) {
        case 'A':
        case 'a': {
            // Move cursor up n lines
            dy = -1 * Amount;
            dx = 0;
            break;
        }
        case 'B':
        case 'b': {
            // move cursor down n lines
            dy = 1 * Amount;
            dx = 0;
            break;
        }
        case 'C':
        case 'c': {
            // move cursor forward n lines
            dx = 1 * Amount;
            dy = 0;
            break;
        }
        case 'D':
        case 'd': {
            // move cursor backward n lines
            dx = -1 * Amount;
            dy = 0;
            break;
        }
        default: return;
    }

    NewX = CursorX + dx;
    NewY = CursorY + dy;

    NewX = ( NewX < 0 ) ? 0 : NewX;
    NewY = ( NewY < 0 ) ? 0 : NewY;

    NewX = ( NewX >= TermWidth ) ? TermWidth - 1 : NewX;
    NewY = ( NewY >= TermHeight ) ? TermHeight - 1 : NewY;

    MoveCursor( NewX, NewY );
}

void TTermbase::HandleEscapeSequence( char Data ) {
    switch ( Data ) {
        case 'A' ... 'D':
        case 'a' ... 'd': {
            Escape_MoveCursor( Data );
            break;
        }
        default: {
            Serial.print( "Unhandled escape code: " );
            Serial.println( Data );
            
            break;
        }
    };
}

void TTermbase::Update( void ) {
    uint32_t Now = millis( );
    int Offset = 0;
    int Row = 0;
    int Col = 0;

    for ( Row = 0; Row < TermHeight; Row++ ) {
        for ( Col = 0; Col < TermWidth; Col++ ) {
            Offset = GetCursorOffset( Col, Row );

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

    BlinkCursor( );
}

void TTermbase::BlinkCursor( void ) {
    uint32_t Now = millis( );
    int Offset = 0;

    if ( Now >= NextCursorBlink ) {
        NextCursorBlink = Now + 250;

        if ( CursorX != LastCursorX || CursorY != LastCursorY ) {
            Offset = GetCursorOffset( LastCursorX, LastCursorY );

            DrawGlyph( LastCursorX * _FontWidth, LastCursorY * _FontHeight, Screen[ Offset ], Attrib[ Offset ] );

            LastCursorX = CursorX;
            LastCursorY = CursorY;
        }

        DrawGlyph( CursorX * _FontWidth, CursorY * _FontHeight, '_', CursorBlinkState == true ? DefaultAttrib : 0 );
        CursorBlinkState = ! CursorBlinkState;
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

        // if the parameter is broken or missing, default to 1
        NewY = ( NewY == LONG_MIN || NewY == LONG_MAX || NewY == 0 ) ? 1 : NewY;
        NewX = ( NewX == LONG_MIN || NewX == LONG_MAX || NewX == 0 ) ? 1 : NewX;

        // coordinates are based on 1,1 being the top left
        NewY = ( NewY < 1 ) ? 1 : NewY;
        NewY = ( NewY > TermHeight ) ? TermHeight - 1 : NewY - 1;

        NewX = ( NewX < 0 ) ? 1 : NewX;
        NewX = ( NewX > TermWidth ) ? TermWidth - 1 : NewX - 1;

        LastCursorX = CursorX;
        LastCursorY = CursorY;

        CursorX = NewX;
        CursorY = NewY;
    }
}

void TTermbase::Escape_EraseInLine( void ) {
    int Offset = GetCursorOffset( CursorX, CursorY );
    int Length = 0;
    int Mode = 0;

    Mode = strtol( EscapeString, NULL, 10 );

    // Default to mode 0 (cursor to end of the line) if there was
    // no argument or it failed to parse
    Mode = ( Mode == LONG_MIN || Mode == LONG_MAX ) ? 0 : Mode;

    switch ( Mode ) {
        case 1: {
            break;
        }
        case 2: {
            break;
        }
        case 0: 
        default: {
            break;
        }
    };
}

int TTermbase::GetCursorOffset( int x, int y ) {
    x = ( x < 0 ) ? 0 : x;
    y = ( y < 0 ) ? 0 : y;

    x = ( x >= TermWidth ) ? TermWidth - 1 : x;
    y = ( y >= TermHeight ) ? TermHeight - 1 : y;

    return x + ( y * TermWidth );
}

bool TTermbase::AddCharacterToEscapeString( char c ) {
    if ( EscapeLength < ( sizeof( EscapeString ) - 1 ) ) {
        EscapeString[ EscapeLength++ ] = c;
        EscapeString[ EscapeLength ] = 0;

        return true;
    }

    return false;
}

void TTermbase::MoveCursor( int NewX, int NewY ) {
    NewX = ( NewX < 0 ) ? 0 : NewX;
    NewY = ( NewY < 0 ) ? 0 : NewY;

    if ( NewX >= TermWidth ) {
        NewX = 0;
        NewY++;
    }

    if ( NewY >= TermHeight ) {
        NewY = TermHeight - 1;
        NewX = 0;

        ScrollUp( );
    }

    CursorX = NewX;
    CursorY = NewY;
}
