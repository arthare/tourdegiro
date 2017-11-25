/*******************************************************************************
 * THE FOLLOWING EXAMPLE CODE IS INTENDED FOR LIMITED CIRCULATION ONLY.
 * 
 * Please forward all questions regarding this code to ANT Technical Support.
 * 
 * Dynastream Innovations Inc.
 * 
 * (P) +1 403 932 9292
 * (F) +1 403 932 4196
 * (E) support@thisisant.com
 * 
 * www.thisisant.com
 *
 * Reference Design Disclaimer
 *
 * The references designs and codes provided may be used with ANT devices only and remain the copyrighted property of 
 * Dynastream Innovations Inc. The reference designs and codes are being provided on an "as-is" basis and as an accommodation, 
 * and therefore all warranties, representations, or guarantees of any kind (whether express, implied or statutory) including, 
 * without limitation, warranties of merchantability, non-infringement,
 * or fitness for a particular purpose, are specifically disclaimed.
 *
 * ©2012 Dynastream Innovations Inc. All Rights Reserved
 * This software may not be reproduced by
 * any means without express written approval of Dynastream
 * Innovations Inc.
 *
 *******************************************************************************/



#ifndef TYPES_H
#define TYPES_H


//////////////////////////////////////////////////////////////////////////////////
// Public Definitions
//////////////////////////////////////////////////////////////////////////////////

#define LITTLE_ENDIAN

#define TRUE                           1
#define FALSE                          0

#if !defined(NULL)
   #define NULL                        ((void *) 0)
#endif

#define MAX_UCHAR                      0xFF
#define MAX_SCHAR                      0x7F
#define MIN_SCHAR                      0x80

#define MAX_SHORT                      0x7FFF
#define MIN_SHORT                      0x8000
#define MAX_USHORT                     0xFFFF
#define MAX_SSHORT                     0x7FFF
#define MIN_SSHORT                     0x8000

#define MAX_LONG                       0x7FFFFFFF
#define MIN_LONG                       0x80000000
#define MAX_ULONG                      0xFFFFFFFF
#define MAX_SLONG                      0x7FFFFFFF
#define MIN_SLONG                      0x80000000

#if !defined(BASETYPES)                                     // windef.h compatibility
   typedef int               BOOL;
#endif

typedef unsigned char                  UCHAR;
typedef signed char                    SCHAR;

typedef short                          SHORT;
typedef unsigned short                 USHORT;
typedef signed short                   SSHORT;

#if !defined(LONG)
   typedef long                        LONG;
#endif
typedef unsigned long                  ULONG;
typedef signed long                    SLONG;

typedef float                          FLOAT;
typedef double                         DOUBLE;

typedef union
{
   USHORT usData;
   struct
   {
      #if defined(LITTLE_ENDIAN)
         UCHAR ucLow;
         UCHAR ucHigh;
      #elif defined(BIG_ENDIAN)
         UCHAR ucHigh;
         UCHAR ucLow;
      #else
         #error
      #endif
   } stBytes;
} USHORT_UNION;

typedef union
{
   ULONG ulData;
   struct
   {
      // The least significant byte of the ULONG in this structure is
      // referenced by ucByte0.
      #if defined(LITTLE_ENDIAN)
         UCHAR ucByte0;
         UCHAR ucByte1;
         UCHAR ucByte2;
         UCHAR ucByte3;
      #elif defined(BIG_ENDIAN)
         UCHAR ucByte3;
         UCHAR ucByte2;
         UCHAR ucByte1;
         UCHAR ucByte0;
      #else
         #error
      #endif
   } stBytes;
} ULONG_UNION;
#endif // !TYPES_H

