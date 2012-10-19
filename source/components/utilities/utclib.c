/******************************************************************************
 *
 * Module Name: cmclib - Local implementation of C library functions
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2012, Intel Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */


#define __CMCLIB_C__

#include "acpi.h"
#include "accommon.h"

/*
 * These implementations of standard C Library routines can optionally be
 * used if a C library is not available. In general, they are less efficient
 * than an inline or assembly implementation
 */

#define _COMPONENT          ACPI_UTILITIES
        ACPI_MODULE_NAME    ("cmclib")


#ifndef ACPI_USE_SYSTEM_CLIBRARY

#define NEGATIVE    1
#define POSITIVE    0


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtMemcmp (memcmp)
 *
 * PARAMETERS:  Buffer1         - First Buffer
 *              Buffer2         - Second Buffer
 *              Count           - Maximum # of bytes to compare
 *
 * RETURN:      Index where Buffers mismatched, or 0 if Buffers matched
 *
 * DESCRIPTION: Compare two Buffers, with a maximum length
 *
 ******************************************************************************/

int
AcpiUtMemcmp (
    const char              *Buffer1,
    const char              *Buffer2,
    ACPI_SIZE               Count)
{

    for ( ; Count-- && (*Buffer1 == *Buffer2); Buffer1++, Buffer2++)
    {
    }

    return ((Count == ACPI_SIZE_MAX) ? 0 : ((unsigned char) *Buffer1 -
        (unsigned char) *Buffer2));
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtMemcpy (memcpy)
 *
 * PARAMETERS:  Dest        - Target of the copy
 *              Src         - Source buffer to copy
 *              Count       - Number of bytes to copy
 *
 * RETURN:      Dest
 *
 * DESCRIPTION: Copy arbitrary bytes of memory
 *
 ******************************************************************************/

void *
AcpiUtMemcpy (
    void                    *Dest,
    const void              *Src,
    ACPI_SIZE               Count)
{
    char                    *New = (char *) Dest;
    char                    *Old = (char *) Src;


    while (Count)
    {
        *New = *Old;
        New++;
        Old++;
        Count--;
    }

    return (Dest);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtMemset (memset)
 *
 * PARAMETERS:  Dest        - Buffer to set
 *              Value       - Value to set each byte of memory
 *              Count       - Number of bytes to set
 *
 * RETURN:      Dest
 *
 * DESCRIPTION: Initialize a buffer to a known value.
 *
 ******************************************************************************/

void *
AcpiUtMemset (
    void                    *Dest,
    UINT8                   Value,
    ACPI_SIZE               Count)
{
    char                    *New = (char *) Dest;


    while (Count)
    {
        *New = (char) Value;
        New++;
        Count--;
    }

    return (Dest);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtStrlen (strlen)
 *
 * PARAMETERS:  String              - Null terminated string
 *
 * RETURN:      Length
 *
 * DESCRIPTION: Returns the length of the input string
 *
 ******************************************************************************/


ACPI_SIZE
AcpiUtStrlen (
    const char              *String)
{
    UINT32                  Length = 0;


    /* Count the string until a null is encountered */

    while (*String)
    {
        Length++;
        String++;
    }

    return (Length);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtStrcpy (strcpy)
 *
 * PARAMETERS:  DstString       - Target of the copy
 *              SrcString       - The source string to copy
 *
 * RETURN:      DstString
 *
 * DESCRIPTION: Copy a null terminated string
 *
 ******************************************************************************/

char *
AcpiUtStrcpy (
    char                    *DstString,
    const char              *SrcString)
{
    char                    *String = DstString;


    /* Move bytes brute force */

    while (*SrcString)
    {
        *String = *SrcString;

        String++;
        SrcString++;
    }

    /* Null terminate */

    *String = 0;
    return (DstString);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtStrncpy (strncpy)
 *
 * PARAMETERS:  DstString       - Target of the copy
 *              SrcString       - The source string to copy
 *              Count           - Maximum # of bytes to copy
 *
 * RETURN:      DstString
 *
 * DESCRIPTION: Copy a null terminated string, with a maximum length
 *
 ******************************************************************************/

char *
AcpiUtStrncpy (
    char                    *DstString,
    const char              *SrcString,
    ACPI_SIZE               Count)
{
    char                    *String = DstString;


    /* Copy the string */

    for (String = DstString;
        Count && (Count--, (*String++ = *SrcString++)); )
    {;}

    /* Pad with nulls if necessary */

    while (Count--)
    {
        *String = 0;
        String++;
    }

    /* Return original pointer */

    return (DstString);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtStrcmp (strcmp)
 *
 * PARAMETERS:  String1         - First string
 *              String2         - Second string
 *
 * RETURN:      Index where strings mismatched, or 0 if strings matched
 *
 * DESCRIPTION: Compare two null terminated strings
 *
 ******************************************************************************/

int
AcpiUtStrcmp (
    const char              *String1,
    const char              *String2)
{


    for ( ; (*String1 == *String2); String2++)
    {
        if (!*String1++)
        {
            return (0);
        }
    }

    return ((unsigned char) *String1 - (unsigned char) *String2);
}


#ifdef ACPI_FUTURE_IMPLEMENTATION
/* Not used at this time */
/*******************************************************************************
 *
 * FUNCTION:    AcpiUtStrchr (strchr)
 *
 * PARAMETERS:  String          - Search string
 *              ch              - character to search for
 *
 * RETURN:      Ptr to char or NULL if not found
 *
 * DESCRIPTION: Search a string for a character
 *
 ******************************************************************************/

char *
AcpiUtStrchr (
    const char              *String,
    int                     ch)
{


    for ( ; (*String); String++)
    {
        if ((*String) == (char) ch)
        {
            return ((char *) String);
        }
    }

    return (NULL);
}
#endif

/*******************************************************************************
 *
 * FUNCTION:    AcpiUtStrncmp (strncmp)
 *
 * PARAMETERS:  String1         - First string
 *              String2         - Second string
 *              Count           - Maximum # of bytes to compare
 *
 * RETURN:      Index where strings mismatched, or 0 if strings matched
 *
 * DESCRIPTION: Compare two null terminated strings, with a maximum length
 *
 ******************************************************************************/

int
AcpiUtStrncmp (
    const char              *String1,
    const char              *String2,
    ACPI_SIZE               Count)
{


    for ( ; Count-- && (*String1 == *String2); String2++)
    {
        if (!*String1++)
        {
            return (0);
        }
    }

    return ((Count == ACPI_SIZE_MAX) ? 0 : ((unsigned char) *String1 -
        (unsigned char) *String2));
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtStrcat (Strcat)
 *
 * PARAMETERS:  DstString       - Target of the copy
 *              SrcString       - The source string to copy
 *
 * RETURN:      DstString
 *
 * DESCRIPTION: Append a null terminated string to a null terminated string
 *
 ******************************************************************************/

char *
AcpiUtStrcat (
    char                    *DstString,
    const char              *SrcString)
{
    char                    *String;


    /* Find end of the destination string */

    for (String = DstString; *String++; )
    { ; }

    /* Concatenate the string */

    for (--String; (*String++ = *SrcString++); )
    { ; }

    return (DstString);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtStrncat (strncat)
 *
 * PARAMETERS:  DstString       - Target of the copy
 *              SrcString       - The source string to copy
 *              Count           - Maximum # of bytes to copy
 *
 * RETURN:      DstString
 *
 * DESCRIPTION: Append a null terminated string to a null terminated string,
 *              with a maximum count.
 *
 ******************************************************************************/

char *
AcpiUtStrncat (
    char                    *DstString,
    const char              *SrcString,
    ACPI_SIZE               Count)
{
    char                    *String;


    if (Count)
    {
        /* Find end of the destination string */

        for (String = DstString; *String++; )
        { ; }

        /* Concatenate the string */

        for (--String; (*String++ = *SrcString++) && --Count; )
        { ; }

        /* Null terminate if necessary */

        if (!Count)
        {
            *String = 0;
        }
    }

    return (DstString);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtStrstr (strstr)
 *
 * PARAMETERS:  String1         - Target string
 *              String2         - Substring to search for
 *
 * RETURN:      Where substring match starts, Null if no match found
 *
 * DESCRIPTION: Checks if String2 occurs in String1. This is not really a
 *              full implementation of strstr, only sufficient for command
 *              matching
 *
 ******************************************************************************/

char *
AcpiUtStrstr (
    char                    *String1,
    char                    *String2)
{
    char                    *String;


    if (AcpiUtStrlen (String2) > AcpiUtStrlen (String1))
    {
        return (NULL);
    }

    /* Walk entire string, comparing the letters */

    for (String = String1; *String2; )
    {
        if (*String2 != *String)
        {
            return (NULL);
        }

        String2++;
        String++;
    }

    return (String1);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtStrtoul (strtoul)
 *
 * PARAMETERS:  String          - Null terminated string
 *              Terminater      - Where a pointer to the terminating byte is
 *                                returned
 *              Base            - Radix of the string
 *
 * RETURN:      Converted value
 *
 * DESCRIPTION: Convert a string into a 32-bit unsigned value.
 *              Note: use AcpiUtStrtoul64 for 64-bit integers.
 *
 ******************************************************************************/

UINT32
AcpiUtStrtoul (
    const char              *String,
    char                    **Terminator,
    UINT32                  Base)
{
    UINT32                  converted = 0;
    UINT32                  index;
    UINT32                  sign;
    const char              *StringStart;
    UINT32                  ReturnValue = 0;
    ACPI_STATUS             Status = AE_OK;


    /*
     * Save the value of the pointer to the buffer's first
     * character, save the current errno value, and then
     * skip over any white space in the buffer:
     */
    StringStart = String;
    while (ACPI_IS_SPACE (*String) || *String == '\t')
    {
        ++String;
    }

    /*
     * The buffer may contain an optional plus or minus sign.
     * If it does, then skip over it but remember what is was:
     */
    if (*String == '-')
    {
        sign = NEGATIVE;
        ++String;
    }
    else if (*String == '+')
    {
        ++String;
        sign = POSITIVE;
    }
    else
    {
        sign = POSITIVE;
    }

    /*
     * If the input parameter Base is zero, then we need to
     * determine if it is octal, decimal, or hexadecimal:
     */
    if (Base == 0)
    {
        if (*String == '0')
        {
            if (AcpiUtToLower (*(++String)) == 'x')
            {
                Base = 16;
                ++String;
            }
            else
            {
                Base = 8;
            }
        }
        else
        {
            Base = 10;
        }
    }
    else if (Base < 2 || Base > 36)
    {
        /*
         * The specified Base parameter is not in the domain of
         * this function:
         */
        goto done;
    }

    /*
     * For octal and hexadecimal bases, skip over the leading
     * 0 or 0x, if they are present.
     */
    if (Base == 8 && *String == '0')
    {
        String++;
    }

    if (Base == 16 &&
        *String == '0' &&
        AcpiUtToLower (*(++String)) == 'x')
    {
        String++;
    }

    /*
     * Main loop: convert the string to an unsigned long:
     */
    while (*String)
    {
        if (ACPI_IS_DIGIT (*String))
        {
            index = (UINT32) ((UINT8) *String - '0');
        }
        else
        {
            index = (UINT32) AcpiUtToUpper (*String);
            if (ACPI_IS_UPPER (index))
            {
                index = index - 'A' + 10;
            }
            else
            {
                goto done;
            }
        }

        if (index >= Base)
        {
            goto done;
        }

        /*
         * Check to see if value is out of range:
         */

        if (ReturnValue > ((ACPI_UINT32_MAX - (UINT32) index) /
                            (UINT32) Base))
        {
            Status = AE_ERROR;
            ReturnValue = 0;           /* reset */
        }
        else
        {
            ReturnValue *= Base;
            ReturnValue += index;
            converted = 1;
        }

        ++String;
    }

done:
    /*
     * If appropriate, update the caller's pointer to the next
     * unconverted character in the buffer.
     */
    if (Terminator)
    {
        if (converted == 0 && ReturnValue == 0 && String != NULL)
        {
            *Terminator = (char *) StringStart;
        }
        else
        {
            *Terminator = (char *) String;
        }
    }

    if (Status == AE_ERROR)
    {
        ReturnValue = ACPI_UINT32_MAX;
    }

    /*
     * If a minus sign was present, then "the conversion is negated":
     */
    if (sign == NEGATIVE)
    {
        ReturnValue = (ACPI_UINT32_MAX - ReturnValue) + 1;
    }

    return (ReturnValue);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtToUpper (TOUPPER)
 *
 * PARAMETERS:  c           - Character to convert
 *
 * RETURN:      Converted character as an int
 *
 * DESCRIPTION: Convert character to uppercase
 *
 ******************************************************************************/

int
AcpiUtToUpper (
    int                     c)
{

    return (ACPI_IS_LOWER(c) ? ((c)-0x20) : (c));
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtToLower (TOLOWER)
 *
 * PARAMETERS:  c           - Character to convert
 *
 * RETURN:      Converted character as an int
 *
 * DESCRIPTION: Convert character to lowercase
 *
 ******************************************************************************/

int
AcpiUtToLower (
    int                     c)
{

    return (ACPI_IS_UPPER(c) ? ((c)+0x20) : (c));
}


/*******************************************************************************
 *
 * FUNCTION:    is* functions
 *
 * DESCRIPTION: is* functions use the ctype table below
 *
 ******************************************************************************/

const UINT8 _acpi_ctype[257] = {
    _ACPI_CN,            /* 0x00     0 NUL */
    _ACPI_CN,            /* 0x01     1 SOH */
    _ACPI_CN,            /* 0x02     2 STX */
    _ACPI_CN,            /* 0x03     3 ETX */
    _ACPI_CN,            /* 0x04     4 EOT */
    _ACPI_CN,            /* 0x05     5 ENQ */
    _ACPI_CN,            /* 0x06     6 ACK */
    _ACPI_CN,            /* 0x07     7 BEL */
    _ACPI_CN,            /* 0x08     8 BS  */
    _ACPI_CN|_ACPI_SP,   /* 0x09     9 TAB */
    _ACPI_CN|_ACPI_SP,   /* 0x0A    10 LF  */
    _ACPI_CN|_ACPI_SP,   /* 0x0B    11 VT  */
    _ACPI_CN|_ACPI_SP,   /* 0x0C    12 FF  */
    _ACPI_CN|_ACPI_SP,   /* 0x0D    13 CR  */
    _ACPI_CN,            /* 0x0E    14 SO  */
    _ACPI_CN,            /* 0x0F    15 SI  */
    _ACPI_CN,            /* 0x10    16 DLE */
    _ACPI_CN,            /* 0x11    17 DC1 */
    _ACPI_CN,            /* 0x12    18 DC2 */
    _ACPI_CN,            /* 0x13    19 DC3 */
    _ACPI_CN,            /* 0x14    20 DC4 */
    _ACPI_CN,            /* 0x15    21 NAK */
    _ACPI_CN,            /* 0x16    22 SYN */
    _ACPI_CN,            /* 0x17    23 ETB */
    _ACPI_CN,            /* 0x18    24 CAN */
    _ACPI_CN,            /* 0x19    25 EM  */
    _ACPI_CN,            /* 0x1A    26 SUB */
    _ACPI_CN,            /* 0x1B    27 ESC */
    _ACPI_CN,            /* 0x1C    28 FS  */
    _ACPI_CN,            /* 0x1D    29 GS  */
    _ACPI_CN,            /* 0x1E    30 RS  */
    _ACPI_CN,            /* 0x1F    31 US  */
    _ACPI_XS|_ACPI_SP,   /* 0x20    32 ' ' */
    _ACPI_PU,            /* 0x21    33 '!' */
    _ACPI_PU,            /* 0x22    34 '"' */
    _ACPI_PU,            /* 0x23    35 '#' */
    _ACPI_PU,            /* 0x24    36 '$' */
    _ACPI_PU,            /* 0x25    37 '%' */
    _ACPI_PU,            /* 0x26    38 '&' */
    _ACPI_PU,            /* 0x27    39 ''' */
    _ACPI_PU,            /* 0x28    40 '(' */
    _ACPI_PU,            /* 0x29    41 ')' */
    _ACPI_PU,            /* 0x2A    42 '*' */
    _ACPI_PU,            /* 0x2B    43 '+' */
    _ACPI_PU,            /* 0x2C    44 ',' */
    _ACPI_PU,            /* 0x2D    45 '-' */
    _ACPI_PU,            /* 0x2E    46 '.' */
    _ACPI_PU,            /* 0x2F    47 '/' */
    _ACPI_XD|_ACPI_DI,   /* 0x30    48 '0' */
    _ACPI_XD|_ACPI_DI,   /* 0x31    49 '1' */
    _ACPI_XD|_ACPI_DI,   /* 0x32    50 '2' */
    _ACPI_XD|_ACPI_DI,   /* 0x33    51 '3' */
    _ACPI_XD|_ACPI_DI,   /* 0x34    52 '4' */
    _ACPI_XD|_ACPI_DI,   /* 0x35    53 '5' */
    _ACPI_XD|_ACPI_DI,   /* 0x36    54 '6' */
    _ACPI_XD|_ACPI_DI,   /* 0x37    55 '7' */
    _ACPI_XD|_ACPI_DI,   /* 0x38    56 '8' */
    _ACPI_XD|_ACPI_DI,   /* 0x39    57 '9' */
    _ACPI_PU,            /* 0x3A    58 ':' */
    _ACPI_PU,            /* 0x3B    59 ';' */
    _ACPI_PU,            /* 0x3C    60 '<' */
    _ACPI_PU,            /* 0x3D    61 '=' */
    _ACPI_PU,            /* 0x3E    62 '>' */
    _ACPI_PU,            /* 0x3F    63 '?' */
    _ACPI_PU,            /* 0x40    64 '@' */
    _ACPI_XD|_ACPI_UP,   /* 0x41    65 'A' */
    _ACPI_XD|_ACPI_UP,   /* 0x42    66 'B' */
    _ACPI_XD|_ACPI_UP,   /* 0x43    67 'C' */
    _ACPI_XD|_ACPI_UP,   /* 0x44    68 'D' */
    _ACPI_XD|_ACPI_UP,   /* 0x45    69 'E' */
    _ACPI_XD|_ACPI_UP,   /* 0x46    70 'F' */
    _ACPI_UP,            /* 0x47    71 'G' */
    _ACPI_UP,            /* 0x48    72 'H' */
    _ACPI_UP,            /* 0x49    73 'I' */
    _ACPI_UP,            /* 0x4A    74 'J' */
    _ACPI_UP,            /* 0x4B    75 'K' */
    _ACPI_UP,            /* 0x4C    76 'L' */
    _ACPI_UP,            /* 0x4D    77 'M' */
    _ACPI_UP,            /* 0x4E    78 'N' */
    _ACPI_UP,            /* 0x4F    79 'O' */
    _ACPI_UP,            /* 0x50    80 'P' */
    _ACPI_UP,            /* 0x51    81 'Q' */
    _ACPI_UP,            /* 0x52    82 'R' */
    _ACPI_UP,            /* 0x53    83 'S' */
    _ACPI_UP,            /* 0x54    84 'T' */
    _ACPI_UP,            /* 0x55    85 'U' */
    _ACPI_UP,            /* 0x56    86 'V' */
    _ACPI_UP,            /* 0x57    87 'W' */
    _ACPI_UP,            /* 0x58    88 'X' */
    _ACPI_UP,            /* 0x59    89 'Y' */
    _ACPI_UP,            /* 0x5A    90 'Z' */
    _ACPI_PU,            /* 0x5B    91 '[' */
    _ACPI_PU,            /* 0x5C    92 '\' */
    _ACPI_PU,            /* 0x5D    93 ']' */
    _ACPI_PU,            /* 0x5E    94 '^' */
    _ACPI_PU,            /* 0x5F    95 '_' */
    _ACPI_PU,            /* 0x60    96 '`' */
    _ACPI_XD|_ACPI_LO,   /* 0x61    97 'a' */
    _ACPI_XD|_ACPI_LO,   /* 0x62    98 'b' */
    _ACPI_XD|_ACPI_LO,   /* 0x63    99 'c' */
    _ACPI_XD|_ACPI_LO,   /* 0x64   100 'd' */
    _ACPI_XD|_ACPI_LO,   /* 0x65   101 'e' */
    _ACPI_XD|_ACPI_LO,   /* 0x66   102 'f' */
    _ACPI_LO,            /* 0x67   103 'g' */
    _ACPI_LO,            /* 0x68   104 'h' */
    _ACPI_LO,            /* 0x69   105 'i' */
    _ACPI_LO,            /* 0x6A   106 'j' */
    _ACPI_LO,            /* 0x6B   107 'k' */
    _ACPI_LO,            /* 0x6C   108 'l' */
    _ACPI_LO,            /* 0x6D   109 'm' */
    _ACPI_LO,            /* 0x6E   110 'n' */
    _ACPI_LO,            /* 0x6F   111 'o' */
    _ACPI_LO,            /* 0x70   112 'p' */
    _ACPI_LO,            /* 0x71   113 'q' */
    _ACPI_LO,            /* 0x72   114 'r' */
    _ACPI_LO,            /* 0x73   115 's' */
    _ACPI_LO,            /* 0x74   116 't' */
    _ACPI_LO,            /* 0x75   117 'u' */
    _ACPI_LO,            /* 0x76   118 'v' */
    _ACPI_LO,            /* 0x77   119 'w' */
    _ACPI_LO,            /* 0x78   120 'x' */
    _ACPI_LO,            /* 0x79   121 'y' */
    _ACPI_LO,            /* 0x7A   122 'z' */
    _ACPI_PU,            /* 0x7B   123 '{' */
    _ACPI_PU,            /* 0x7C   124 '|' */
    _ACPI_PU,            /* 0x7D   125 '}' */
    _ACPI_PU,            /* 0x7E   126 '~' */
    _ACPI_CN,            /* 0x7F   127 DEL */

    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 0x80 to 0x8F    */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 0x90 to 0x9F    */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 0xA0 to 0xAF    */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 0xB0 to 0xBF    */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 0xC0 to 0xCF    */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 0xD0 to 0xDF    */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 0xE0 to 0xEF    */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 0xF0 to 0xFF    */
    0                                 /* 0x100 */
};


#endif /* ACPI_USE_SYSTEM_CLIBRARY */
