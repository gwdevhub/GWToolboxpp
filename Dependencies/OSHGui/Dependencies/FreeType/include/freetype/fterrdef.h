/***************************************************************************/
/*                                                                         */
/*  fterrdef.h                                                             */
/*                                                                         */
/*    FreeType error codes (specification).                                */
/*                                                                         */
/*  Copyright 2002, 2004, 2006, 2007, 2010-2013 by                         */
/*  David Turner, Robert Wilhelm, and Werner Lemberg.                      */
/*                                                                         */
/*  This file is part of the FreeType project, and may only be used,       */
/*  modified, and distributed under the terms of the FreeType project      */
/*  license, LICENSE.TXT.  By continuing to use, modify, or distribute     */
/*  this file you indicate that you have read the license and              */
/*  understand and accept it fully.                                        */
/*                                                                         */
/***************************************************************************/


  /*******************************************************************/
  /*******************************************************************/
  /*****                                                         *****/
  /*****                LIST OF ERROR CODES/MESSAGES             *****/
  /*****                                                         *****/
  /*******************************************************************/
  /*******************************************************************/


  /* You need to define both FT_ERRORDEF_ and FT_NOERRORDEF_ before */
  /* including this file.                                           */


  /* generic errors */

  FT_NOERRORDEF_( Ok,                                        0x00, \
                  "" )

  FT_ERRORDEF_( Cannot_Open_Resource,                        0x01, \
                "" )
  FT_ERRORDEF_( Unknown_File_Format,                         0x02, \
                "" )
  FT_ERRORDEF_( Invalid_File_Format,                         0x03, \
                "" )
  FT_ERRORDEF_( Invalid_Version,                             0x04, \
                "" )
  FT_ERRORDEF_( Lower_Module_Version,                        0x05, \
                "" )
  FT_ERRORDEF_( Invalid_Argument,                            0x06, \
                "" )
  FT_ERRORDEF_( Unimplemented_Feature,                       0x07, \
                "" )
  FT_ERRORDEF_( Invalid_Table,                               0x08, \
                "" )
  FT_ERRORDEF_( Invalid_Offset,                              0x09, \
                "" )
  FT_ERRORDEF_( Array_Too_Large,                             0x0A, \
                "" )
  FT_ERRORDEF_( Missing_Module,                              0x0B, \
                "" )
  FT_ERRORDEF_( Missing_Property,                            0x0C, \
                "" )

  /* glyph/character errors */

  FT_ERRORDEF_( Invalid_Glyph_Index,                         0x10, \
                "" )
  FT_ERRORDEF_( Invalid_Character_Code,                      0x11, \
                "" )
  FT_ERRORDEF_( Invalid_Glyph_Format,                        0x12, \
                "" )
  FT_ERRORDEF_( Cannot_Render_Glyph,                         0x13, \
                "" )
  FT_ERRORDEF_( Invalid_Outline,                             0x14, \
                "" )
  FT_ERRORDEF_( Invalid_Composite,                           0x15, \
                "" )
  FT_ERRORDEF_( Too_Many_Hints,                              0x16, \
                "" )
  FT_ERRORDEF_( Invalid_Pixel_Size,                          0x17, \
                "" )

  /* handle errors */

  FT_ERRORDEF_( Invalid_Handle,                              0x20, \
                "" )
  FT_ERRORDEF_( Invalid_Library_Handle,                      0x21, \
                "" )
  FT_ERRORDEF_( Invalid_Driver_Handle,                       0x22, \
                "" )
  FT_ERRORDEF_( Invalid_Face_Handle,                         0x23, \
                "" )
  FT_ERRORDEF_( Invalid_Size_Handle,                         0x24, \
                "" )
  FT_ERRORDEF_( Invalid_Slot_Handle,                         0x25, \
                "" )
  FT_ERRORDEF_( Invalid_CharMap_Handle,                      0x26, \
                "" )
  FT_ERRORDEF_( Invalid_Cache_Handle,                        0x27, \
                "" )
  FT_ERRORDEF_( Invalid_Stream_Handle,                       0x28, \
                "" )

  /* driver errors */

  FT_ERRORDEF_( Too_Many_Drivers,                            0x30, \
                "" )
  FT_ERRORDEF_( Too_Many_Extensions,                         0x31, \
                "" )

  /* memory errors */

  FT_ERRORDEF_( Out_Of_Memory,                               0x40, \
                "" )
  FT_ERRORDEF_( Unlisted_Object,                             0x41, \
                "" )

  /* stream errors */

  FT_ERRORDEF_( Cannot_Open_Stream,                          0x51, \
                "" )
  FT_ERRORDEF_( Invalid_Stream_Seek,                         0x52, \
                "" )
  FT_ERRORDEF_( Invalid_Stream_Skip,                         0x53, \
                "" )
  FT_ERRORDEF_( Invalid_Stream_Read,                         0x54, \
                "" )
  FT_ERRORDEF_( Invalid_Stream_Operation,                    0x55, \
                "" )
  FT_ERRORDEF_( Invalid_Frame_Operation,                     0x56, \
                "" )
  FT_ERRORDEF_( Nested_Frame_Access,                         0x57, \
                "" )
  FT_ERRORDEF_( Invalid_Frame_Read,                          0x58, \
                "" )

  /* raster errors */

  FT_ERRORDEF_( Raster_Uninitialized,                        0x60, \
                "" )
  FT_ERRORDEF_( Raster_Corrupted,                            0x61, \
                "" )
  FT_ERRORDEF_( Raster_Overflow,                             0x62, \
                "" )
  FT_ERRORDEF_( Raster_Negative_Height,                      0x63, \
                "" )

  /* cache errors */

  FT_ERRORDEF_( Too_Many_Caches,                             0x70, \
                "" )

  /* TrueType and SFNT errors */

  FT_ERRORDEF_( Invalid_Opcode,                              0x80, \
                "" )
  FT_ERRORDEF_( Too_Few_Arguments,                           0x81, \
                "" )
  FT_ERRORDEF_( Stack_Overflow,                              0x82, \
                "" )
  FT_ERRORDEF_( Code_Overflow,                               0x83, \
                "" )
  FT_ERRORDEF_( Bad_Argument,                                0x84, \
                "" )
  FT_ERRORDEF_( Divide_By_Zero,                              0x85, \
                "" )
  FT_ERRORDEF_( Invalid_Reference,                           0x86, \
                "" )
  FT_ERRORDEF_( Debug_OpCode,                                0x87, \
                "" )
  FT_ERRORDEF_( ENDF_In_Exec_Stream,                         0x88, \
                "" )
  FT_ERRORDEF_( Nested_DEFS,                                 0x89, \
                "" )
  FT_ERRORDEF_( Invalid_CodeRange,                           0x8A, \
                "" )
  FT_ERRORDEF_( Execution_Too_Long,                          0x8B, \
                "" )
  FT_ERRORDEF_( Too_Many_Function_Defs,                      0x8C, \
                "" )
  FT_ERRORDEF_( Too_Many_Instruction_Defs,                   0x8D, \
                "" )
  FT_ERRORDEF_( Table_Missing,                               0x8E, \
                "" )
  FT_ERRORDEF_( Horiz_Header_Missing,                        0x8F, \
                "" )
  FT_ERRORDEF_( Locations_Missing,                           0x90, \
                "" )
  FT_ERRORDEF_( Name_Table_Missing,                          0x91, \
                "" )
  FT_ERRORDEF_( CMap_Table_Missing,                          0x92, \
                "" )
  FT_ERRORDEF_( Hmtx_Table_Missing,                          0x93, \
                "" )
  FT_ERRORDEF_( Post_Table_Missing,                          0x94, \
                "" )
  FT_ERRORDEF_( Invalid_Horiz_Metrics,                       0x95, \
                "" )
  FT_ERRORDEF_( Invalid_CharMap_Format,                      0x96, \
                "" )
  FT_ERRORDEF_( Invalid_PPem,                                0x97, \
                "" )
  FT_ERRORDEF_( Invalid_Vert_Metrics,                        0x98, \
                "" )
  FT_ERRORDEF_( Could_Not_Find_Context,                      0x99, \
                "" )
  FT_ERRORDEF_( Invalid_Post_Table_Format,                   0x9A, \
                "" )
  FT_ERRORDEF_( Invalid_Post_Table,                          0x9B, \
                "" )

  /* CFF, CID, and Type 1 errors */

  FT_ERRORDEF_( Syntax_Error,                                0xA0, \
                "" )
  FT_ERRORDEF_( Stack_Underflow,                             0xA1, \
                "" )
  FT_ERRORDEF_( Ignore,                                      0xA2, \
                "" )
  FT_ERRORDEF_( No_Unicode_Glyph_Name,                       0xA3, \
                "" )
  FT_ERRORDEF_( Glyph_Too_Big,                               0xA4, \
                "" )

  /* BDF errors */

  FT_ERRORDEF_( Missing_Startfont_Field,                     0xB0, \
                "" )
  FT_ERRORDEF_( Missing_Font_Field,                          0xB1, \
                "" )
  FT_ERRORDEF_( Missing_Size_Field,                          0xB2, \
                "" )
  FT_ERRORDEF_( Missing_Fontboundingbox_Field,               0xB3, \
                "" )
  FT_ERRORDEF_( Missing_Chars_Field,                         0xB4, \
                "" )
  FT_ERRORDEF_( Missing_Startchar_Field,                     0xB5, \
                "" )
  FT_ERRORDEF_( Missing_Encoding_Field,                      0xB6, \
                "" )
  FT_ERRORDEF_( Missing_Bbx_Field,                           0xB7, \
                "" )
  FT_ERRORDEF_( Bbx_Too_Big,                                 0xB8, \
                "" )
  FT_ERRORDEF_( Corrupted_Font_Header,                       0xB9, \
                "" )
  FT_ERRORDEF_( Corrupted_Font_Glyphs,                       0xBA, \
                "" )


/* END */
