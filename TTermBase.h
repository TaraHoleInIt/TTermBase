#ifndef _TTERM_BASE_H_
#define _TTERM_BASE_H_

#include <Arduino.h>
#include <stdint.h>

#define Attrib_Dirty bit( 15 )
#define Attrib_Hidden bit( 14 )
#define Attrib_Reverse bit( 13 )
// free bit at pos 12
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
private:
    char EscapeString[ 64 ];
    int EscapeLength;
    bool IsInEscape;

    int LastCursorX;
    int LastCursorY;

    uint16_t LastAttribAtCursorPos;
    char LastCharAtCursorPos;

    uint16_t DefaultAttrib;
    uint16_t AttribMask;

    void MoveCursor( int NewX, int NewY );

    /**
     * @brief Used to build the ANSI escape string from individual characters.
     * 
     * @param c Character to add
     * @return true If character was added to the escape string
     * @return false If there is no more room in the escape string
     */
    bool AddCharacterToEscapeString( char c );

    /**
     * @brief Sets the dirty attribute for all characters between Offset and Offset + Length
     * 
     * @param Offset Offset into the screen to start
     * @param Length Number of characters to mark as dirty
     */
    void MarkAsDirty( int Offset, int Length );

    /**
     * @brief Clears the screen from Offset to Offset + Length
     * 
     * @param Offset Offset into screen to start clearing
     * @param Length Number of characters to clear
     */
    void Clear( int Offset, int Length );

    /**
     * @brief Gets a screen offset from the given x and y coordinates
     * 
     * @param x X Coordinate
     * @param y Y Coordinate
     * @return Offset into screen or attribute memory
     */
    int GetCursorOffset( int x, int y );

    /**
     * @brief Escape sequence to erase withing a line/row
     * 
     */
    virtual void Escape_EraseInLine( void );

    /**
     * @brief Escape sequence to set the cursor position
     * 
     */
    virtual void Escape_CursorPosition( void );

    /**
     * @brief Escape sequence to clear the screen
     * 
     */
    virtual void Escape_Clear( void );

    /**
     * @brief Escape sequence to set special text attributes
     * 
     */
    virtual void Escape_SGR( void );

    /**
     * @brief Escape sequence to move the cursor
     * 
     * @param Mode Direction to move the cursor: a /\, b \/, c >, or d <
     */
    virtual void Escape_MoveCursor( char Mode );

    virtual void HandleEscapeSequence( char Data );
protected:
    char* Screen;
    uint16_t* Attrib;

    int CursorX;
    int CursorY;

    int TermLength;
    int TermWidth;
    int TermHeight;

    int _FontWidth;
    int _FontHeight;

    bool CursorBlinkState;
    uint32_t NextCursorBlink;
    uint32_t NextSlowBlink;
    uint32_t NextFastBlink;

    virtual void BlinkCursor( void );

    /**
     * @brief Display drivers must implement this to draw characters on the screen
     * 
     * @param x X Screen coordinate to draw the character
     * @param y Y Screen coordinate to draw the character
     * @param Character Character to draw
     * @param Attrib Character attributes
     */
    virtual void DrawGlyph( int x, int y, char Character, uint16_t Attrib ) = 0;
    void ScrollUp( void );
public:
    virtual void Begin( int DisplayWidth, int DisplayHeight, int FontWidth, int FontHeight );

    virtual size_t write( uint8_t Data );
    void Update( void );
};

#endif
