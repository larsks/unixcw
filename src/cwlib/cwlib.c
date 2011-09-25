/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
 * Copyright (C) 2011 Kamil Ignacak (acerion@wp.pl)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "../config.h"

#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

#if (defined(__unix__) || defined(unix)) && !defined(USG)
# include <sys/param.h>
#endif

#if defined(HAVE_STRING_H)
# include <string.h>
#endif

#if defined(HAVE_STRINGS_H)
# include <strings.h>
#endif

#if defined(HAVE_SYS_KD_H)
# include <sys/kd.h>
#elif defined(HAVE_SYS_VTKD_H)
# include <sys/vtkd.h>
#elif defined(HAVE_SYS_KBIO_H)
# include <sys/kbio.h>
#endif

#if defined(HAVE_SYS_SOUNDCARD_H)
# include <sys/soundcard.h>
#elif defined(HAVE_SOUNDCARD_H)
# include <soundcard.h>
#else
# error "Neither sys/soundcard.h nor soundcard.h header file available"
#endif

#if defined(BSD)
# define ERR_NO_SUPPORT EPROTONOSUPPORT
#else
# define ERR_NO_SUPPORT EPROTO
#endif

#include "cwlib.h"

/* for stand-alone compilation and tests */
/* #define CWLIB_MAIN */



static int   cw_oss_open_device(const char *device);
static int   cw_oss_close_device(void);
static void *cw_oss_generator_write_sinewave(void *arg);
static int   cw_oss_generator_calculate_amplitude(cw_gen_t *gen);

static int cw_console_open_device(const char *device);
static int cw_console_close_device(void);
static int cw_set_soundcard_device(const char *device);
static int cw_set_console_device(const char *device);






/* Fixed general soundcard parameters. */
static const int CW_OSS_SETFRAGMENT = 7;       /* Sound fragment size, 2^N */
static const int CW_OSS_FORMAT = AFMT_S16_NE;  /* Sound format AFMT_S16_NE = signed 16 bit, native endianess; LE = Little endianess */
static const int CW_OSS_CHANNELS = 1;          /* Sound in mono only */
static const int CW_SAMPLE_RATE = 44100;       /* Sound sampling rate */

static const int CW_OSS_GENERATOR_SLOPE = 100; /* 100 for 48000 Hz sample rate */
#define CW_OSS_GENERATOR_BUF_SIZE 128
#define CW_OSS_SET_FRAGMENT 1
#define CW_OSS_SET_POLICY   0



/* main data container; this is a library file, so in future
   the variable must be moved from the file to client code;
   this is a global variable that should be converted into
   a function argument; this pointer should exist only in
   client's code, should initially be returned by new(), and
   deleted by delete();
   TODO: perform the conversion later, when you figure out
   ins and outs of the library */
static cw_gen_t *generator = NULL;


/*---------------------------------------------------------------------*/
/*  Module variables, copyright, miscellaneous other stuff             */
/*---------------------------------------------------------------------*/

/* False, true, and library return codes. */
enum { FALSE = 0, TRUE = !FALSE };
enum { RC_SUCCESS = TRUE, RC_ERROR = FALSE };

static const char *const CW_COPYRIGHT =
	"Copyright (C) 2001-2006  Simon Baldwin\n"
	"Copyright (C) 2011 Kamil Ignacak\n\n"
	"This program comes with ABSOLUTELY NO WARRANTY; for details please see\n"
	"the file 'COPYING' supplied with the source code.  This is free software,\n"
	"and you are welcome to redistribute it under certain conditions; again,\n"
	"see 'COPYING' for details. This program is released under the GNU General\n"
	"Public License.\n";


/**
 * cw_version()
 *
 * Returns the version number of the library.  Version numbers are returned
 * as an int, composed of major_version << 16 | minor_version.
 */
int
cw_version (void)
{
  unsigned int major = 0, minor = 0;

  sscanf (PACKAGE_VERSION, "%u.%u", &major, &minor);
  return major << 16 | minor;
}


/**
 * cw_license()
 *
 * Prints a short library licensing message to stdout.
 */
void
cw_license (void)
{
  printf ("cwlib version %s, %s\n", PACKAGE_VERSION, CW_COPYRIGHT);
}


/*---------------------------------------------------------------------*/
/*  Debugging flags and controls                                       */
/*---------------------------------------------------------------------*/

/* Current debug flags setting; no debug unless requested. */
static unsigned int cw_debug_flags = 0;


/**
 * cw_set_debug_flags()
 *
 * Sets a value for the library debug flags.  Debug output is generally
 * strings printed on stderr.  There is no validation of flags.
 */
void
cw_set_debug_flags (unsigned int new_value)
{
  cw_debug_flags = new_value;
}


/**
 * cw_get_debug_flags()
 *
 * Retrieves library debug flags.  If no flags are set, then on first
 * call, it will check the environment variable CWLIB_DEBUG, and if it
 * is available, will set debug flags to its value.  The provides a way
 * for a program to set the debug flags without needing to make any source
 * code changes.
 */
unsigned int
cw_get_debug_flags (void)
{
  static int is_initialized = FALSE;

  if (!is_initialized)
    {
      /* Do not overwrite any debug flags already set. */
      if (cw_debug_flags == 0)
        {
          const char *debug_value;

          /*
           * Set the debug flags from CWLIB_DEBUG.  If it is an invalid
           * numeric, treat it as 0; there is no error checking.
           */
          debug_value = getenv ("CWLIB_DEBUG");
          if (debug_value)
            cw_debug_flags = strtoul (debug_value, NULL, 0);
        }

      is_initialized = TRUE;
    }

  return cw_debug_flags;
}


/**
 * cw_is_debugging_internal()
 *
 * Returns TRUE if a given debugging flag is set.
 */
static int
cw_is_debugging_internal (unsigned int flag)
{
  return cw_get_debug_flags () & flag;
}


/*---------------------------------------------------------------------*/
/*  Core Morse code data and lookup                                    */
/*---------------------------------------------------------------------*/

/*
 * Morse code characters table.  This table allows lookup of the Morse shape
 * of a given alphanumeric character.  Shapes are held as a string, with '-'
 * representing dash, and '.' representing dot.  The table ends with a NULL
 * entry.
 */
typedef struct
{
  const char character;              /* Character represented */
  const char *const representation;  /* Dot-dash shape of the character */
} cw_entry_t;

static const cw_entry_t CW_TABLE[] = {
  /* ASCII 7bit letters */
  {'A', ".-"},    {'B', "-..."},  {'C', "-.-."},
  {'D', "-.."},   {'E', "."},     {'F', "..-."},
  {'G', "--."},   {'H', "...."},  {'I', ".."},
  {'J', ".---"},  {'K', "-.-"},   {'L', ".-.."},
  {'M', "--"},    {'N', "-."},    {'O', "---"},
  {'P', ".--."},  {'Q', "--.-"},  {'R', ".-."},
  {'S', "..."},   {'T', "-"},     {'U', "..-"},
  {'V', "...-"},  {'W', ".--"},   {'X', "-..-"},
  {'Y', "-.--"},  {'Z', "--.."},

  /* Numerals */
  {'0', "-----"},  {'1', ".----"},  {'2', "..---"},
  {'3', "...--"},  {'4', "....-"},  {'5', "....."},
  {'6', "-...."},  {'7', "--..."},  {'8', "---.."},
  {'9', "----."},

  /* Punctuation */
  {'"', ".-..-."},  {'\'', ".----."},  {'$', "...-..-"},
  {'(', "-.--."},   {')', "-.--.-"},   {'+', ".-.-."},
  {',', "--..--"},  {'-', "-....-"},   {'.', ".-.-.-"},
  {'/', "-..-."},   {':', "---..."},   {';', "-.-.-."},
  {'=', "-...-"},   {'?', "..--.."},   {'_', "..--.-"},
  {'@', ".--.-."},

  /* ISO 8859-1 accented characters */
  {'\334', "..--"},    /* U with diaeresis */
  {'\304', ".-.-"},    /* A with diaeresis */
  {'\307', "-.-.."},   /* C with cedilla */
  {'\326', "---."},    /* O with diaeresis */
  {'\311', "..-.."},   /* E with acute */
  {'\310', ".-..-"},   /* E with grave */
  {'\300', ".--.-"},   /* A with grave */
  {'\321', "--.--"},   /* N with tilde */

  /* ISO 8859-2 accented characters */
  {'\252', "----"},    /* S with cedilla */
  {'\256', "--..-"},   /* Z with dot above */

  /* Non-standard procedural signal extensions to standard CW characters. */
  {'<', "...-.-"},     /* VA/SK, end of work */
  {'>', "-...-.-"},    /* BK, break */
  {'!', "...-."},      /* SN, understood */
  {'&', ".-..."},      /* AS, wait */
  {'^', "-.-.-"},      /* KA, starting signal */
  {'~', ".-.-.."},     /* AL, paragraph */

  /* Sentinel end of table value */
  {0, NULL}
};


/**
 * cw_get_character_count()
 *
 * Returns the number of characters represented in the character lookup
 * table.
 */
int
cw_get_character_count (void)
{
  static int character_count = 0;

  if (character_count == 0)
    {
      const cw_entry_t *cw_entry;

      for (cw_entry = CW_TABLE; cw_entry->character; cw_entry++)
        character_count++;
    }

  return character_count;
}


/**
 * cw_list_characters()
 *
 * Returns into list a string containing all of the Morse characters
 * represented in the table.  The length of list must be at least one greater
 * than the number of characters represented in the character lookup table,
 * returned by cw_get_character_count.
 */
void
cw_list_characters (char *list)
{
  const cw_entry_t *cw_entry;
  int index;

  /* Append each table character to the output string. */
  index = 0;
  for (cw_entry = CW_TABLE; cw_entry->character; cw_entry++)
    list[index++] = cw_entry->character;

  list[index] = '\0';
}


/**
 * cw_get_maximum_representation_length()
 *
 * Returns the string length of the longest representation in the character
 * lookup table.
 */
int
cw_get_maximum_representation_length (void)
{
  static int maximum_length = 0;

  if (maximum_length == 0)
    {
      const cw_entry_t *cw_entry;

      /* Traverse the main lookup table, finding the longest. */
      for (cw_entry = CW_TABLE; cw_entry->character; cw_entry++)
        {
          int length;

          length = (int) strlen (cw_entry->representation);
          if (length > maximum_length)
            maximum_length = length;
        }
    }

  return maximum_length;
}


/**
 * cw_lookup_character_internal()
 *
 * Look up the given character, and return the representation of that
 * character.  Returns NULL if there is no table entry for the given
 * character.
 */
static const char *
cw_lookup_character_internal (char c)
{
  static const cw_entry_t *lookup[UCHAR_MAX];  /* Fast lookup table */
  static int is_initialized = FALSE;

  const cw_entry_t *cw_entry;

  /*
   * If this is the first call, set up the fast lookup table to give direct
   * access to the CW table for a given character.
   */
  if (!is_initialized)
    {
      if (cw_is_debugging_internal (CW_DEBUG_LOOKUPS))
        fprintf (stderr, "cw: initialize fast lookup table\n");

      for (cw_entry = CW_TABLE; cw_entry->character; cw_entry++)
        lookup[(unsigned char) cw_entry->character] = cw_entry;

      is_initialized = TRUE;
    }

  /*
   * There is no differentiation in the table between upper and lower case
   * characters; everything is held as uppercase.  So before we do the lookup,
   * we convert to ensure that both cases work.
   */
  c = toupper (c);

  /*
   * Now use the table to lookup the table entry.  Unknown characters return
   * NULL, courtesy of the fact that explicitly uninitialized static variables
   * are initialized to zero, so lookup[x] is NULL if it's not assigned to in
   * the above loop.
   */
  cw_entry = lookup[(unsigned char) c];

  if (cw_is_debugging_internal (CW_DEBUG_LOOKUPS))
    {
      if (cw_entry)
        fprintf (stderr, "cw: lookup '%c' returned <'%c':\"%s\">\n",
                 c, cw_entry->character, cw_entry->representation);
      else if (isprint (c))
        fprintf (stderr, "cw: lookup '%c' found nothing\n", c);
      else
        fprintf (stderr, "cw: lookup 0x%02x found nothing\n",
                 (unsigned char) c);
    }

  return cw_entry ? cw_entry->representation : NULL;
}


/**
 * cw_lookup_character()
 *
 * Returns the string 'shape' of a given Morse code character.  The routine
 * returns TRUE on success, and fills in the string pointer passed in.  On
 * error, it returns FALSE and sets errno to ENOENT, indicating that the
 * character could not be found.  The length of representation must be at
 * least one greater than the longest representation held in the character
 * lookup table, returned by cw_get_maximum_representation_length.
 */
int
cw_lookup_character (char c, char *representation)
{
  const char *retval;

  /* Lookup the character, and if found, return the string. */
  retval = cw_lookup_character_internal (c);
  if (retval)
    {
      if (representation)
        strcpy (representation, retval);
      return RC_SUCCESS;
    }

  /* Failed to find the requested character. */
  errno = ENOENT;
  return RC_ERROR;
}


/**
 * cw_hash_representation_internal()
 *
 * Return a hash value, in the range 2-255, for a lookup table representation.
 * The routine returns 0 if no valid hash could be made from the string.  To
 * avoid casting the value a lot in the caller (we want to use it as an array
 * index), we actually return an unsigned int.
 *
 * This hash algorithm is designed ONLY for valid CW representations; that is,
 * strings composed of only '.' and '-', and in this case, strings shorter than
 * eight characters.  The algorithm simply turns the representation into a
 * 'bitmask', based on occurrences of '.' and '-'.  The first bit set in the
 * mask indicates the start of data (hence the 7-character limit).  This mask
 * is viewable as an integer in the range 2 (".") to 255 ("-------"), and can
 * be used as an index into a fast lookup array.
 */
static unsigned int
cw_hash_representation_internal (const char *representation)
{
  int length, index;
  unsigned int hash;

  /*
   * Our algorithm can handle only 7 characters of representation.  And we
   * insist on there being at least one character, too.
   */
  length = (int) strlen (representation);
  if (length > CHAR_BIT - 1 || length < 1)
    return 0;

  /*
   * Build up the hash based on the dots and dashes; start at 1, the sentinel
   * (start) bit.
   */
  hash = 1;
  for (index = 0; index < length; index++)
    {
      /* Left-shift everything so far. */
      hash <<= 1;

      /*
       * If the next element is a dash, OR in another bit.  If it is not a
       * dash or a dot, then there is an error in the representation string.
       */
      if (representation[index] == CW_DASH_REPRESENTATION)
        hash |= 1;
      else if (representation[index] != CW_DOT_REPRESENTATION)
        return 0;
    }

  return hash;
}


/**
 * cw_lookup_representation_internal()
 *
 * Look up the given representation, and return the character that it
 * represents.  Returns zero if there is no table entry for the given
 * representation.
 */
static char
cw_lookup_representation_internal (const char *representation)
{
  static const cw_entry_t *lookup[UCHAR_MAX];  /* Fast lookup table */
  static int is_complete = TRUE;               /* Set to FALSE if there are any
                                                  lookup table entries not in
                                                  the fast lookup table */
  static int is_initialized = FALSE;

  const cw_entry_t *cw_entry;
  unsigned int hash;

  /*
   * If this is the first call, set up the fast lookup table to give direct
   * access to the CW table for a hashed representation.
   */
  if (!is_initialized)
    {
      if (cw_is_debugging_internal (CW_DEBUG_LOOKUPS))
        fprintf (stderr, "cw: initialize hash lookup table\n");

      /*
       * For each main table entry, create a hash entry.  If the hashing
       * of any entry fails, note that the table is not complete and ignore
       * that entry for now (for the current lookup table, this should not
       * happen).  The hashed table speeds up lookups of representations by
       * a factor of 5-10.
       */
      for (cw_entry = CW_TABLE; cw_entry->character; cw_entry++)
        {
          hash = cw_hash_representation_internal (cw_entry->representation);
          if (hash)
            lookup[hash] = cw_entry;
          else
            is_complete = FALSE;
        }

      if (!is_complete && cw_is_debugging_internal (CW_DEBUG_LOOKUPS))
        fprintf (stderr, "cw: hash lookup table incomplete\n");

      is_initialized = TRUE;
    }

  /* Hash the representation to get an index for the fast lookup. */
  hash = cw_hash_representation_internal (representation);

  /*
   * If the hashed lookup table is complete, we can simply believe any
   * hash value that came back.  That is, we just use what is at the index
   * 'hash', since this is either the entry we want, or NULL.
   */
  if (is_complete)
    cw_entry = lookup[hash];
  else
    {
      /*
       * If the hashed lookup table is not complete, the lookup might still
       * have found us the entry we are looking for.  Here, we'll check to
       * see if it did.
       */
      if (hash && lookup[hash] && lookup[hash]->representation
          && strcmp (lookup[hash]->representation, representation) == 0)
        {
          /* Found it in an incomplete table. */
          cw_entry = lookup[hash];
        }
      else
        {
          /*
           * We have no choice but to search the table entry by entry,
           * sequentially, from top to bottom.
           */
          for (cw_entry = CW_TABLE; cw_entry->character; cw_entry++)
            {
              if (strcmp (cw_entry->representation, representation) == 0)
                break;
            }

          /* If we got to the end of the table, return NULL. */
          cw_entry = cw_entry->character ? cw_entry : NULL;
        }
    }

  if (cw_is_debugging_internal (CW_DEBUG_LOOKUPS))
    {
      if (cw_entry)
        fprintf (stderr, "cw: lookup [0x%02x]'%s' returned <'%c':\"%s\">\n",
                 hash, representation,
                 cw_entry->character, cw_entry->representation);
      else
        fprintf (stderr, "cw: lookup [0x%02x]'%s' found nothing\n",
                 hash, representation);
    }

  return cw_entry ? cw_entry->character : 0;
}


/**
 * cw_check_representation()
 *
 * Checks that the given string is a valid Morse representation.  A valid
 * string is one composed of only '.' and '-' characters.  On success, the
 * routine returns TRUE.  On error, it returns FALSE, with errno set to EINVAL.
 */
int
cw_check_representation (const char *representation)
{
  int index;

  /* Check the characters in representation. */
  for (index = 0; representation[index]; index++)
    {
      if (representation[index] != CW_DOT_REPRESENTATION
          && representation[index] != CW_DASH_REPRESENTATION)
        {
          errno = EINVAL;
          return RC_ERROR;
        }
    }

  return RC_SUCCESS;
}


/**
 * cw_lookup_representation()
 *
 * Returns the character for a given Morse representation.  On success, the
 * routine returns TRUE, and fills in char *c.  On error, it returns FALSE,
 * and sets errno to EINVAL if any character of the representation is
 * invalid, or ENOENT to indicate that the representation could not be found.
 */
int
cw_lookup_representation (const char *representation, char *c)
{
  char character;

  /* Check the characters in representation. */
  if (!cw_check_representation (representation))
    {
      errno = EINVAL;
      return RC_ERROR;
    }

  /* Lookup the representation, and if found, return the character. */
  character = cw_lookup_representation_internal (representation);
  if (character)
    {
      if (c)
        *c = character;
      return RC_SUCCESS;
    }

  /* Failed to find the requested representation. */
  errno = ENOENT;
  return RC_ERROR;
}


/*---------------------------------------------------------------------*/
/*  Extended Morse code data and lookup                                */
/*---------------------------------------------------------------------*/

/*
 * Ancilliary procedural signals table.  This table maps procedural signal
 * characters in the main table to their expansions, along with a flag noting
 * if the character is usually expanded for display.
 */
typedef struct
{
  const char character;           /* Character represented */
  const char *const expansion;    /* Procedural expansion of the character */
  const int is_usually_expanded;  /* If expanded display is usual */
} cw_prosign_entry_t;

static const cw_prosign_entry_t CW_PROSIGN_TABLE[] = {
  /* Standard procedural signals */
  {'"', "AF", FALSE},   {'\'', "WG", FALSE},  {'$', "SX", FALSE},
  {'(', "KN", FALSE},   {')', "KK", FALSE},   {'+', "AR", FALSE},
  {',', "MIM", FALSE},  {'-', "DU", FALSE},   {'.', "AAA", FALSE},
  {'/', "DN", FALSE},   {':', "OS", FALSE},   {';', "KR", FALSE},
  {'=', "BT", FALSE},   {'?', "IMI", FALSE},  {'_', "IQ", FALSE},
  {'@', "AC", FALSE},

  /* Non-standard procedural signal extensions to standard CW characters. */
  {'<', "VA", TRUE},  /* VA/SK, end of work */
  {'>', "BK", TRUE},  /* BK, break */
  {'!', "SN", TRUE},  /* SN, understood */
  {'&', "AS", TRUE},  /* AS, wait */
  {'^', "KA", TRUE},  /* KA, starting signal */
  {'~', "AL", TRUE},  /* AL, paragraph */

  /* Sentinel end of table value */
  {0, NULL, FALSE}
};


/**
 * cw_get_procedural_character_count()
 *
 * Returns the number of characters represented in the procedural signal
 * expansion lookup table.
 */
int
cw_get_procedural_character_count (void)
{
  static int character_count = 0;

  if (character_count == 0)
    {
      const cw_prosign_entry_t *cw_prosign;

      for (cw_prosign = CW_PROSIGN_TABLE; cw_prosign->character; cw_prosign++)
        character_count++;
    }

  return character_count;
}


/**
 * cw_list_procedural_characters()
 *
 * Returns into list a string containing all of the Morse characters for which
 * procedural expansion are available.  The length of list must be at least
 * one greater than the number of characters represented in the procedural
 * signal expansion lookup table, returned by cw_get_procedural_character_count.
 */
void
cw_list_procedural_characters (char *list)
{
  const cw_prosign_entry_t *cw_prosign;
  int index;

  /* Append each table character to the output string. */
  index = 0;
  for (cw_prosign = CW_PROSIGN_TABLE; cw_prosign->character; cw_prosign++)
    list[index++] = cw_prosign->character;

  list[index] = '\0';
}


/**
 * cw_get_maximum_procedural_expansion_length()
 *
 * Returns the string length of the longest expansion in the procedural
 * signal expansion table.
 */
int
cw_get_maximum_procedural_expansion_length (void)
{
  static int maximum_length = 0;

  if (maximum_length == 0)
    {
      const cw_prosign_entry_t *cw_prosign;

      /* Traverse the main lookup table, finding the longest. */
      for (cw_prosign = CW_PROSIGN_TABLE; cw_prosign->character; cw_prosign++)
        {
          int length;

          length = (int) strlen (cw_prosign->expansion);
          if (length > maximum_length)
            maximum_length = length;
        }
    }

  return maximum_length;
}


/**
 * cw_lookup_procedural_character_internal()
 *
 * Look up the given procedural character, and return the expansion of that
 * procedural character, with a display hint in is_usually_expanded.  Returns
 * NULL if there is no table entry for the given character.
 */
static const char *
cw_lookup_procedural_character_internal (char c, int *is_usually_expanded)
{
  static const cw_prosign_entry_t *lookup[UCHAR_MAX];  /* Fast lookup table */
  static int is_initialized = FALSE;

  const cw_prosign_entry_t *cw_prosign;

  /*
   * If this is the first call, set up the fast lookup table to give direct
   * access to the procedural expansions table for a given character.
   */
  if (!is_initialized)
    {
      if (cw_is_debugging_internal (CW_DEBUG_LOOKUPS))
        fprintf (stderr, "cw: initialize prosign fast lookup table\n");

      for (cw_prosign = CW_PROSIGN_TABLE; cw_prosign->character; cw_prosign++)
        lookup[(unsigned char) cw_prosign->character] = cw_prosign;

      is_initialized = TRUE;
    }

  /*
   * Lookup the procedural signal table entry.  Unknown characters return
   * NULL.  All procedural signals are non-alphabetical, so no need to use
   * any uppercase coercion here.
   */
  cw_prosign = lookup[(unsigned char) c];

  if (cw_is_debugging_internal (CW_DEBUG_LOOKUPS))
    {
      if (cw_prosign)
        fprintf (stderr, "cw: prosign lookup '%c' returned <'%c':\"%s\":%d>\n",
                 c, cw_prosign->character,
                 cw_prosign->expansion, cw_prosign->is_usually_expanded);
      else if (isprint (c))
        fprintf (stderr, "cw: prosign lookup '%c' found nothing\n", c);
      else
        fprintf (stderr, "cw: prosign lookup 0x%02x found nothing\n",
                 (unsigned char) c);
    }

  /* If found, return any display hint and the expansion; otherwise, NULL. */
  if (cw_prosign)
    *is_usually_expanded = cw_prosign->is_usually_expanded;
  return cw_prosign ? cw_prosign->expansion : NULL;
}


/**
 * cw_lookup_procedural_character()
 *
 * Returns the string expansion of a given Morse code procedural signal
 * character.  The routine returns TRUE on success, filling in the string
 * pointer passed in and setting is_usuall_expanded to TRUE as a display
 * hint for the caller.  On error, it returns FALSE and sets errno to ENOENT,
 * indicating that the procedural signal character could not be found.  The
 * length of representation must be at least one greater than the longest
 * representation held in the procedural signal character lookup table,
 * returned by cw_get_maximum_procedural_expansion_length.
 */
int
cw_lookup_procedural_character (char c, char *representation,
                                int *is_usually_expanded)
{
  const char *retval;
  int is_expanded;

  /* Lookup, and if found, return the string and display hint. */
  retval = cw_lookup_procedural_character_internal (c, &is_expanded);
  if (retval)
    {
      if (representation)
        strcpy (representation, retval);
      if (is_usually_expanded)
        *is_usually_expanded = is_expanded;
      return RC_SUCCESS;
    }

  /* Failed to find the requested procedural signal character. */
  errno = ENOENT;
  return RC_ERROR;
}


/*
 * Phonetics table.  Not really CW, but it might be handy to have.  The
 * table contains ITU/NATO phonetics.
 */
static const char *const CW_PHONETICS[27] = {
  "Alfa", "Bravo", "Charlie", "Delta", "Echo", "Foxtrot", "Golf", "Hotel",
  "India", "Juliett", "Kilo", "Lima", "Mike", "November", "Oscar", "Papa",
  "Quebec", "Romeo", "Sierra", "Tango", "Uniform", "Victor", "Whiskey",
  "X-ray", "Yankee", "Zulu", NULL
};


/**
 * cw_get_maximum_phonetic_length()
 *
 * Returns the string length of the longest phonetic in the phonetics
 * lookup table.
 */
int
cw_get_maximum_phonetic_length (void)
{
  static int maximum_length = 0;

  if (maximum_length == 0)
    {
      int phonetic;

      /* Traverse the main lookup table, finding the longest. */
      for (phonetic = 0; CW_PHONETICS[phonetic]; phonetic++)
        {
          int length;

          length = (int) strlen (CW_PHONETICS[phonetic]);
          if (length > maximum_length)
            maximum_length = length;
        }
    }

  return maximum_length;
}


/**
 * cw_lookup_phonetic()
 *
 * Returns the phonetic of a given character.  The routine returns TRUE on
 * success, and fills in the string pointer passed in.  On error, it returns
 * FALSE and sets errno to ENOENT, indicating that the character could not be
 * found.  The length of phonetic must be at least one greater than the
 * longest phonetic held in the phonetic lookup table, returned by
 * cw_get_maximum_phonetic_length.
 */
int
cw_lookup_phonetic (char c, char *phonetic)
{
  /* Coerce to uppercase, and verify the input argument. */
  c = toupper (c);
  if (c >= 'A' && c <= 'Z')
    {
      if (phonetic)
        strcpy (phonetic, CW_PHONETICS[c - 'A']);
      return RC_SUCCESS;
    }

  /* No such phonetic. */
  errno = ENOENT;
  return RC_ERROR;
}


/*---------------------------------------------------------------------*/
/* Morse code controls and timing parameters                           */
/*---------------------------------------------------------------------*/

/* Dot length magic number; from PARIS calibration, 1 Dot=1200000/WPM usec. */
enum { DOT_CALIBRATION = 1200000 };

/* Limits on values of CW send and timing parameters */
enum
{ CW_MIN_SPEED = 4,            /* Lowest WPM allowed */
  CW_MAX_SPEED = 60,           /* Highest WPM allowed */
  CW_MIN_FREQUENCY = 0,        /* Lowest tone allowed (0=silent) */
  CW_MAX_FREQUENCY = 4000,     /* Highest tone allowed */
  CW_MIN_VOLUME = 0,           /* Quietest volume allowed (0=silent) */
  CW_MAX_VOLUME = 100,         /* Loudest volume allowed */
  CW_MIN_GAP = 0,              /* Lowest extra gap allowed */
  CW_MAX_GAP = 60,             /* Highest extra gap allowed */
  CW_MIN_TOLERANCE =0,         /* Lowest receive tolerance allowed */
  CW_MAX_TOLERANCE = 90,       /* Highest receive tolerance allowed */
  CW_MIN_WEIGHTING = 20,       /* Lowest weighting allowed */
  CW_MAX_WEIGHTING = 80        /* Highest weighting allowed */
};

/* Default initial values for library controls. */
enum
{ INITIAL_SEND_SPEED = 12,     /* Initial send speed in WPM */
  INITIAL_RECEIVE_SPEED = 12,  /* Initial receive speed in WPM */
  INITIAL_FREQUENCY = 800,     /* Initial tone in Hz */
  INITIAL_VOLUME = 70,         /* Initial volume percent */
  INITIAL_GAP = 0,             /* Initial gap setting */
  INITIAL_TOLERANCE = 50,      /* Initial tolerance setting */
  INITIAL_WEIGHTING = 50,      /* Initial weighting setting */
  INITIAL_ADAPTIVE = FALSE,    /* Initial adaptive receive setting */
  INITIAL_THRESHOLD = (DOT_CALIBRATION / INITIAL_RECEIVE_SPEED) * 2,
                               /* Initial adaptive speed threshold */
  INITIAL_NOISE_THRESHOLD = (DOT_CALIBRATION / CW_MAX_SPEED) / 2
                               /* Initial noise filter threshold */
};

/*
 * Library variables, indicating the user-selected parameters for generating
 * Morse code output and receiving Morse code input.  These values can be
 * set by client code; setting them may trigger a recalculation of the low
 * level timing values held and set below.
 */
static int cw_send_speed = INITIAL_SEND_SPEED,
           cw_gap = INITIAL_GAP,
           cw_receive_speed = INITIAL_RECEIVE_SPEED,
           cw_tolerance = INITIAL_TOLERANCE,
           cw_weighting = INITIAL_WEIGHTING,
           cw_is_adaptive_receive_enabled = INITIAL_ADAPTIVE,
           cw_noise_spike_threshold = INITIAL_NOISE_THRESHOLD;

/*
 * The following variables must be recalculated each time any of the above
 * Morse parameters associated with speeds, gap, tolerance, or threshold
 * change.  Keeping these in step means that we then don't have to spend time
 * calculating them on the fly.
 *
 * Since they have to be kept in sync, the problem of how to have them
 * calculated on first call if none of the above parameters has been
 * changed is taken care of with a synchronization flag.  Doing this saves
 * us from otherwise having to have a 'library initialize' function.
 */
static int cw_is_in_sync = FALSE,       /* Synchronization flag */
/* Sending parameters: */
           cw_send_dot_length = 0,      /* Length of a send Dot, in usec */
           cw_send_dash_length = 0,     /* Length of a send Dash, in usec */
           cw_end_of_ele_delay = 0,     /* Extra delay at the end of element */
           cw_end_of_char_delay = 0,    /* Extra delay at the end of a char */
           cw_additional_delay = 0,     /* More delay at the end of a char */
           cw_end_of_word_delay = 0,    /* Extra delay at the end of a word */
           cw_adjustment_delay = 0,     /* More delay at the end of a word */
/* Receiving parameters: */
           cw_receive_dot_length = 0,   /* Length of a receive Dot, in usec */
           cw_receive_dash_length = 0,  /* Length of a receive Dash, in usec */
           cw_dot_range_minimum = 0,    /* Shortest dot period allowable */
           cw_dot_range_maximum = 0,    /* Longest dot period allowable */
           cw_dash_range_minimum = 0,   /* Shortest dot period allowable */
           cw_dash_range_maximum = 0,   /* Longest dot period allowable */
           cw_eoe_range_minimum = 0,    /* Shortest end of ele allowable */
           cw_eoe_range_maximum = 0,    /* Longest end of ele allowable */
           cw_eoe_range_ideal = 0,      /* Ideal end of ele, for stats */
           cw_eoc_range_minimum = 0,    /* Shortest end of char allowable */
           cw_eoc_range_maximum = 0,    /* Longest end of char allowable */
           cw_eoc_range_ideal = 0;      /* Ideal end of char, for stats */

/*
 * Library variable which is automatically maintained from the Morse input
 * stream, rather than being settable by the user.
 */
static int cw_adaptive_receive_threshold = INITIAL_THRESHOLD;
                                        /* Initially 2-dot threshold for
                                           adaptive speed */


/**
 * cw_get_[speed|frequency|volume|gap|tolerance|weighting]_limits()
 *
 * Return the limits on the speed, frequency, volume, gap, tolerance, and
 * weighting parameters.  Normal values are speed 4-60 WPM, frequency
 * 0-10,000 Hz, volume 0-70 %, gap 0-20 dots, tolerance 0-90 %, and
 * weighting 20-80 %.
 */
void
cw_get_speed_limits (int *min_speed, int *max_speed)
{
  if (min_speed)
    *min_speed = CW_MIN_SPEED;
  if (max_speed)
    *max_speed = CW_MAX_SPEED;
}

void
cw_get_frequency_limits (int *min_frequency, int *max_frequency)
{
  if (min_frequency)
    *min_frequency = CW_MIN_FREQUENCY;
  if (max_frequency)
    *max_frequency = CW_MAX_FREQUENCY;
}

void
cw_get_volume_limits (int *min_volume, int *max_volume)
{
  if (min_volume)
    *min_volume = CW_MIN_VOLUME;
  if (max_volume)
    *max_volume = CW_MAX_VOLUME;
}

void
cw_get_gap_limits (int *min_gap, int *max_gap)
{
  if (min_gap)
    *min_gap = CW_MIN_GAP;
  if (max_gap)
    *max_gap = CW_MAX_GAP;
}

void
cw_get_tolerance_limits (int *min_tolerance, int *max_tolerance)
{
  if (min_tolerance)
    *min_tolerance = CW_MIN_TOLERANCE;
  if (max_tolerance)
    *max_tolerance = CW_MAX_TOLERANCE;
}

void
cw_get_weighting_limits (int *min_weighting, int *max_weighting)
{
  if (min_weighting)
    *min_weighting = CW_MIN_WEIGHTING;
  if (max_weighting)
    *max_weighting = CW_MAX_WEIGHTING;
}


/**
 * cw_sync_parameters_internal()
 *
 * Synchronize the dot, dash, end of element, end of character, and end
 * of word timings and ranges to new values of Morse speed, 'Farnsworth'
 * gap, receive tolerance, or weighting.
 */
static void
cw_sync_parameters_internal (void)
{
  /* Do nothing if we are already synchronized with speed/gap. */
  if (!cw_is_in_sync)
    {
      int unit_length, weighting_length;

      /*
       * Send parameters:
       *
       * Set the length of a Dot to be a Unit with any weighting adjustment,
       * and the length of a Dash as three Dot lengths.  The weighting
       * adjustment is by adding or subtracting a length based on 50 % as a
       * neutral weighting.
       */
      unit_length = DOT_CALIBRATION / cw_send_speed;
      weighting_length = (2 * (cw_weighting - 50) * unit_length) / 100;
      cw_send_dot_length = unit_length + weighting_length;
      cw_send_dash_length = 3 * cw_send_dot_length;

      /*
       * An end of element length is one Unit, perhaps adjusted, the end of
       * character is three Units total, and end of word is seven Units total.
       *
       * The end of element length is adjusted by 28/22 times weighting
       * length to keep PARIS calibration correctly timed (PARIS has 22 full
       * units, and 28 empty ones).  End of element and end of character
       * delays take weightings into account.
       */
      cw_end_of_ele_delay = unit_length - (28 * weighting_length) / 22;
      cw_end_of_char_delay = 3 * unit_length - cw_end_of_ele_delay;
      cw_end_of_word_delay = 7 * unit_length - cw_end_of_char_delay;
      cw_additional_delay = cw_gap * unit_length;

      /*
       * For 'Farnsworth', there also needs to be an adjustment delay added
       * to the end of words, otherwise the rhythm is lost on word end.  I
       * don't know if there is an "official" value for this, but 2.33 or so
       * times the gap is the correctly scaled value, and seems to sound
       * okay.
       *
       * Thanks to Michael D. Ivey <ivey@gweezlebur.com> for identifying this
       * in earlier versions of cwlib.
       */
      cw_adjustment_delay = (7 * cw_additional_delay) / 3;

      if (cw_is_debugging_internal (CW_DEBUG_PARAMETERS))
        {
          fprintf (stderr, "cw: send usec timings <%d>: %d, %d, %d, %d,"
                   " %d, %d, %d\n",
                   cw_send_speed,
                   cw_send_dot_length, cw_send_dash_length,
                   cw_end_of_ele_delay, cw_end_of_char_delay,
                   cw_end_of_word_delay, cw_additional_delay,
                   cw_adjustment_delay);
        }

      /*
       * Receive parameters:
       *
       * First, depending on whether we are set for fixed speed or adaptive
       * speed, calculate either the threshold from the receive speed, or the
       * receive speed from the threshold, knowing that the threshold is
       * always, effectively, two dot lengths.  Weighting is ignored for
       * receive parameters, although the core unit length is recalculated
       * for the receive speed, which may differ from the send speed.
       */
      unit_length = DOT_CALIBRATION / cw_receive_speed;
      if (cw_is_adaptive_receive_enabled)
        cw_receive_speed = DOT_CALIBRATION
                           / (cw_adaptive_receive_threshold / 2);
      else
        cw_adaptive_receive_threshold = 2 * unit_length;

      /*
       * Calculate the basic receive dot and dash lengths.
       */
      cw_receive_dot_length = unit_length;
      cw_receive_dash_length = 3 * unit_length;

      /*
       * Set the ranges of respectable timing elements depending very much on
       * whether we are required to adapt to the incoming Morse code speeds.
       */
      if (cw_is_adaptive_receive_enabled)
        {
          /*
           * For adaptive timing, calculate the Dot and Dash timing ranges
           * as zero to two Dots is a Dot, and anything, anything at all,
           * larger than this is a Dash.
           */
          cw_dot_range_minimum = 0;
          cw_dot_range_maximum = 2 * cw_receive_dot_length;
          cw_dash_range_minimum = cw_dot_range_maximum;
          cw_dash_range_maximum = INT_MAX;

          /*
           * Make the inter-element gap be anything up to the adaptive
           * threshold lengths - that is two Dots.  And the end of character
           * gap is anything longer than that, and shorter than five dots.
           */
          cw_eoe_range_minimum = cw_dot_range_minimum;
          cw_eoe_range_maximum = cw_dot_range_maximum;
          cw_eoc_range_minimum = cw_eoe_range_maximum;
          cw_eoc_range_maximum = 5 * cw_receive_dot_length;
        }
      else
        {
          int tolerance;

          /*
           * For fixed speed receiving, calculate the Dot timing range as the
           * Dot length +/- dot*tolerance%, and the Dash timing range as the
           * Dash length including +/- dot*tolerance% as well.
           */
          tolerance = (cw_receive_dot_length * cw_tolerance) / 100;
          cw_dot_range_minimum = cw_receive_dot_length - tolerance;
          cw_dot_range_maximum = cw_receive_dot_length + tolerance;
          cw_dash_range_minimum = cw_receive_dash_length - tolerance;
          cw_dash_range_maximum = cw_receive_dash_length + tolerance;

          /*
           * Make the inter-element gap the same as the Dot range.  Make the
           * inter-character gap, expected to be three Dots, the same as Dash
           * range at the lower end, but make it the same as the Dash range
           * _plus_ the 'Farnsworth' delay at the top of the range.
           *
           * Any gap longer than this is by implication inter-word.
           */
          cw_eoe_range_minimum = cw_dot_range_minimum;
          cw_eoe_range_maximum = cw_dot_range_maximum;
          cw_eoc_range_minimum = cw_dash_range_minimum;
          cw_eoc_range_maximum = cw_dash_range_maximum
                                 + cw_additional_delay + cw_adjustment_delay;
        }

      /*
       * For statistical purposes, calculate the ideal end of element and
       * end of character timings.
       */
      cw_eoe_range_ideal = unit_length;
      cw_eoc_range_ideal = 3 * unit_length;

      if (cw_is_debugging_internal (CW_DEBUG_PARAMETERS))
        {
          fprintf (stderr, "cw: receive usec timings <%d>: %d-%d, %d-%d,"
                   " %d-%d[%d], %d-%d[%d], %d\n",
                   cw_receive_speed,
                   cw_dot_range_minimum, cw_dot_range_maximum,
                   cw_dash_range_minimum, cw_dash_range_maximum,
                   cw_eoe_range_minimum, cw_eoe_range_maximum,
                   cw_eoe_range_ideal,
                   cw_eoc_range_minimum, cw_eoc_range_maximum,
                   cw_eoc_range_ideal,
                   cw_adaptive_receive_threshold);
        }

      /* Set the parameters in sync flag. */
      cw_is_in_sync = TRUE;
    }
}


/**
 * cw_reset_send_receive_parameters()
 *
 * Reset the library speed, frequency, volume, gap, tolerance, weighting,
 * adaptive receive, and noise spike threshold to their initial default values:
 * send/receive speed 12 WPM, volume 70 %, frequency 800 Hz, gap 0 dots,
 * tolerance 50 %, and weighting 50 %.
 */
void
cw_reset_send_receive_parameters (void)
{
  cw_send_speed = INITIAL_SEND_SPEED;
  generator->frequency = INITIAL_FREQUENCY;
  generator->volume = INITIAL_VOLUME;
  cw_gap = INITIAL_GAP;
  cw_receive_speed = INITIAL_RECEIVE_SPEED;
  cw_tolerance = INITIAL_TOLERANCE;
  cw_weighting = INITIAL_WEIGHTING;
  cw_is_adaptive_receive_enabled = INITIAL_ADAPTIVE;
  cw_noise_spike_threshold = INITIAL_NOISE_THRESHOLD;

  /* Changes require resynchronization. */
  cw_is_in_sync = FALSE;
  cw_sync_parameters_internal ();
}


/**
 * cw_set_[send_speed|receive_speed|frequency|volume|gap|tolerance|weighting]()
 * cw_get_[send_speed|receive_speed|frequency|volume|gap|tolerance|weighting]()
 *
 * Get and set routines for all the Morse code parameters available to
 * control the library.  Set routines return TRUE on success, or FALSE on
 * failure, with errno set to indicate the problem, usually EINVAL, except for
 * cw_set_receive_speed, which returns EINVAL if the new value is
 * invalid, or EPERM if the receive mode is currently set for adaptive
 * receive speed tracking.  Get routines simply return the current value.
 *
 * The default values of the parameters where none are explicitly set are
 * send/receive speed 12 WPM, volume 70 %, frequency 800 Hz, gap 0 dots,
 * tolerance 50 %, and weighting 50 %.  Note that volume settings are not
 * fully possible for the console speaker; in this case, volume settings
 * greater than zero indicate console speaker sound is on, and setting volume
 * to zero will turn off console speaker sound.
 */
int
cw_set_send_speed (int new_value)
{
  if (new_value < CW_MIN_SPEED || new_value > CW_MAX_SPEED)
    {
      errno = EINVAL;
      return RC_ERROR;
    }
  if (new_value != cw_send_speed)
    {
      cw_send_speed = new_value;

      /* Changes of send speed require resynchronization. */
      cw_is_in_sync = FALSE;
      cw_sync_parameters_internal ();
    }

  return RC_SUCCESS;
}

int
cw_set_receive_speed (int new_value)
{
  if (cw_is_adaptive_receive_enabled)
    {
      errno = EPERM;
      return RC_ERROR;
    }
  else
    {
      if (new_value < CW_MIN_SPEED || new_value > CW_MAX_SPEED)
        {
          errno = EINVAL;
          return RC_ERROR;
        }
    }
  if (new_value != cw_receive_speed)
    {
      cw_receive_speed = new_value;

      /* Changes of receive speed require resynchronization. */
      cw_is_in_sync = FALSE;
      cw_sync_parameters_internal ();
    }

  return RC_SUCCESS;
}

int
cw_set_frequency (int new_value)
{
  if (new_value < CW_MIN_FREQUENCY || new_value > CW_MAX_FREQUENCY)
    {
      errno = EINVAL;
      return RC_ERROR;
    }
  generator->frequency = new_value;
  return RC_SUCCESS;
}

int
cw_set_volume (int new_value)
{
  if (new_value < CW_MIN_VOLUME || new_value > CW_MAX_VOLUME)
    {
      errno = EINVAL;
      return RC_ERROR;
    }
  generator->volume = new_value;
  return RC_SUCCESS;
}

int
cw_set_gap (int new_value)
{
  if (new_value < CW_MIN_GAP || new_value > CW_MAX_GAP)
    {
      errno = EINVAL;
      return RC_ERROR;
    }
  if (new_value != cw_gap)
    {
      cw_gap = new_value;

      /* Changes of gap require resynchronization. */
      cw_is_in_sync = FALSE;
      cw_sync_parameters_internal ();
    }

  return RC_SUCCESS;
}

int
cw_set_tolerance (int new_value)
{
  if (new_value < CW_MIN_TOLERANCE || new_value > CW_MAX_TOLERANCE)
    {
      errno = EINVAL;
      return RC_ERROR;
    }
  if (new_value != cw_tolerance)
    {
      cw_tolerance = new_value;

      /* Changes of tolerance require resynchronization. */
      cw_is_in_sync = FALSE;
      cw_sync_parameters_internal ();
    }

  return RC_SUCCESS;
}

int
cw_set_weighting (int new_value)
{
  if (new_value < CW_MIN_WEIGHTING || new_value > CW_MAX_WEIGHTING)
    {
      errno = EINVAL;
      return RC_ERROR;
    }
  if (new_value != cw_weighting)
    {
      cw_weighting = new_value;

      /* Changes of weighting require resynchronization. */
      cw_is_in_sync = FALSE;
      cw_sync_parameters_internal ();
    }

  return RC_SUCCESS;
}

int
cw_get_send_speed (void)
{
  return cw_send_speed;
}

int
cw_get_receive_speed (void)
{
  return cw_receive_speed;
}

int
cw_get_frequency (void)
{
  return generator->frequency;
}

int
cw_get_volume (void)
{
  return generator->volume;
}

int
cw_get_gap (void)
{
  return cw_gap;
}

int
cw_get_tolerance (void)
{
  return cw_tolerance;
}

int
cw_get_weighting (void)
{
  return cw_weighting;
}


/**
 * cw_get_send_parameters()
 * cw_get_receive_parameters()
 *
 * Return the low-level timing parameters calculated from the speed, gap,
 * tolerance, and weighting set.  Parameter values are returned in
 * microseconds.  Use NULL for the pointer argument to any parameter value
 * not required.
 */
void
cw_get_send_parameters (int *dot_usecs, int *dash_usecs,
                        int *end_of_element_usecs,
                        int *end_of_character_usecs, int *end_of_word_usecs,
                        int *additional_usecs, int *adjustment_usecs)
{
  cw_sync_parameters_internal ();
  if (dot_usecs)
    *dot_usecs = cw_send_dot_length;
  if (dash_usecs)
    *dash_usecs = cw_send_dash_length;
  if (end_of_element_usecs)
    *end_of_element_usecs = cw_end_of_ele_delay;
  if (end_of_character_usecs)
    *end_of_character_usecs = cw_end_of_char_delay;
  if (end_of_word_usecs)
    *end_of_word_usecs = cw_end_of_word_delay;
  if (additional_usecs)
    *additional_usecs = cw_additional_delay;
  if (adjustment_usecs)
    *adjustment_usecs = cw_adjustment_delay;
}

void
cw_get_receive_parameters (int *dot_usecs, int *dash_usecs,
                           int *dot_min_usecs, int *dot_max_usecs,
                           int *dash_min_usecs, int *dash_max_usecs,
                           int *end_of_element_min_usecs,
                           int *end_of_element_max_usecs,
                           int *end_of_element_ideal_usecs,
                           int *end_of_character_min_usecs,
                           int *end_of_character_max_usecs,
                           int *end_of_character_ideal_usecs,
                           int *adaptive_threshold)
{
  cw_sync_parameters_internal ();
  if (dot_usecs)
    *dot_usecs = cw_receive_dot_length;
  if (dash_usecs)
    *dash_usecs = cw_receive_dash_length;
  if (dot_min_usecs)
    *dot_min_usecs = cw_dot_range_minimum;
  if (dot_max_usecs)
    *dot_max_usecs = cw_dot_range_maximum;
  if (dash_min_usecs)
    *dash_min_usecs = cw_dash_range_minimum;
  if (dash_max_usecs)
    *dash_max_usecs = cw_dash_range_maximum;
  if (end_of_element_min_usecs)
    *end_of_element_min_usecs = cw_eoe_range_minimum;
  if (end_of_element_max_usecs)
    *end_of_element_max_usecs = cw_eoe_range_maximum;
  if (end_of_element_ideal_usecs)
    *end_of_element_ideal_usecs = cw_eoe_range_ideal;
  if (end_of_character_min_usecs)
    *end_of_character_min_usecs = cw_eoc_range_minimum;
  if (end_of_character_max_usecs)
    *end_of_character_max_usecs = cw_eoc_range_maximum;
  if (end_of_character_ideal_usecs)
    *end_of_character_ideal_usecs = cw_eoc_range_ideal;
  if (adaptive_threshold)
    *adaptive_threshold = cw_adaptive_receive_threshold;
}


/**
 * cw_[gs]et_noise_spike_threshold()
 *
 * Set and get the period shorter than which, on receive, received tones are
 * ignored.  This allows the receive tone functions to apply noise canceling
 * for very short apparent tones.  For useful results the value should never
 * exceed the dot length of a dot at maximum speed; 20,000 microseconds (the
 * dot length at 60WPM).  Setting a noise threshold of zero turns off receive
 * tone noise canceling.  The default noise spike threshold is 10,000
 * microseconds.
 */
int
cw_set_noise_spike_threshold (int threshold)
{
  if (threshold < 0)
    {
      errno = EINVAL;
      return RC_ERROR;
    }
  cw_noise_spike_threshold = threshold;

  return RC_SUCCESS;
}

int
cw_get_noise_spike_threshold (void)
{
  return cw_noise_spike_threshold;
}


/*---------------------------------------------------------------------*/
/*  SIGALRM and timer handling                                         */
/*---------------------------------------------------------------------*/

/* Microseconds in a second, for struct timeval handling. */
static const int USECS_PER_SEC = 1000000;

/*
 * The library registers a single central SIGALRM handler.  This handler will
 * call all of the functions on a list sequentially on each SIGALRM received.
 * This is where the list is kept.
 */
enum { SIGALRM_HANDLERS = 32 };
static void (*cw_request_handlers[SIGALRM_HANDLERS]) (void);

/*
 * Flag to tell us if the SIGALRM handler is installed, and a place to keep
 * the old SIGALRM disposition, so we can restore it when the library decides
 * it can stop handling SIGALRM for a while.
 */
static int cw_is_sigalrm_handler_installed = FALSE;
static struct sigaction cw_sigalrm_original_disposition;

/* Forward declaration of finalization cancel function. */
static void cw_cancel_finalization_internal (void);


/**
 * cw_sigalrm_handler_internal()
 *
 * Common SIGALRM handler.  This function calls the signal handlers of the
 * library subsystems, expecting them to ignore unexpected calls.
 */
static void
cw_sigalrm_handler_internal (int signal_number)
{
  int handler;

  /* Avoid compiler warnings about unused argument. */
  signal_number = 0;

  /*
   * Call the known functions that are interested in this signal.  Stop on
   * the first free slot found; valid because the array is filled in order
   * from index 0, and there are no deletions.
   */
  for (handler = 0;
       handler < SIGALRM_HANDLERS && cw_request_handlers[handler]; handler++)
    {
      void (*request_handler) (void);

      request_handler = cw_request_handlers[handler];
      (*request_handler) ();
    }
}


/**
 * cw_set_timer_internal()
 *
 * Convenience function to set the itimer for a single shot timeout after a
 * given number of microseconds.
 */
static int
cw_set_timer_internal (int usecs)
{
  int status;
  struct itimerval itimer;

  /* Set up a single shot timeout for the given period. */
  itimer.it_interval.tv_sec = 0;
  itimer.it_interval.tv_usec = 0;
  itimer.it_value.tv_sec = usecs / USECS_PER_SEC;
  itimer.it_value.tv_usec = usecs % USECS_PER_SEC;
  status = setitimer (ITIMER_REAL, &itimer, NULL);
  if (status == -1)
    {
      perror ("cw: setitimer");
      return RC_ERROR;
    }

  return RC_SUCCESS;
}


/**
 * cw_request_timeout_internal()
 *
 * Install the SIGALRM handler, if not yet installed.  Add any given lower
 * level handler to the list of registered handlers.  Then set an itimer
 * to expire after the requested number of microseconds.
 */
static int
cw_request_timeout_internal (int usecs, void (*request_handler) (void))
{
  struct sigaction action;

  /* Install the SIGALRM handler if not currently installed. */
  if (!cw_is_sigalrm_handler_installed)
    {
      int status;

      /*
       * Register the SIGALRM handler routine, and keep the old information
       * so we can put it back when useful to do so.
       */
      action.sa_handler = cw_sigalrm_handler_internal;
      action.sa_flags = SA_RESTART;
      sigemptyset (&action.sa_mask);
      status = sigaction (SIGALRM, &action, &cw_sigalrm_original_disposition);
      if (status == -1)
        {
          perror ("cw: sigaction");
          return RC_ERROR;
        }

      cw_is_sigalrm_handler_installed = TRUE;
    }

  /*
   * If it's not already present, and one was given, add the request handler
   * address to the list of known handlers.
   */
  if (request_handler)
    {
      int handler;

      /*
       * Search for this handler, or the first free entry, stopping at the
       * last entry in the table even if it's not a match and not free.
       */
      for (handler = 0; handler < SIGALRM_HANDLERS - 1; handler++)
        {
          if (!cw_request_handlers[handler]
              || cw_request_handlers[handler] == request_handler)
            break;
        }

      /*
       * If the handler is already there, do no more.  Otherwise, if we ended
       * the search at an unused entry, add it to the list of lower level
       * handlers.
       */
      if (cw_request_handlers[handler] != request_handler)
        {
          if (cw_request_handlers[handler])
            {
              errno = ENOMEM;
              perror ("cw: overflow cw_request_handlers");
              return RC_ERROR;
            }
          cw_request_handlers[handler] = request_handler;
        }
    }

  /*
   * The fact that we receive a call means that something is using timeouts
   * and sound, so make sure that any pending finalization doesn't happen.
   */
  cw_cancel_finalization_internal ();

  /*
   * Depending on the value of usec, either set an itimer, or send ourselves
   * SIGALRM right away.
   */
  if (usecs <= 0)
    {
      /* Send ourselves SIGALRM immediately. */
      if (raise (SIGALRM) != 0)
        {
          perror ("cw: raise");
          return RC_ERROR;
        }
    }
  else
    {
      /*
       * Set the itimer to produce a single interrupt after the given
       * duration.
       */
      if (!cw_set_timer_internal (usecs))
        return RC_ERROR;
    }

  return RC_SUCCESS;
}


/**
 * cw_release_timeouts_internal()
 *
 * Uninstall the SIGALRM handler, if installed.  Return SIGALRM's disposition
 * for the system to the state we found it in before we installed our own
 * SIGALRM handler.
 */
static int
cw_release_timeouts_internal (void)
{
  /* Ignore the call if we haven't installed our handler. */
  if (cw_is_sigalrm_handler_installed)
    {
      int status;

      /* Cancel any pending itimer setting. */
      if (!cw_set_timer_internal (0))
        return RC_ERROR;

      /* Put back the SIGALRM information saved earlier. */
      status = sigaction (SIGALRM, &cw_sigalrm_original_disposition, NULL);
      if (status == -1)
        {
          perror ("cw: sigaction");
          return RC_ERROR;
        }

      cw_is_sigalrm_handler_installed = FALSE;
    }

  return RC_SUCCESS;
}


/**
 * cw_check_signal_mask_internal()
 *
 * Check the signal mask of the process, and return an error, with errno
 * set to EDEADLK, if SIGALRM is blocked.
 */
static int
cw_check_signal_mask_internal (void)
{
  int status;
  sigset_t empty_set, current_set;

  /* Block a empty set of signals to obtain the current mask. */
  sigemptyset (&empty_set);
  status = sigprocmask (SIG_BLOCK, &empty_set, &current_set);
  if (status == -1)
    {
      perror ("cw: sigprocmask");
      return RC_ERROR;
    }

  /* Check that SIGALRM is not blocked in the current mask. */
  if (sigismember (&current_set, SIGALRM))
    {
      errno = EDEADLK;
      return RC_ERROR;
    }

  return RC_SUCCESS;
}


/**
 * cw_block_signal_internal()
 *
 * Block SIGALRM for the duration of certain critical sections, or unblock
 * after; passed TRUE to block SIGALRM, and FALSE to unblock.
 */
static int
cw_block_signal_internal (int is_block)
{
  int status;
  sigset_t block_set;

  /* Block or unblock SIGALRM for the process. */
  sigemptyset (&block_set);
  sigaddset (&block_set, SIGALRM);
  status = sigprocmask (is_block ? SIG_BLOCK : SIG_UNBLOCK, &block_set, NULL);
  if (status == -1)
    {
      perror ("cw: sigprocmask");
      return RC_ERROR;
    }

  return RC_SUCCESS;
}


/**
 * cw_block_callback()
 *
 * Blocks the callback from being called for a critical section of caller
 * code if is_block is TRUE, and unblocks the callback if block is FALSE.
 * Works by blocking SIGALRM; a block should always be matched by an unblock,
 * otherwise the tone queue will suspend forever.
 */
void
cw_block_callback (int is_block)
{
  cw_block_signal_internal (is_block);
}


/**
 * cw_wait_for_signal_internal()
 *
 * Wait for a signal, usually a SIGALRM.  Assumes SIGALRM is not blocked.
 */
static int
cw_wait_for_signal_internal (void)
{
  int status;
  sigset_t empty_set, current_set;

  /* Block a empty set of signals to obtain the current mask. */
  sigemptyset (&empty_set);
  status = sigprocmask (SIG_BLOCK, &empty_set, &current_set);
  if (status == -1)
    {
      perror ("cw: sigprocmask");
      return RC_ERROR;
    }

  /* Wait on the current mask. */
  status = sigsuspend (&current_set);
  if (status == -1 && errno != EINTR)
    {
      perror ("cw: sigsuspend");
      return RC_ERROR;
    }

  return RC_SUCCESS;
}


/*---------------------------------------------------------------------*/
/*  Console and soundcard control                                      */
/*---------------------------------------------------------------------*/

/* 0Hz = silent 'tone'. */
enum { TONE_SILENT = 0 };

/* Fixed general soundcard parameters. */
static const int DSP_SETFRAGMENT = 7,   /* Sound fragment size, 128 (2^7) */
                 DSP_FORMAT = AFMT_U8,  /* Sound format */
                 DSP_CHANNELS = 1,      /* Sound in mono only */
                 DSP_RATE = 8192;       /* Sound sampling rate (~=2xMaxFreq) */

/*
 * Paths to console and soundcard devices used by the library.
 * These can be reset by the user.
 */
static char *cw_console_device = NULL;
static char *cw_oss_device = NULL;


/* Current background tone generation frequency. */
static int cw_sound_generate_frequency = TONE_SILENT;

/* DSP device sample rate, and saved mixer PCM volume setting. */
static int cw_sound_sample_rate = 0;
//           cw_sound_saved_vol = 0;

/*
 * Sound output control flags, to enable soundcard, console, or both.  By
 * default, console output is disabled, and soundcard enabled.
 */
static int cw_sound_soundcard_on = TRUE,
           cw_sound_console_on = FALSE;


/**
 * cw_[gs]et_console_file()
 *
 * Get and set routines for the path to the console device through which the
 * library generates PC speaker sound.  The set routine does not return any
 * indication of whether the device is a valid console; for that, use
 * cw_is_console_possible() to test to see if the value passed in might be an
 * acceptable console device.
 *
 * The default console file is /dev/console.  The call is ingored on platforms
 * that do no support the KIOCSOUND ioctl.
 */
int cw_set_console_device(const char *device)
{
	assert (!cw_console_device); /* this should be NULL, either
					because it has been initialized
					statically as NULL, or set to
					NULL by generator destructor */

	if (device) {
		cw_console_device = strdup(device);
	} else {
		cw_console_device = strdup(CW_DEFAULT_CONSOLE_DEVICE);
	}

	if (cw_console_device) {
		return RC_SUCCESS;
	} else {
		fprintf(stderr, "cw: malloc\n");
		return RC_ERROR;
	}
}

const char *cw_get_console_device(void)
{
	return cw_console_device;
}


/**
 * cw_[gs]et_soundcard_file()
 *
 * Get and set routines for the path to the sound device through which the
 * library generates soundcard sound.  The set routine does not return
 * any indication of whether the device is a valid soundcard; use
 * cw_is_soundcard_possible() to test to see if the value passed in might
 * be an acceptable soundcard device.
 *
 * The default soundcard file is /dev/audio.
 */
int cw_set_soundcard_device(const char *device)
{
	assert (!cw_oss_device); /* this should be NULL, either
				    because it has been initialized
				    statically as NULL, or set to
				    NULL by generator destructor */
	if (device) {
		cw_oss_device = strdup(device);
	} else {
		cw_oss_device = strdup(CW_DEFAULT_OSS_DEVICE);
	}

	if (cw_oss_device) {
		return RC_SUCCESS;
	} else {
		fprintf(stderr, "cw: malloc\n");
		return RC_ERROR;
	}
}

const char *cw_get_soundcard_device(void)
{
  return cw_oss_device;
}

#if 0
/**
 * cw_[gs]et_soundmixer_file()
 *
 * Get and set routines for the path to the mixer device that the library
 * may use to gate soundcard sound.  The library uses the mixer device
 * where the main sound device does not support the PCM volume control
 * ioctl call; this tends to occur on newer OSS drivers, but not on older
 * ones, or on ALSA in OSS emulation mode.  The set routine does not return
 * any indication of whether the device is a valid mixer.
 *
 * The default sound mixer file is /dev/mixer.
 */
void
cw_set_soundmixer_file (const char *new_value)
{
  static char *allocated = NULL;

  free (allocated);
  allocated = new_value ? malloc (strlen (new_value) + 1) : NULL;
  if (allocated)
    strcpy (allocated, new_value);
  cw_mixer_device = allocated;
}

const char *
cw_get_soundmixer_file (void)
{
  return cw_mixer_device;
}
#endif

/**
 * cw_is_soundcard_possible()
 *
 * Return success status if it appears that the library will be able to
 * generate tones through a sound card.  If it appears that a sound card
 * may not work, the routine returns FALSE to indicate failure.
 *
 * The function tests only that the given sound card file exists and is
 * writable.
 */
int
cw_is_soundcard_possible (void)
{
  if (generator->sound_system == CW_AUDIO_OSS)
    return RC_SUCCESS;

  if (!cw_oss_device)
    {
      errno = EINVAL;
      return RC_ERROR;
    }

  if (access (cw_oss_device, W_OK) == -1)
    {
      return RC_ERROR;
    }

  return RC_SUCCESS;
}


/**
 * cw_is_console_possible()
 *
 * Return success status if it appears that the library will be able to
 * generate tones through the console speaker.  If it appears that console
 * sound may not work, the routine returns FALSE to indicate failure.
 *
 * The function tests that the given console file exists, and that it will
 * accept the KIOCSOUND ioctl.  It unconditionally returns FALSE on platforms
 * that do no support the KIOCSOUND ioctl.
 *
 * Call to ioctl will fail if calling code doesn't have root privileges.
 */
int cw_is_console_possible(const char *device)
{
#if defined(KIOCSOUND)
	/* no need to allocate space for device path, just a
	   pointer (to a memory allocated somewhere else by
	   someone else) will be sufficient in local scope */
	const char *device_path = device ? device : CW_DEFAULT_CONSOLE_DEVICE;

	int fd = open(device_path, O_WRONLY);
	if (fd == -1) {
		return RC_ERROR;
	}
	if (ioctl(fd, KIOCSOUND, 0) == -1) {
		close(fd);
		return RC_ERROR;
	} else {
		close(fd);
		return RC_SUCCESS;
	}
#else
	return RC_ERROR;
#endif
}


/*---------------------------------------------------------------------*/
/*  Console and soundcard tone generation                              */
/*---------------------------------------------------------------------*/

/*
 * Clock tick rate used for KIOCSOUND console ioctls.  This value is taken
 * from linux/include/asm-i386/timex.h, included here for portability.
 */
static const int KIOCSOUND_CLOCK_TICK_RATE = 1193180;

#if 0
/**
 * cw_get_sound_pcm_volume_internal()
 *
 * Return the PCM channel volume setting for the soundcard.  This function
 * tries to apply the PCM volume read ioctl() to the main soundcard device
 * first.  If this fails, it retries using the PCM channel of the given
 * mixer device, if it has one.  The function returns the retrieved volume
 * set on success.
 */
static int
cw_get_sound_pcm_volume_internal (int *volume)
{
  int read_volume, mixer, device_mask;

  /* Try to use the main /dev/audio device for ioctls first. */
  if (ioctl (cw_sound_descriptor,
             MIXER_READ (SOUND_MIXER_PCM), &read_volume) == 0)
    {
      *volume = read_volume;
      return RC_SUCCESS;
    }

  /* Volume not found; try the mixer PCM channel volume instead. */
  mixer = open (cw_mixer_device, O_RDONLY | O_NONBLOCK);
  if (mixer == -1)
    {
      fprintf (stderr, "cw: open ");
      perror (cw_mixer_device);
      return RC_ERROR;
    }

  /* Check the available mixer channels. */
  if (ioctl (mixer, SOUND_MIXER_READ_DEVMASK, &device_mask) == -1)
    {
      perror ("cw: ioctl SOUND_MIXER_READ_DEVMASK");
      close (mixer);
      return RC_ERROR;
    }
  if (device_mask & SOUND_MASK_PCM)
    {
      /* Read the PCM volume from the mixer. */
      if (ioctl (mixer, MIXER_READ (SOUND_MIXER_PCM), &read_volume) == -1)
        {
          perror ("cw: ioctl MIXER_READ(SOUND_MIXER_PCM)");
          close (mixer);
          return RC_ERROR;
        }

      *volume = read_volume;
      close (mixer);
      return RC_SUCCESS;
    }
  else
    {
      if (device_mask & SOUND_MASK_VOLUME)
        {
          /* If in extreme difficulty, try main volume. */
          if (ioctl (mixer,
                     MIXER_READ (SOUND_MIXER_VOLUME), &read_volume) == -1)
            {
              perror ("cw: ioctl MIXER_READ(SOUND_MIXER_VOLUME)");
              close (mixer);
              return RC_ERROR;
            }

          *volume = read_volume;
          close (mixer);
          return RC_SUCCESS;
        }
    }

  /* There seems to be no way to control volumes. */
  errno = EINVAL;
  perror ("cw: mixer DEVMASK lacks volume controls");
  close (mixer);
  return RC_ERROR;
}
#endif

/**
 * cw_set_sound_pcm_volume_internal()
 *
 * Set the PCM channel volume setting for the soundcard.  This function
 * tries to apply the PCM volume write ioctl() to the main soundcard device
 * first.  If this fails, it retries using the PCM channel of the given
 * mixer device, if it has one.
 */
static int
cw_set_sound_pcm_volume_internal (int volume)
{
	if (volume == 0) {
		/* With this new scheme of producing a sound
		   the sound is a bit longer than a dot (or a
		   dash) by a time needed to decrease amplitude
		   of sine wave from 'volume' to zero. When a
		   timer signals that it's time to stop
		   generating a sound, the library proceeds to
		   gradually decrease amplitude of sine wave
		   producing a falling slope. Length of the
		   slope is inversely proportional to
		   CW_OSS_GENERATOR_SLOPE.

		   This additional time used to generate falling
		   slope is rather small but cannot be tolerated.
		   Thus I need to come up with better way of
		   ending a sound. Somehow the slope must appear
		   (i.e. the amplitude needs to start decreasing)
		   *before* expected end of sound. */
		generator->slope = -CW_OSS_GENERATOR_SLOPE;
	} else {
		generator->slope = CW_OSS_GENERATOR_SLOPE;
	}

	return RC_SUCCESS;
#if 0
  int mixer, device_mask;

  /* Try to use the main /dev/audio device for ioctls first. */
  if (ioctl (generator->audio_sink,
             MIXER_WRITE (SOUND_MIXER_PCM), &volume) == 0)
    return RC_SUCCESS;

  /* Try the mixer PCM channel volume instead. */
  mixer = open (cw_mixer_device, O_RDWR | O_NONBLOCK);
  if (mixer == -1)
    {
      fprintf (stderr, "cw: open ");
      perror (cw_mixer_device);
      return RC_ERROR;
    }

  /* Check the available mixer channels. */
  if (ioctl (mixer, SOUND_MIXER_READ_DEVMASK, &device_mask) == -1)
    {
      perror ("cw: ioctl SOUND_MIXER_READ_DEVMASK");
      close (mixer);
      return RC_ERROR;
    }
  if (device_mask & SOUND_MASK_PCM)
    {
      /* Write the PCM volume to the mixer. */
      if (ioctl (mixer, MIXER_WRITE (SOUND_MIXER_PCM), &volume) == -1)
        {
          perror ("cw: ioctl MIXER_WRITE(SOUND_MIXER_PCM)");
          close (mixer);
          return RC_ERROR;
        }

      close (mixer);
      return RC_SUCCESS;
    }
  else
    {
      if (device_mask & SOUND_MASK_VOLUME)
        {
          /* If in extreme difficulty, try main volume. */
          if (ioctl (mixer,
                     MIXER_WRITE (SOUND_MIXER_VOLUME), &volume) == -1)
            {
              perror ("cw: ioctl MIXER_WRITE(SOUND_MIXER_VOLUME)");
              close (mixer);
              return RC_ERROR;
            }

          close (mixer);
          return RC_SUCCESS;
        }
    }

  /* There seems to be no way to control volumes. */
  errno = EINVAL;
  perror ("cw: mixer DEVMASK lacks volume controls");
  close (mixer);
  return RC_ERROR;
#endif
}


#if 0
/**
 * cw_open_sound_soundcard_internal()
 *
 * Open and initialize the sound device for Morse code tones.  This function
 * sets suitable sampling rates and formats, and notes the file descriptor
 * and the sample rate set for the device in library variables.
 */
static int
cw_open_sound_soundcard_internal (void)
{
  /* Ignore the call if the sound device is already open. */
  if (!(generator->sound_system == CW_AUDIO_OSS))
    {
      int soundcard, parameter;

      /* Open the given soundcard device file, for write only. */
      soundcard = open (cw_sound_device, O_WRONLY | O_NONBLOCK);
      if (soundcard == -1)
        {
          fprintf (stderr, "cw: open ");
          perror (cw_sound_device);
          return RC_ERROR;
        }

      /*
       * Live a little dangerously, by trying to set the fragment size of the
       * card.  We'll try for a relatively short fragment of 128 bytes.  This
       * gives us a little better granularity over the amounts of audio data
       * we write periodically to the soundcard output buffer.  We may not get
       * the requested fragment size, and may be stuck with the default.  The
       * argument has the format 0xMMMMSSSS - fragment size is 2^SSSS, and
       * setting 0x7fff for MMMM allows as many fragments as the driver can
       * support.
       */
      parameter = 0x7fff << 16 | DSP_SETFRAGMENT;
      if (ioctl (soundcard, SNDCTL_DSP_SETFRAGMENT, &parameter) == -1)
        {
          perror ("cw: ioctl SNDCTL_DSP_SETFRAGMENT");
          close (soundcard);
          return RC_ERROR;
        }

      /* Set the audio format to 8-bit unsigned. */
      parameter = DSP_FORMAT;
      if (ioctl (soundcard, SNDCTL_DSP_SETFMT, &parameter) == -1)
        {
          perror ("cw: ioctl SNDCTL_DSP_SETFMT");
          close (soundcard);
          return RC_ERROR;
        }
      if (parameter != DSP_FORMAT)
        {
          errno = ERR_NO_SUPPORT;
          perror ("cw: sound AFMT_U8 not supported");
          close (soundcard);
          return RC_ERROR;
        }

      /* Set up mono mode - a single audio channel. */
      parameter = DSP_CHANNELS;
      if (ioctl (soundcard, SNDCTL_DSP_CHANNELS, &parameter) == -1)
        {
          perror ("cw: ioctl SNDCTL_DSP_CHANNELS");
          close (soundcard);
          return RC_ERROR;
        }
      if (parameter != DSP_CHANNELS)
        {
          errno = ERR_NO_SUPPORT;
          perror ("cw: sound mono not supported");
          close (soundcard);
          return RC_ERROR;
        }

      /*
       * Set up a standard sampling rate based on the notional correct value,
       * and retain the one we actually get in the library variable.
       */
      cw_sound_sample_rate = DSP_RATE;
      if (ioctl (soundcard, SNDCTL_DSP_SPEED, &cw_sound_sample_rate) == -1)
        {
          perror ("cw: ioctl SNDCTL_DSP_SPEED");
          close (soundcard);
          return RC_ERROR;
        }
      if (cw_sound_sample_rate != DSP_RATE)
        {
          if (cw_is_debugging_internal (CW_DEBUG_SOUND))
            fprintf (stderr,
                     "cw: dsp sample_rate -> %d\n", cw_sound_sample_rate);
        }

      /* Query fragment size just to get the driver buffers set. */
      if (ioctl (soundcard, SNDCTL_DSP_GETBLKSIZE, &parameter) == -1)
        {
          perror ("cw: ioctl SNDCTL_DSP_GETBLKSIZE");
          close (soundcard);
          return RC_ERROR;
        }
      if (parameter != (1 << DSP_SETFRAGMENT))
        {
          if (cw_is_debugging_internal (CW_DEBUG_SOUND))
            fprintf (stderr, "cw: dsp fragment size not set, %d\n", parameter);
        }

      /*
       * Save the opened file descriptor in a library variable.  Do it now
       * rather than later since the volume setting functions try to use it.
       */
      cw_sound_descriptor = soundcard;

      /* Save the current volume setting of the sound device. */
      if (!cw_get_sound_pcm_volume_internal (&cw_sound_saved_vol))
        {
          close (cw_sound_descriptor);
          cw_sound_descriptor = -1;
          return RC_ERROR;
        }

      /* Set the mixer volume to zero, so the card is silent initially. */
      if (!cw_set_sound_pcm_volume_internal (0))
        {
          close (cw_sound_descriptor);
          cw_sound_descriptor = -1;
          return RC_ERROR;
        }

      if (cw_is_debugging_internal (CW_DEBUG_SOUND))
        fprintf (stderr, "cw: dsp opened\n");

      /* Note sound as now open for business. */
      cw_is_sound_open = TRUE;
    }

  return RC_SUCCESS;
}


/**
 * cw_close_sound_soundcard_internal()
 *
 * Flush the soundcard output buffer, and close the dsp device file.
 */
static int
cw_close_sound_soundcard_internal (void)
{
  /* Only attempt to close the sound device if it is already open. */
  if (cw_is_sound_open)
    {
      /* Stop any and all current tone output. */
      if (ioctl (cw_sound_descriptor, SNDCTL_DSP_RESET, 0) == -1)
        {
          /* This will make the close take forever... */
          perror ("cw: ioctl SNDCTL_DSP_RESET");
        }

      /* Restore the saved volume. */
      if (!cw_set_sound_pcm_volume_internal (cw_sound_saved_vol))
        return RC_ERROR;

      /* Close the file descriptor, and note as closed. */
      close (cw_sound_descriptor);
      cw_sound_descriptor = -1;
      cw_is_sound_open = FALSE;

      if (cw_is_debugging_internal (CW_DEBUG_SOUND))
        fprintf (stderr, "cw: dsp flushed and closed\n");
    }

  return RC_SUCCESS;
}
#endif

/**
 * cw_generate_sound_internal()
 *
 * If we are currently charged with generating soundcard tone data, put a
 * quantum of audio, at the current sampling rate, into the DSP device.  The
 * quantum is at most about a second's worth of audio data -  the function
 * stops at this point, or when it finds that the soundcard output buffer is
 * full.  The function checks the current sound generate frequency, and goes
 * idle if this is set to TONE_SILENT prior to a call.
 *
 * Two seconds of audio should be sufficient for all current uses of the tone
 * queue.  For CW at the slowest rate supported by the library (4 WPM), a dash
 * is 900 ms (3 dots, of 1200 / 4 ms), so for normal use by the sending and
 * iambic keying functions, there will not be a case where dequeued tones
 * happen less frequently than once per second, and since dequeueing tones
 * calls this function, the soundcard data buffer cannot underrun.  The
 * problem case is where the straight key is held down for more than one
 * second.  To avoid underruns, the straight key functions can use the timeout
 * callback to regularly call this function, ensuring that the soundcard
 * buffer remains populated.
 */
static int
cw_generate_sound_internal (void)
{
	return RC_SUCCESS;
#if 0
  static const int AMPLITUDE = 100;            /* Wave amplitude multiplier */
  static double phase_offset = 0.0;            /* Wave shape phase offset */
  static int current_frequency = TONE_SILENT;  /* Note of freq on last call */

  /* Print out some debug trace on a call to generate audio data. */
  if (cw_is_debugging_internal (CW_DEBUG_SOUND))
    fprintf (stderr,
             "cw: dsp generate request, %d Hz\n", cw_sound_generate_frequency);

  /* Look first for a switch to silence in the generated tone. */
  if (cw_sound_generate_frequency == TONE_SILENT)
    {
      if (current_frequency != TONE_SILENT)
        {
          /* Stop generating soundcard tones, and go idle. */
          cw_close_sound_soundcard_internal ();

          /* Clear the current generated tone and phase. */
          current_frequency = TONE_SILENT;
          phase_offset = 0.0;

          if (cw_is_debugging_internal (CW_DEBUG_SOUND))
            fprintf (stderr, "cw: dsp goes idle\n");
        }
    }
  else
    {
      /* Not silent; handle any change of tone generation variable. */
      if (cw_sound_generate_frequency != current_frequency)
        {
          /*
           * To fully reset to a new tone frequency, close the audio device,
           * then reopen it.  This seems a little blunt, but it's the
           * recommended action according to the OSS doc.
           */
          if (current_frequency != TONE_SILENT)
            cw_close_sound_soundcard_internal ();
          if (!cw_open_sound_soundcard_internal ())
            return RC_ERROR;

          /* Reset the current generated tone and phase. */
          current_frequency = cw_sound_generate_frequency;
          phase_offset = 0.0;

          if (cw_is_debugging_internal (CW_DEBUG_SOUND))
            fprintf (stderr, "cw: dsp changes to %d Hz\n", current_frequency);
        }
    }

  /*
   * Now generate soundcard tone data depending on the value of the current
   * frequency as determined or reset above.
   */
  if (current_frequency != TONE_SILENT)
    {
      audio_buf_info info_buf;
      int fragment, limit, bytes;
      char buffer[1024];

      /* Get the current buffer status of the soundcard. */
      if (ioctl (cw_sound_descriptor, SNDCTL_DSP_GETOSPACE, &info_buf) == -1)
        {
          perror ("cw: ioctl SNDCTL_DSP_GETOSPACE");
          return RC_ERROR;
        }

      /*
       * We don't know the fragment size until we get here, but allocating
       * based on the query is dodgy in signal handler context, and can lead
       * to deadlocks.  To avoid that, we have to use a stack buffer, and so
       * limit the fragment size to sizeof buffer.  Most soundcards return
       * ~128 bytes, so the buffer here is comfortably large.
       */
      if (info_buf.fragsize > (int) sizeof (buffer))
        info_buf.fragsize = sizeof (buffer);

      /*
       * Put a limit on the data written on this pass.  The limit is set
       * initially to be about a second of audio, but if this is less than
       * two fragments, and if the soundcard buffer is totally empty (all
       * fragments available), then then it's increased to be at least two
       * fragments.  This is because the OSS driver optimizes operation by
       * waiting in most cases until at least two fragments contain data.
       */
      limit = cw_sound_sample_rate;
      if (limit < 2 * info_buf.fragsize)
        {
          if (info_buf.fragments == info_buf.fragstotal)
            limit = 2 * info_buf.fragsize;
        }

      /*
       * Loop for each unfilled fragment, or until the maximum number of
       * bytes has been written.
       *
       * The tone generation code below is based heavily on work by Paolo
       * Cravero <paolo@best.eu.org>.
       */
      for (fragment = 0, bytes = 0;
           fragment < info_buf.fragments && bytes < limit; fragment++)
        {
          int index;
          double offset_abs, offset_term;

          /* Create a fragment's worth of shaped wave data. */
          for (index = 0; index < info_buf.fragsize; index++)
            {
              buffer[index] = (char) (AMPLITUDE
                  * sin ((2.0 * M_PI
                          * (double) current_frequency
                          * (double) index
                          / (double) cw_sound_sample_rate) + phase_offset)
                  + 128 + phase_offset);
            }

          /* Compute the phase of the last generated sample. */
          offset_abs = (2.0 * M_PI
                        * (double) current_frequency
                        * (double) index
                        / (double) cw_sound_sample_rate) + phase_offset;

          /* Extract the normalized phase offset. */
          offset_term = offset_abs / 2.0 / M_PI;
          phase_offset = offset_abs - floor (offset_term) * 2.0 * M_PI;

          /* Write the buffer data. */
          if (write (cw_sound_descriptor,
                     buffer, info_buf.fragsize) != info_buf.fragsize)
            {
              perror ("cw: soundcard write");
              return RC_ERROR;
            }

          /* Update the count of bytes written on this call. */
          bytes += info_buf.fragsize;
        }

      if (cw_is_debugging_internal (CW_DEBUG_SOUND))
        fprintf (stderr, "cw: dsp data buffered, %d Hz, %d\n",
                 cw_sound_generate_frequency, bytes);
    }

  return RC_SUCCESS;
#endif
}


/**
 * cw_sound_soundcard_internal()
 *
 * Set up a tone on the soundcard.  The function starts the generation of
 * any non-silent tone, and tries to keep that tone going on subsequent calls.
 *
 * If the input frequency is silent, the function will turn off audio output
 * at the card, but keep on generating tone data.  This is an optimization;
 * the function is optimized for calls to either the same frequency, or to
 * silence (that is, on/off keying).  Calls that change frequency require
 * the sound wave buffer to be flushed and reloaded, and this is a somewhat
 * lengthy process.
 */
static int
cw_sound_soundcard_internal (int frequency)
{
  static int current_frequency = TONE_SILENT;  /* Note of freq on last call */
  static int current_volume = 0;               /* Note of volume last call */

  if (cw_is_debugging_internal (CW_DEBUG_SOUND))
    fprintf (stderr, "cw: dsp request %d Hz, current %d Hz, "
             "volume %d %%, current %d %%\n",
             frequency, current_frequency, generator->volume, current_volume);

  /* Look first for a change to the current sound frequency. */
  if (frequency != current_frequency)
    {
      int volume;

      /* If moving to silence, set the mixer volume to zero. */
      if (frequency == TONE_SILENT)
        {
          /* Keep background tone data generation going. */
          cw_generate_sound_internal ();

          /* Suppress sound by setting the volume to zero. */
          volume = 0;
        }
      else
        {
          /* Reset generated tone, then prod data generation. */
          cw_sound_generate_frequency = frequency;
          cw_generate_sound_internal ();

          /*
           * Set the volume according to the library variable.  Volume is
           * held as a percentage, and supplied to the card as two values,
           * one per channel, in the lower two bytes of the volume argument.
           */
          volume = generator->volume; /* cw_volume << 8 | cw_volume; */
        }

      /*
       * Set this mixer volume on the sound device, unless there was some
       * problem or another opening the soundcard file in the first place.
       */
      if (generator->sound_system == CW_AUDIO_OSS
          && !cw_set_sound_pcm_volume_internal (volume))
        return RC_ERROR;

      /* Set this new frequency and volume as our current one. */
      current_frequency = frequency;
      current_volume = generator->volume;
    }

  /* Look for changes to the main volume, but the same frequency. */
  else if (generator->volume != current_volume)
    {
      /* Keep background tone data generation going. */
      cw_generate_sound_internal ();

      /* Do nothing more if current frequency is silent. */
      if (frequency != TONE_SILENT)
        {
          int volume;

          /*
           * Reset only the volume for the soundcard, using the same setting
           * trick as we used above.
           */
          volume = generator->volume; /* cw_volume << 8 | cw_volume; */

          /*
           * Set this mixer volume on the sound device, providing the sound
           * card opened okay.
           */
          if (generator->sound_system == CW_AUDIO_OSS
              && !cw_set_sound_pcm_volume_internal (volume))
            return RC_ERROR;

          /* Set this new volume as our current one. */
          current_volume = generator->volume;
        }
    }

  else
    {
      /*
       * No change in sound frequency or volume, but keep the data generation
       * going to keep soundcard buffers from underrunning.
       */
      cw_generate_sound_internal ();
    }

  return RC_SUCCESS;
}


#if defined(KIOCSOUND)


/**
 * cw_console_open_device()
 *
 * Open the console device for Morse code tones.  Verify that the given
 * device can do KIOCSOUND ioctls before returning.
 */
static int cw_console_open_device(const char *device)
{
	if (generator->sound_system == CW_AUDIO_CONSOLE) {
		/* Ignore the call if the console device is already open. */
		return RC_SUCCESS;
	}

	int console = open(device, O_WRONLY);
	if (console == -1) {
		fprintf(stderr, "cw: open ");
		perror(cw_console_device);
		return RC_ERROR;
        }

	/* Check to see if the file can do console tones. */
	if (ioctl(console, KIOCSOUND, 0) == -1) {
		close (console);
		return RC_ERROR;
        }

	if (cw_is_debugging_internal (CW_DEBUG_SOUND)) {
		fprintf (stderr, "cw: console opened and operational\n");
	}

	/* Save the opened file descriptor in a library variable. */
	generator->audio_sink = console;

	/* Note console as now open for business. */
	generator->sound_system = CW_AUDIO_CONSOLE;

	return RC_SUCCESS;
}


#endif


/**
 * cw_console_close_device()
 *
 * Close the console device file.
 */
static int cw_console_close_device(void)
{
	close(generator->audio_sink);
	generator->audio_sink = -1;

	if (cw_is_debugging_internal(CW_DEBUG_SOUND)) {
		fprintf (stderr, "cw: console closed\n");
	}

	return RC_SUCCESS;
}


/**
 * cw_sound_console_internal()
 *
 * Set up a tone on the console PC speaker.  The function calls the KIOCSOUND
 * ioctl to start a particular tone generating in the kernel.  Once started,
 * the console tone generation needs no maintenance.
 */
static int cw_sound_console_internal(int frequency)
{
	/* TODO: do we have to re-check this here? */
#if defined(KIOCSOUND)

	/*
	 * Calculate the correct argument for KIOCSOUND.  There's nothing we
	 * can do to control the volume, but if we find the volume is set to
	 * zero, the one thing we can do is to just turn off tones.  A bit
	 * crude, but perhaps just slightly better than doing nothing.
	 */
	int argument = 0;
	if (generator->volume > 0 && frequency > 0) {
		argument = KIOCSOUND_CLOCK_TICK_RATE / frequency;
	}

	if (cw_is_debugging_internal(CW_DEBUG_SOUND)) {
		fprintf (stderr, "cw: KIOCSOUND arg = %d (%d Hz, %d %%)\n", argument, frequency, generator->volume);
	}

	/* If the console file is not open, open it now. */
	if (generator->sound_system == CW_AUDIO_NONE) {
		cw_console_open_device(cw_console_device);
	}

	/* Call the ioctl, and return any error status. */
	if (ioctl(generator->audio_sink, KIOCSOUND, argument) == -1) {
		perror("cw: ioctl KIOCSOUND");
		return RC_ERROR;
	}

	return RC_SUCCESS;
#else
	return RC_ERROR;
#endif
}


/**
 * cw_release_sound_internal()
 *
 * Release the hold that the library has on the soundcard and the console.
 * This function closes the console,  closes the soundcard, and resets
 * background tone generation so that no work is done on tone creation
 * until the next tone is requested.  This also ensures that the DSP
 * soundcard device remains closed until it is needed again.
 */
static int
cw_release_sound_internal(void)
{
#if 0
	/* Close the console if it is open. */
	if (generator->sound_system == CW_AUDIO_CONSOLE) {
		cw_close_sound_console_internal();
	}

	/* Silence the soundcard if there is current soundcard activity. */
	if (cw_sound_generate_frequency != TONE_SILENT) {
		/* Tell the background tone generation to go idle. */
		cw_sound_generate_frequency = TONE_SILENT;
		cw_generate_sound_internal();
	}
#endif
	return RC_SUCCESS;
}



/**
 * cw_sound_internal()
 *
 * Start a tone of the given frequency, on either of, or both of, the sound
 * card and the console PC speaker.  This function returns a status, but
 * routines running inside signal handlers are free to ignore this.  The
 * function does nothing if silence is requested in the library flags.
 */
static int cw_sound_internal(int frequency)
{
	int status = RC_SUCCESS;
	/* If silence requested, then ignore the call. */
	if (!(cw_is_debugging_internal(CW_DEBUG_SILENT))) {

		if (generator->sound_system == CW_AUDIO_OSS) {
			status = cw_sound_soundcard_internal(frequency);
		} else if (generator->sound_system == CW_AUDIO_CONSOLE) {
			status = cw_sound_console_internal(frequency);
		} else {
			;
		}
	}

	return status;
}


/**
 * cw_set_console_sound()
 * cw_get_console_sound()
 *
 * Enable and disable console sound output, and return the current console
 * sound output setting.  By default, console sound output is disabled.  On
 * platforms that do not support the KIOCSOUND ioctl, enabling console sound
 * output has no effect.
 */
void
cw_set_console_sound (int sound_state)
{
  if (cw_sound_console_on && !sound_state)
    {
      /* Send the console into silent mode. */
      cw_sound_console_internal (TONE_SILENT);
    }

  cw_sound_console_on = (sound_state != 0);
}

int
cw_get_console_sound (void)
{
  return cw_sound_console_on;
}


/**
 * cw_set_soundcard_sound()
 * cw_get_soundcard_sound()
 *
 * Enable and disable soundcard sound output, and return the current
 * soundcard sound output setting.  By default, soundcard sound output
 * is enabled.
 */
void
cw_set_soundcard_sound (int sound_state)
{
  if (cw_sound_soundcard_on && !sound_state)
    {
      /* Silence the soundcard, and release it altogether. */
      cw_sound_soundcard_internal (TONE_SILENT);
      cw_release_sound_internal ();
    }

  cw_sound_soundcard_on = (sound_state != 0);
}

int
cw_get_soundcard_sound (void)
{
  return cw_sound_soundcard_on;
}


/*---------------------------------------------------------------------*/
/*  Finalization and cleanup                                           */
/*---------------------------------------------------------------------*/

/*
 * We prefer to close the soundcard after a period of library inactivity,
 * so that other applications can use it.  Ten seconds seems about right.
 * We do it in one-second timeouts so that any leaked pending timeouts from
 * other facilities don't cause premature finalization.
 */
static const int FINALIZATION_DELAY = 10000000;

/* Counter counting down the number of clock calls before we finalize. */
static volatile int cw_is_finalization_pending = FALSE;
static volatile int cw_finalization_countdown = 0;

/* Use a mutex to suppress delayed finalizations on complete resets. */
static volatile int cw_is_finalization_locked_out = FALSE;

/*
 * Array of callbacks registered for convenience signal handling.  They're
 * initialized dynamically to SIG_DFL (if SIG_DFL is not NULL, which it
 * seems that it is in most cases).
 */
static void (*cw_signal_callbacks[RTSIG_MAX]) (int);


/**
 * cw_finalization_clock_internal()
 *
 * If finalization is pending, decrement the countdown, and if this reaches
 * zero, we've waited long enough to release sound and timeouts.
 */
static void
cw_finalization_clock_internal (void)
{
  if (cw_is_finalization_pending)
    {
      /* Decrement the timeout countdown, and finalize if we reach zero. */
      cw_finalization_countdown--;
      if (cw_finalization_countdown <= 0)
        {
          if (cw_is_debugging_internal (CW_DEBUG_FINALIZATION))
            fprintf (stderr, "cw: finalization timeout, closing down\n");

          cw_release_timeouts_internal ();
          cw_release_sound_internal ();

          cw_is_finalization_pending = FALSE;
          cw_finalization_countdown = 0;
        }
      else
        {
          if (cw_is_debugging_internal (CW_DEBUG_FINALIZATION))
            fprintf (stderr, "cw: finalization countdown %d\n",
                     cw_finalization_countdown);

          /* Request another timeout.  This results in a call to our
           * cw_cancel_finalization_internal below; to ensure that it doesn't
           * really cancel finalization, unset the pending flag, then set it
           * back again after reqesting the timeout.
           */
          cw_is_finalization_pending = FALSE;
          cw_request_timeout_internal (USECS_PER_SEC, NULL);
          cw_is_finalization_pending = TRUE;
        }
    }
}


/**
 * cw_schedule_finalization_internal()
 * cw_cancel_finalization_internal()
 *
 * Set the finalization pending flag, and request a timeout to call the
 * finalization function after a delay of a few seconds.  Cancel any pending
 * finalization on noting other library activity, indicated by a call from
 * the timeout request function telling us that it is setting a timeout.
 */
static void
cw_schedule_finalization_internal (void)
{
  if (!cw_is_finalization_locked_out && !cw_is_finalization_pending)
    {
      cw_request_timeout_internal (USECS_PER_SEC,
                                   cw_finalization_clock_internal);

      /*
       * Set the flag and countdown last; calling cw_request_timeout_internal
       * above results in a call to our cw_cancel_finalization, which clears
       * the flag and countdown if we set them early.
       */
      cw_is_finalization_pending = TRUE;
      cw_finalization_countdown = FINALIZATION_DELAY / USECS_PER_SEC;

      if (cw_is_debugging_internal (CW_DEBUG_FINALIZATION))
        fprintf (stderr, "cw: finalization scheduled\n");
    }
}

static void
cw_cancel_finalization_internal (void)
{
  if (cw_is_finalization_pending)
    {
      /* Cancel pending finalization and return to doing nothing. */
      cw_is_finalization_pending = FALSE;
      cw_finalization_countdown = 0;

      if (cw_is_debugging_internal (CW_DEBUG_FINALIZATION))
        fprintf (stderr, "cw: finalization canceled\n");
    }
}


/**
 * cw_complete_reset()
 *
 * Reset all library features to their default states.  Clears the tone
 * queue, receive buffers and retained state information, any current
 * keyer activity, and any straight key activity, returns to silence, and
 * closes soundcard and console devices.  This function is suitable for
 * calling from an application exit handler.
 */
void
cw_complete_reset (void)
{
  /*
   * If the finalizer thinks it's pending, stop it, then temporarily lock
   * out finalizations.
   */
  cw_cancel_finalization_internal ();
  cw_is_finalization_locked_out = TRUE;

  /* Silence sound, and shutdown use of the sound devices. */
  cw_sound_soundcard_internal (TONE_SILENT);
  cw_release_sound_internal ();
  cw_release_timeouts_internal ();

  /* Call the reset functions for each subsystem. */
  cw_reset_tone_queue ();
  cw_reset_receive ();
  cw_reset_keyer ();
  cw_reset_straight_key ();

  /* Now we can re-enable delayed finalizations. */
  cw_is_finalization_locked_out = FALSE;
}


/**
 *
 * cw_interpose_signal_handler_internal()
 *
 * Signal handler function registered when cw_register_signal_handler is
 * requested.  Resets the library, and then either calls any supplied
 * sub-handler, exits (if SIG_DFL) or continues (if SIG_IGN).
 */
static void
cw_interpose_signal_handler_internal (int signal_number)
{
  void (*callback_func) (int);

  if (cw_is_debugging_internal (CW_DEBUG_FINALIZATION))
    fprintf (stderr, "cw: caught signal %d\n", signal_number);

  /* Reset the library and retrieve the signal's handler. */
  cw_complete_reset ();
  callback_func = cw_signal_callbacks[signal_number];

  /* The default action is to stop the process; exit(1) seems to cover it. */
  if (callback_func == SIG_DFL)
    exit (EXIT_FAILURE);

  /* If we didn't exit, invoke any additional handler callback function. */
  if (callback_func != SIG_IGN)
    (*callback_func) (signal_number);
}


/**
 * cw_register_signal_handler()
 *
 * Register a signal handler and optional callback function for signals.  On
 * receipt of that signal, all library features will be reset to their default
 * states.  Following the reset, if callback_func is a function pointer, the
 * function is called; if it is SIG_DFL, the library calls exit(); and if it
 * is SIG_IGN, the library returns from the signal handler.  This is a
 * convenience function for clients that need to clean up library on signals,
 * with either exit, continue, or an additional function call; in effect, a
 * wrapper round a restricted form of sigaction.  The signal_number argument
 * indicates which signal to catch.  Returns TRUE if the signal handler
 * installs correctly, FALSE otherwise, with errno set to EINVAL if
 * signal_number is invalid or if a handler is already installed for that
 * signal, or to the sigaction error code.
 */
int
cw_register_signal_handler (int signal_number,
                            void (*callback_func) (int))
{
  static int is_initialized = FALSE;

  struct sigaction action, original_disposition;
  int status;

  /* On first call, initialize all signal_callbacks to SIG_DFL. */
  if (!is_initialized)
    {
      int index;

      for (index = 0; index < RTSIG_MAX; index++)
        cw_signal_callbacks[index] = SIG_DFL;

      is_initialized = TRUE;
    }

  /* Reject invalid signal numbers, and SIGALRM, which we use internally. */
  if (signal_number < 0 || signal_number >= RTSIG_MAX
      || signal_number == SIGALRM)
    {
      errno = EINVAL;
      return RC_ERROR;
    }

  /* Install our handler as the actual handler. */
  action.sa_handler = cw_interpose_signal_handler_internal;
  action.sa_flags = SA_RESTART;
  sigemptyset (&action.sa_mask);
  status = sigaction (signal_number, &action, &original_disposition);
  if (status == -1)
    {
      perror ("cw: sigaction");
      return RC_ERROR;
    }

  /* If we trampled another handler, replace it and return FALSE. */
  if (!(original_disposition.sa_handler == cw_interpose_signal_handler_internal
      || original_disposition.sa_handler == SIG_DFL
      || original_disposition.sa_handler == SIG_IGN))
    {
      status = sigaction (signal_number, &original_disposition, NULL);
      if (status == -1)
        {
          perror ("cw: sigaction");
          return RC_ERROR;
        }

      errno = EINVAL;
      return RC_ERROR;
    }

  /* Save the callback function (it may validly be SIG_DFL or SIG_IGN). */
  cw_signal_callbacks[signal_number] = callback_func;
  return RC_SUCCESS;
}


/**
 * cw_unregister_signal_handler()
 *
 * Removes a signal handler interception previously registered with
 * cw_register_signal_handler.  Returns TRUE if the signal handler uninstalls
 * correctly, FALSE otherwise, with errno set to EINVAL or to the sigaction
 * error code.
 */
int
cw_unregister_signal_handler (int signal_number)
{
  struct sigaction action, original_disposition;
  int status;

  /* As above, reject unacceptable signal numbers. */
  if (signal_number < 0 || signal_number >= RTSIG_MAX
      || signal_number == SIGALRM)
    {
      errno = EINVAL;
      return RC_ERROR;
    }

  /* See if the current handler was put there by us. */
  status = sigaction (signal_number, NULL, &original_disposition);
  if (status == -1)
    {
      perror ("cw: sigaction");
      return RC_ERROR;
    }
  if (original_disposition.sa_handler != cw_interpose_signal_handler_internal)
    {
      errno = EINVAL;
      return RC_ERROR;
    }

  /* Remove the signal handler by resetting to SIG_DFL. */
  action.sa_handler = SIG_DFL;
  action.sa_flags = 0;
  sigemptyset (&action.sa_mask);
  status = sigaction (signal_number, &action, NULL);
  if (status == -1)
    {
      perror ("cw: sigaction");
      return RC_ERROR;
    }

  /* Reset the callback entry for tidiness. */
  cw_signal_callbacks[signal_number] = SIG_DFL;
  return RC_SUCCESS;
}


/*---------------------------------------------------------------------*/
/*  Keying control                                                     */
/*---------------------------------------------------------------------*/

/*
 * External keying function.  It may be useful for a client to have this
 * library control an external keying device, for example, an oscillator,
 * or a transmitter.  Here is where we keep the address of a function that
 * is passed to us for this purpose, and a void* argument for it.
 */
static void (*cw_kk_key_callback) (void*, int) = NULL;
static void *cw_kk_key_callback_arg = NULL;


/**
 * cw_register_keying_callback()
 *
 * Register a function that should be called when a tone state changes from
 * key-up to key-down, or vice-versa.  The first argument passed out to the
 * registered function is the supplied callback_arg, if any.  The second
 * argument passed out is the key state: TRUE for down, FALSE for up.  Calling
 * this routine with an NULL function address disables keying callbacks.  Any
 * callback supplied will be called in signal handler context.
 */
void
cw_register_keying_callback (void (*callback_func) (void*, int),
                             void *callback_arg)
{
  cw_kk_key_callback = callback_func;
  cw_kk_key_callback_arg = callback_arg;
}


/**
 * cw_key_control_internal()
 *
 * Control function that calls any requested keying callback only when there
 * is a change of keying state.  This function filters successive key-down
 * or key-up actions into a single action.
 */
static void
cw_key_control_internal (int requested_key_state)
{
  static int current_key_state = FALSE;  /* Maintained key control state */

  if (current_key_state != requested_key_state)
    {
      if (cw_is_debugging_internal (CW_DEBUG_KEYING))
        fprintf (stderr, "cw: keying state %d->%d\n",
                 current_key_state, requested_key_state);

      /* Set the new keying state, and call any requested callback. */
      current_key_state = requested_key_state;
      if (cw_kk_key_callback)
        (*cw_kk_key_callback) (cw_kk_key_callback_arg, current_key_state);
    }
}


/*---------------------------------------------------------------------*/
/*  Tone queue                                                         */
/*---------------------------------------------------------------------*/

/*
 * Tone queue.  This is a circular list of tone durations and frequencies
 * pending, and a pair of indexes, tail (enqueue) and head (dequeue) to
 * manage additions and asynchronous sending.
 */
enum
{ TONE_QUEUE_CAPACITY = 3000,        /* ~= 5 minutes at 12 WPM */
  TONE_QUEUE_HIGH_WATER_MARK = 2900  /* Refuse characters if <100 free */
};
typedef struct
{
  int usecs;      /* Tone duration in usecs */
  int frequency;  /* Frequency of the tone */
} cw_queued_tone_t;

static volatile cw_queued_tone_t cw_tone_queue[TONE_QUEUE_CAPACITY];
static volatile int cw_tq_tail = 0,  /* Tone queue tail index */
                    cw_tq_head = 0;  /* Tone queue head index */

/*
 * It's useful to have the tone queue dequeue function call a client-supplied
 * callback routine when the amount of data in the queue drops below a
 * defined low water mark.  This routine can then refill the buffer, as
 * required.
 */
static volatile int cw_tq_low_water_mark = 0;
static void (*cw_tq_low_water_callback) (void*) = NULL;
static void *cw_tq_low_water_callback_arg = NULL;


/**
 * cw_get_tone_queue_length_internal
 * cw_next_tone_queue_index_internal
 *
 * Return the count of tones currently held in the circular tone buffer, and
 * advance a tone queue index, including circular wrapping.
 */
static int
cw_get_tone_queue_length_internal (void)
{
  return cw_tq_tail >= cw_tq_head
         ? cw_tq_tail - cw_tq_head
         : cw_tq_tail - cw_tq_head + TONE_QUEUE_CAPACITY;
}

static int
cw_next_tone_queue_index_internal (int current)
{
  return (current + 1) % TONE_QUEUE_CAPACITY;
}


/*
 * The CW tone queue functions implement the following state graph:
 *
 *          (queue empty)
 *        +-------------------------------+
 *        |                               |
 *        v    (queue started)            |
 * --> QS_IDLE ---------------> QS_BUSY --+
 *                              ^     |
 *                              |     |
 *                              +-----+
 *                          (queue not empty)
 */
static volatile enum { QS_IDLE, QS_BUSY } cw_dequeue_state = QS_IDLE;


/**
 * cw_tone_queue_clock_internal()
 *
 * Signal handler for itimer.  Dequeue a tone request, and send the ioctl to
 * generate the tone.  If the queue is empty when we get the signal, then
 * we're at the end of the work list, so set the dequeue state to idle and
 * return.
 */
static void
cw_tone_queue_clock_internal (void)
{
  /* Decide what to do based on the current state. */
  switch (cw_dequeue_state)
    {
    /* Ignore calls if our state is idle. */
    case QS_IDLE:
      return;

    /*
     * If busy, dequeue the next tone, or if no more tones, go to the idle
     * state.
     */
    case QS_BUSY:
      if (cw_tq_head != cw_tq_tail)
        {
          int usecs, frequency, queue_length;

          /*
           * Get the current queue length.  Later on, we'll compare with the
           * length after we've scanned over every tone we can omit, and use it
           * to see if we've crossed the low water mark, if any.
           */
          queue_length = cw_get_tone_queue_length_internal ();

          /*
           * Advance over the tones list until we find the first tone with a
           * duration of more than zero usecs, or until the end of the list.
           */
          do
            {
              cw_tq_head = cw_next_tone_queue_index_internal (cw_tq_head);
            }
          while (cw_tq_head != cw_tq_tail
                 && cw_tone_queue[cw_tq_head].usecs == 0);

          /* Dequeue the next tone to send. */
          usecs = cw_tone_queue[cw_tq_head].usecs;
          frequency = cw_tone_queue[cw_tq_head].frequency;

          if (cw_is_debugging_internal (CW_DEBUG_TONE_QUEUE))
            fprintf (stderr,
                     "cw: dequeue tone %d usec, %d Hz\n", usecs, frequency);

          /*
           * Start the tone.  If the ioctl fails, there's nothing we can do at
           * this point, in the way of returning error codes.
           */
          cw_sound_internal (frequency);

          /*
           * Notify the key control function that there might have been a
           * change of keying state (and then again, there might not have
           * been -- it will sort this out for us).
           */
          cw_key_control_internal (frequency != TONE_SILENT);

          /*
           * If microseconds is zero, leave it at that.  This way, a queued
           * tone of 0 usec implies leaving the sound in this state, and 0
           * usec and 0 frequency leaves silence.
           */
          if (usecs > 0)
            {
              /*
               * Request a timeout.  If it fails, there's little we can do at
               * this point.  But it shouldn't fail.
               */
              cw_request_timeout_internal (usecs, NULL);
            }
          else
            {
              /* Autonomous dequeuing has finished for the moment. */
              cw_dequeue_state = QS_IDLE;
              cw_schedule_finalization_internal ();
            }

          /*
           * If there is a low water mark callback registered, and if we passed
           * under the water mark, call the callback here.  We want to be sure
           * to call this late in the processing, especially after setting the
           * state to idle, since the most likely action of this routine is to
           * queue tones, and we don't want to play with the state here after
           * that.
           */
          if (cw_tq_low_water_callback)
            {
              /*
               * If the length we originally calculated was above the low water
               * mark, and the one we have now is below or equal to it, call
               * the callback.
               */
              if (queue_length > cw_tq_low_water_mark
                  && cw_get_tone_queue_length_internal ()
                     <= cw_tq_low_water_mark)
                (*cw_tq_low_water_callback) (cw_tq_low_water_callback_arg);
            }
        }
      else
        {
          /*
           * This is the end of the last tone on the queue, and since we got a
           * signal we know that it had a usec greater than zero.  So this is
           * the time to return to silence.
           */
          cw_sound_internal (TONE_SILENT);

          /* Notify the keying control function, as above. */
          cw_key_control_internal (FALSE);

          /*
           * Set state to idle, indicating that autonomous dequeueing has
           * finished for the moment.  We need this set whenever the queue
           * indexes are equal and there is no pending itimeout.
           */
          cw_dequeue_state = QS_IDLE;
          cw_schedule_finalization_internal ();
        }
    }
}


/**
 * cw_enqueue_tone_internal()
 *
 * Enqueue a tone for specified frequency and number of microseconds.  This
 * routine adds the new tone to the queue, and if necessary starts the
 * itimer process to have the tone sent.  The routine returns TRUE on success.
 * If the tone queue is full, the routine returns FALSE, with errno set to
 * EAGAIN.  If the iambic keyer or straight key are currently busy, the
 * routine returns FALSE, with errno set to EBUSY.
 */
static int
cw_enqueue_tone_internal (int usecs, int frequency)
{
  int new_tq_tail;

  /*
   * If the keyer or straight key are busy, return an error.  This is because
   * they use the sound card/console tones and key control, and will interfere
   * with us if we try to use them at the same time.
   */
  if (cw_is_keyer_busy () || cw_is_straight_key_busy ())
    {
      errno = EBUSY;
      return RC_ERROR;
    }

  /* Get the new value of the queue tail index. */
  new_tq_tail = cw_next_tone_queue_index_internal (cw_tq_tail);

  /*
   * If the new value is bumping against the head index, then the queue
   * is currently full, so return EAGAIN.
   */
  if (new_tq_tail == cw_tq_head)
    {
      errno = EAGAIN;
      return RC_ERROR;
    }

  if (cw_is_debugging_internal (CW_DEBUG_TONE_QUEUE))
    fprintf (stderr, "cw: enqueue tone %d usec, %d Hz\n", usecs, frequency);

  /* Set the new tail index, and enqueue the new tone. */
  cw_tq_tail = new_tq_tail;
  cw_tone_queue[cw_tq_tail].usecs = usecs;
  cw_tone_queue[cw_tq_tail].frequency = frequency;

  /*
   * If there is currently no autonomous dequeue happening, kick off the
   * itimer process.
   */
  if (cw_dequeue_state == QS_IDLE)
    {
      cw_dequeue_state = QS_BUSY;
      cw_request_timeout_internal (0, cw_tone_queue_clock_internal);
    }

  return RC_SUCCESS;
}


/**
 * cw_register_tone_queue_low_callback()
 *
 * Registers a function to be called automatically by the dequeue routine
 * whenever the tone queue falls to a given level; callback_arg may be used
 * to give a value passed back on callback calls.  A NULL function pointer
 * suppresses callbacks.  On success, the routine returns TRUE.  If level is
 * invalid, the routine returns FALSE with errno set to EINVAL.  Any callback
 * supplied will be called in signal handler context.
 */
int
cw_register_tone_queue_low_callback (void (*callback_func) (void*),
                                     void *callback_arg, int level)
{
  if (level < 0 || level >= TONE_QUEUE_CAPACITY - 1)
    {
      errno = EINVAL;
      return RC_ERROR;
    }

  /* Store the function and low water mark level. */
  cw_tq_low_water_mark = level;
  cw_tq_low_water_callback = callback_func;
  cw_tq_low_water_callback_arg = callback_arg;

  return RC_SUCCESS;
}


/**
 * cw_is_tone_busy()
 *
 * Indicates if the tone sender is busy; returns TRUE if there are still
 * entries in the tone queue, FALSE if the queue is empty.
 */
int
cw_is_tone_busy (void)
{
  return cw_dequeue_state != QS_IDLE;
}


/**
 * cw_wait_for_tone()
 *
 * Wait for the current tone to complete.  The routine returns TRUE on
 * success.  If called with SIGALRM blocked, the routine returns FALSE, with
 * errno set to EDEADLK, to avoid indefinite waits.
 */
int
cw_wait_for_tone (void)
{
  int status, check_tq_head;

  /* Check that SIGALRM is not blocked. */
  status = cw_check_signal_mask_internal ();
  if (!status)
    return RC_ERROR;

  /* Wait for the tail index to change or the dequeue to go idle. */
  check_tq_head = cw_tq_head;
  while (cw_tq_head == check_tq_head && cw_dequeue_state != QS_IDLE)
    cw_wait_for_signal_internal ();

  return RC_SUCCESS;
}


/**
 * cw_wait_for_tone_queue()
 *
 * Wait for the tone queue to drain.  The routine returns TRUE on success.
 * If called with SIGALRM blocked, the routine returns FALSE, with errno set
 * to EDEADLK, to avoid indefinite waits.
 */
int
cw_wait_for_tone_queue (void)
{
  int status;

  /* Check that SIGALRM is not blocked. */
  status = cw_check_signal_mask_internal ();
  if (!status)
    return RC_ERROR;

  /* Wait until the dequeue indicates it's hit the end of the queue. */
  while (cw_dequeue_state != QS_IDLE)
    cw_wait_for_signal_internal ();

  return RC_SUCCESS;
}


/**
 * cw_wait_for_tone_queue_critical()
 *
 * Wait for the tone queue to drain until only as many tones as given
 * in level remain queued.  This routine is for use by programs that want
 * to optimize themselves to avoid the cleanup that happens when the tone
 * queue drains completely; such programs have a short time in which to
 * add more tones to the queue.  The routine returns TRUE on success.  If
 * called with SIGALRM blocked, the routine returns FALSE, with errno set to
 * EDEADLK, to avoid indefinite waits.
 */
int
cw_wait_for_tone_queue_critical (int level)
{
  int status;

  /* Check that SIGALRM is not blocked. */
  status = cw_check_signal_mask_internal ();
  if (!status)
    return RC_ERROR;

  /* Wait until the queue length is at or below criticality. */
  while (cw_get_tone_queue_length_internal () > level)
    cw_wait_for_signal_internal ();

  return RC_SUCCESS;
}


/**
 * cw_is_tone_queue_full()
 *
 * Indicates if the tone queue is full, returning TRUE if full, FALSE if not.
 */
int
cw_is_tone_queue_full (void)
{
  /* If advancing would meet the tail index, return TRUE. */
  return cw_next_tone_queue_index_internal (cw_tq_tail) == cw_tq_head;
}


/**
 * cw_get_tone_queue_capacity()
 *
 * Returns the number of entries the tone queue can accommodate.
 */
int
cw_get_tone_queue_capacity (void)
{
  /*
   * Since the head and tail indexes cannot be equal, the perceived capacity
   * for the client is always one less than the actual declared queue size.
   */
  return TONE_QUEUE_CAPACITY - 1;
}


/**
 * cw_get_tone_queue_length()
 *
 * Returns the number of entries currently pending in the tone queue.
 */
int
cw_get_tone_queue_length (void)
{
  return cw_get_tone_queue_length_internal ();
}


/**
 * cw_flush_tone_queue()
 *
 * Cancel all pending queued tones, and return to silence.  If there is a
 * tone in progress, the function will wait until this last one has
 * completed, then silence the tones.
 *
 * This function may be called with SIGALRM blocked, in which case it
 * will empty the queue as best it can, then return without waiting for
 * the final tone to complete.  In this case, it may not be possible to
 * guarantee silence after the call.
 */
void
cw_flush_tone_queue (void)
{
  /* Empty the queue, by setting the head to the tail. */
  cw_tq_head = cw_tq_tail;

  /* If we can, wait until the dequeue goes idle. */
  if (cw_check_signal_mask_internal ())
    cw_wait_for_tone_queue ();

  /*
   * Force silence on the speaker anyway, and stop any background soundcard
   * tone generation.
   */
  cw_sound_internal (TONE_SILENT);
  cw_schedule_finalization_internal ();
}


/**
 * cw_queue_tone()
 *
 * Provides primitive access to simple tone generation.  This routine queues
 * a tone of given duration and frequency.  The routine returns TRUE on
 * success.  If usec or frequency are invalid, it returns FALSE with errno
 * set to EINVAL.  If the sound card, console speaker, or keying function are
 * busy, it returns FALSE with errno set to EBUSY.  If the tone queue is full,
 * it returns FALSE with errno set to EAGAIN.
 */
int
cw_queue_tone (int usecs, int frequency)
{
  /*
   * Check the arguments given for realistic values.  Note that we do nothing
   * here to protect the caller from setting up neverending (0 usecs) tones,
   * if that's what they want to do.
   */
  if (usecs < 0 || frequency < 0
      || frequency < CW_MIN_FREQUENCY || frequency > CW_MAX_FREQUENCY)
    {
      errno = EINVAL;
      return RC_ERROR;
    }

  return cw_enqueue_tone_internal (usecs, frequency);
}


/**
 * cw_reset_tone_queue()
 *
 * Cancel all pending queued tones, reset any queue low callback registered,
 * and return to silence.  This function is suitable for calling from an
 * application exit handler.
 */
void
cw_reset_tone_queue (void)
{
  /* Empty the queue, and force state to idle. */
  cw_tq_head = cw_tq_tail;
  cw_dequeue_state = QS_IDLE;

  /* Reset low water mark details to their initial values. */
  cw_tq_low_water_mark = 0;
  cw_tq_low_water_callback = NULL;
  cw_tq_low_water_callback_arg = NULL;

  /* Silence sound and stop any background soundcard tone generation. */
  cw_sound_internal (TONE_SILENT);
  cw_schedule_finalization_internal ();

  if (cw_is_debugging_internal (CW_DEBUG_TONE_QUEUE))
    fprintf (stderr, "cw: tone queue reset\n");
}


/*---------------------------------------------------------------------*/
/*  Sending                                                            */
/*---------------------------------------------------------------------*/

/**
 * cw_send_element_internal()
 *
 * Low level primitive to send a tone element of the given type, followed
 * by the standard inter-element silence.
 */
static int
cw_send_element_internal (char element)
{
  int status;

  /* Synchronize low-level timings if required. */
  cw_sync_parameters_internal ();

  /* Send either a dot or a dash element, depending on representation. */
  if (element == CW_DOT_REPRESENTATION)
    status = cw_enqueue_tone_internal (cw_send_dot_length, generator->frequency);
  else if (element == CW_DASH_REPRESENTATION)
    status = cw_enqueue_tone_internal (cw_send_dash_length, generator->frequency);
  else
    {
      errno = EINVAL;
      status = RC_ERROR;
    }
  if (!status)
    return RC_ERROR;

  /* Send the inter-element gap. */
  status = cw_enqueue_tone_internal (cw_end_of_ele_delay, TONE_SILENT);
  if (!status)
    return RC_ERROR;

  return RC_SUCCESS;
}


/**
 * cw_send_[dot|dash|character_space|word_space]()
 *
 * Low level primitives, available to send single dots, dashes, character
 * spaces, and word spaces.  The dot and dash routines always append the
 * normal inter-element gap after the tone sent.  The cw_send_character_space
 * routine sends space timed to exclude the expected prior dot/dash
 * inter-element gap.  The cw_send_word_space routine sends space timed to
 * exclude both the expected prior dot/dash inter-element gap and the prior
 * end of character space.  These functions return TRUE on success, or FALSE
 * with errno set to EBUSY or EAGAIN on error.
 */
int
cw_send_dot (void)
{
  return cw_send_element_internal (CW_DOT_REPRESENTATION);
}

int
cw_send_dash (void)
{
  return cw_send_element_internal (CW_DASH_REPRESENTATION);
}

int
cw_send_character_space (void)
{
  /* Synchronize low-level timing parameters. */
  cw_sync_parameters_internal ();

  /*
   * Delay for the standard end of character period, plus any additional
   * inter-character gap
   */
  return cw_enqueue_tone_internal (cw_end_of_char_delay + cw_additional_delay,
                                   TONE_SILENT);
}

int
cw_send_word_space (void)
{
  /* Synchronize low-level timing parameters. */
  cw_sync_parameters_internal ();

  /*
   * Send silence for the word delay period, plus any adjustment that may be
   * needed at end of word.
   */
  return cw_enqueue_tone_internal (cw_end_of_word_delay + cw_adjustment_delay,
                                   TONE_SILENT);
}


/**
 * cw_send_representation_internal()
 *
 * Send the given string as dots and dashes, adding the post-character
 * gap.
 */
static int
cw_send_representation_internal (const char *representation, int partial)
{
  int index;

  /*
   * Before we let this representation loose on tone generation, we'd really
   * like to know that all of its tones will get queued up successfully.  The
   * right way to do this is to calculate the number of tones in our
   * representation, then check that the space exists in the tone queue.
   * However, since the queue is comfortably long, we can get away with just
   * looking for a high water mark.
   */
  if (cw_get_tone_queue_length () >= TONE_QUEUE_HIGH_WATER_MARK)
    {
      errno = EAGAIN;
      return RC_ERROR;
    }

  /* Sound the elements of the CW equivalent. */
  for (index = 0; representation[index] != '\0'; index++)
    {
      int status;

      /*
       * Send a tone of dot or dash length, followed by the normal, standard,
       * inter-element gap.
       */
      status = cw_send_element_internal (representation[index]);
      if (!status)
        return RC_ERROR;
    }

  /*
   * If this representation is stated as being 'partial', then suppress any
   * and all end of character delays.
   */
  if (!partial)
    {
      int status;

      status = cw_send_character_space ();
      if (!status)
        return RC_ERROR;
    }

  return RC_SUCCESS;
}


/**
 * cw_send_representation()
 *
 * Checks, then sends the given string as dots and dashes.  The representation
 * passed in is assumed to be a complete Morse character; that is, all post-
 * character delays will be added when the character is sent.  On success,
 * the routine returns TRUE.  On error, it returns FALSE, with errno set to
 * EINVAL if any character of the representation is invalid, EBUSY if the
 * sound card, console speaker, or keying system is busy, or EAGAIN if the
 * tone queue is full, or if there is insufficient space to queue the tones
 * for the representation.
 */
int
cw_send_representation (const char *representation)
{
  if (!cw_check_representation (representation))
    {
      errno = EINVAL;
      return RC_ERROR;
    }

  return cw_send_representation_internal (representation, FALSE);
}


/**
 * cw_send_representation_partial()
 *
 * Check, then send the given string as dots and dashes.  The representation
 * passed in is assumed to be only part of a larger Morse representation;
 * that is, no post-character delays will be added when the character is sent.
 * On success, the routine returns TRUE.  On error, it returns FALSE, with
 * errno set to EINVAL if any character of the representation is invalid,
 * EBUSY if the sound card, console speaker, or keying system is busy, or
 * EAGAIN if the tone queue is full, or if there is insufficient space to
 * queue the tones for the representation.
 */
int
cw_send_representation_partial (const char *representation)
{
  if (!cw_check_representation (representation))
    {
      errno = ENOENT;
      return RC_ERROR;
    }

  return cw_send_representation_internal (representation, TRUE);
}


/**
 * cw_send_character_internal()
 *
 * Lookup, and send a given ASCII character as cw.  If 'partial' is set, the
 * end of character delay is not appended to the Morse sent. On success,
 * the routine returns TRUE, otherwise it returns an error.
 */
static int
cw_send_character_internal (char c, int partial)
{
  int status;
  const char *representation;

  /* Handle space special case; delay end-of-word and return. */
  if (c == ' ')
    return cw_send_word_space ();

  /* Lookup the character, and sound it. */
  representation = cw_lookup_character_internal (c);
  if (!representation)
    {
      errno = ENOENT;
      return RC_ERROR;
    }

  status = cw_send_representation_internal (representation, partial);
  if (!status)
    return RC_ERROR;

  return RC_SUCCESS;
}


/**
 * cw_check_character()
 *
 * Checks that the given character is validly sendable in Morse.  If it is,
 * the routine returns TRUE.  If not, the routine returns FALSE, with errno
 * set to ENOENT.
 */
int
cw_check_character (char c)
{
  /*
   * If the character is the space special-case, or if not, but it is in the
   * lookup table, return success.
   */
  if (c == ' ' || cw_lookup_character_internal (c))
    return RC_SUCCESS;

  errno = ENOENT;
  return RC_ERROR;
}


/**
 * cw_send_character()
 *
 * Lookup, and send a given ASCII character as Morse.  The end of character
 * delay is appended to the Morse sent. On success, the routine returns TRUE.
 * On error, it returns FALSE, with errno set to ENOENT if the given character
 * is not a valid Morse character, EBUSY if the sound card, console speaker,
 * or keying system is busy, or EAGAIN if the tone queue is full, or if
 * there is insufficient space to queue the tones for the representation.
 *
 * This routine returns as soon as the character has been successfully
 * queued for send; that is, almost immediately.  The actual sending happens
 * in background processing.  See cw_wait_for_tone and cw_wait_for_tone_queue
 * for ways to check the progress of sending.
 */
int
cw_send_character (char c)
{
  if (!cw_check_character (c))
    {
      errno = ENOENT;
      return RC_ERROR;
    }

  return cw_send_character_internal (c, FALSE);
}


/**
 * cw_send_character_partial()
 *
 * Lookup, and send a given ASCII character as Morse.  The end of character
 * delay is not appended to the Morse sent by the function, to support the
 * formation of combination characters. On success, the routine returns TRUE.
 * On error, it returns FALSE, with errno set to ENOENT if the given character
 * is not a valid Morse character, EBUSY if the sound card, console speaker,
 * or keying system is busy, or EAGAIN if the tone queue is full, or if
 * there is insufficient space to queue the tones for the representation.
 *
 * This routine queues its arguments for background processing.  See
 * cw_send_character for details of how to check the queue status.
 */
int
cw_send_character_partial (char c)
{
  if (!cw_check_character (c))
    {
      errno = ENOENT;
      return RC_ERROR;
    }

  return cw_send_character_internal (c, TRUE);
}


/**
 * cw_check_string()
 *
 * Checks that each character in the given string is validly sendable in Morse.
 * On success, the routine returns TRUE.  On error, it returns FALSE, with
 * errno set to EINVAL.
 */
int
cw_check_string (const char *string)
{
  int index;

  /*
   * Check that each character in the string has a Morse representation, or
   * is the space special case.
   */
  for (index = 0; string[index] != '\0'; index++)
    {
      if (!(string[index] == ' '
            || cw_lookup_character_internal (string[index])))
        {
          errno = EINVAL;
          return RC_ERROR;
        }
    }

  return RC_SUCCESS;
}


/**
 * cw_send_string()
 *
 * Send a given ASCII string as cw.  On success, the routine returns TRUE.
 * On error, it returns FALSE, with errno set to ENOENT if any character in
 * the string is not a valid Morse character, EBUSY if the sound card, console
 * speaker, or keying system is in use by the iambic keyer or the straight
 * key, or EAGAIN if the tone queue is full.  If the tone queue runs out
 * of space part way through queueing the string, the function returns EAGAIN.
 * However, an indeterminate number of the characters from the string will
 * have already been queued.  For safety, clients can ensure the tone queue
 * is empty before queueing a string, or use cw_send_character() if they
 * need finer control.
 *
 * This routine queues its arguments for background processing.  See
 * cw_send_character for details of how to check the queue status.
 */
int
cw_send_string (const char *string)
{
  int index;

  /* Check the string is composed of sendable characters. */
  if (!cw_check_string (string))
    {
      errno = ENOENT;
      return RC_ERROR;
    }

  /* Send every character in the string. */
  for (index = 0; string[index] != '\0'; index++)
    {
      int status;

      status = cw_send_character_internal (string[index], FALSE);
      if (!status)
        return RC_ERROR;
    }

  return RC_SUCCESS;
}


/*---------------------------------------------------------------------*/
/*  Receive tracking and statistics helpers                            */
/*---------------------------------------------------------------------*/

/*
 * Receive adaptive speed tracking.  A moving averages structure, comprising
 * a small array of element lengths, a circular index into the array, and a
 * a running sum of elements for efficient calculation of moving averages.
 */
enum { AVERAGE_ARRAY_LENGTH = 4 };
typedef struct {
  int buffer[AVERAGE_ARRAY_LENGTH];  /* Buffered element lengths */
  int cursor;                        /* Circular buffer cursor */
  int sum;                           /* Running sum */
} cw_tracking_t;

static cw_tracking_t cw_dot_tracking = { {0}, 0, 0 },
                     cw_dash_tracking = { {0}, 0, 0 };


/**
 * cw_reset_adaptive_average_internal()
 * cw_update_adaptive_average_internal()
 * cw_get_adaptive_average_internal()
 *
 * Moving average functions for smoothed tracking of dot and dash lengths.
 */
static void
cw_reset_adaptive_average_internal (cw_tracking_t *tracking, int initial)
{
  int element;

  for (element = 0; element < AVERAGE_ARRAY_LENGTH; element++)
    tracking->buffer[element] = initial;
  tracking->sum = initial * AVERAGE_ARRAY_LENGTH;
  tracking->cursor = 0;
}

static void
cw_update_adaptive_average_internal (cw_tracking_t *tracking, int element_usec)
{
  tracking->sum += element_usec - tracking->buffer[tracking->cursor];
  tracking->buffer[tracking->cursor++] = element_usec;
  tracking->cursor %= AVERAGE_ARRAY_LENGTH;
}

static int
cw_get_adaptive_average_internal (cw_tracking_t *tracking)
{
  return tracking->sum / AVERAGE_ARRAY_LENGTH;
}


/*
 * Receive timing statistics.  A circular buffer of entries indicating the
 * difference between the actual and the ideal timing for a receive element,
 * tagged with the type of statistic held, and a circular buffer pointer.
 * STAT_NONE must be zero so that the statistics buffer is initially empty.
 */
typedef enum
{ STAT_NONE = 0, STAT_DOT, STAT_DASH, STAT_END_ELEMENT, STAT_END_CHARACTER
} stat_type_t;

typedef struct {
  stat_type_t type;  /* Record type */
  int delta;         /* Difference between actual and ideal timing */
} cw_statistics_t;

enum { STATISTICS_ARRAY_LENGTH = 256 };
static cw_statistics_t
  cw_receive_statistics[STATISTICS_ARRAY_LENGTH] = { {0, 0} };
static int cw_statistics_cursor = 0;


/**
 * cw_add_receive_statistic_internal()
 *
 * Add an element timing with a given statistic type to the circular
 * statistics buffer.  The buffer stores only the delta from the ideal value;
 * the ideal is inferred from the type passed in.
 */
static void
cw_add_receive_statistic_internal (stat_type_t type, int usecs)
{
  int delta;

  /* Synchronize low-level timings if required. */
  cw_sync_parameters_internal ();

  /* Calculate delta as difference between usec and the ideal value. */
  delta = usecs - ((type == STAT_DOT) ? cw_receive_dot_length
                 : (type == STAT_DASH) ? cw_receive_dash_length
                 : (type == STAT_END_ELEMENT) ? cw_eoe_range_ideal
                 : (type == STAT_END_CHARACTER) ? cw_eoc_range_ideal : usecs);

  /* Add this statistic to the buffer. */
  cw_receive_statistics[cw_statistics_cursor].type = type;
  cw_receive_statistics[cw_statistics_cursor++].delta = delta;
  cw_statistics_cursor %= STATISTICS_ARRAY_LENGTH;
}


/**
 * cw_get_receive_statistic_internal()
 *
 * Calculate and return one given timing statistic type.  If no records of
 * that type were found, return 0.0.
 */
static double
cw_get_receive_statistic_internal (stat_type_t type)
{
  double sum_of_squares;
  int count, cursor;

  /*
   * Sum and count elements matching the given type.  A cleared buffer always
   * begins refilling at element zero, so to optimize we can stop on the first
   * unoccupied slot in the circular buffer.
   */
  sum_of_squares = 0.0;
  count = 0;
  for (cursor = 0; cursor < STATISTICS_ARRAY_LENGTH; cursor++)
    {
      if (cw_receive_statistics[cursor].type == type)
        {
          int delta;

          delta = cw_receive_statistics[cursor].delta;
          sum_of_squares += (double) delta * (double) delta;
          count++;
        }
      else if (cw_receive_statistics[cursor].type == STAT_NONE)
        break;
    }

  /* Return the standard deviation, or zero if no matching elements. */
  return count > 0 ? sqrt (sum_of_squares / (double) count) : 0.0;
}


/**
 * cw_get_receive_statistics()
 *
 * Calculate and return receive timing statistics.  These statistics may be
 * used to obtain a measure of the accuracy of received CW.  The values
 * dot_sd and dash_sd contain the standard deviation of dot and dash lengths
 * from the ideal values, and element_end_sd and character_end_sd the
 * deviations for inter element and inter character spacing.  Statistics are
 * held for all timings in a 256 element circular buffer.  If any statistic
 * cannot be calculated, because no records for it exist, the returned value
 * is 0.0.  Use NULL for the pointer argument to any statistic not required.
 */
void
cw_get_receive_statistics (double *dot_sd, double *dash_sd,
                           double *element_end_sd, double *character_end_sd)
{
  if (dot_sd)
    *dot_sd = cw_get_receive_statistic_internal (STAT_DOT);
  if (dash_sd)
    *dash_sd = cw_get_receive_statistic_internal (STAT_DASH);
  if (element_end_sd)
    *element_end_sd = cw_get_receive_statistic_internal (STAT_END_ELEMENT);
  if (character_end_sd)
    *character_end_sd = cw_get_receive_statistic_internal (STAT_END_CHARACTER);
}


/**
 * cw_reset_receive_statistics()
 *
 * Clear the receive statistics buffer, removing all records from it and
 * returning it to its initial default state.
 */
void
cw_reset_receive_statistics (void)
{
  int cursor;

  for (cursor = 0; cursor < STATISTICS_ARRAY_LENGTH; cursor++)
    {
      cw_receive_statistics[cursor].type = STAT_NONE;
      cw_receive_statistics[cursor].delta = 0;
    }
  cw_statistics_cursor = 0;
}


/*---------------------------------------------------------------------*/
/*  Receiving                                                          */
/*---------------------------------------------------------------------*/

/*
 * Receive buffering.  This is a fixed-length representation, filled in
 * as tone on/off timings are taken.  The buffer is vastly longer than
 * any practical representation, and along with it we maintain a cursor
 * indicating the current write position.
 */
enum { RECEIVE_CAPACITY = 256 };
static char cw_receive_representation_buffer[RECEIVE_CAPACITY];
static int cw_rr_current = 0;

/* Retained tone start and end timestamps. */
static struct timeval cw_rr_start_timestamp = {0, 0},
                      cw_rr_end_timestamp = {0, 0};

/**
 * cw_set_adaptive_receive_internal()
 *
 * Set the value of the flag that controls whether, on receive, the receive
 * functions do fixed speed receive, or track the speed of the received Morse
 * code by adapting to the input stream.
 */
static void
cw_set_adaptive_receive_internal (int flag)
{
  /* Look for change of adaptive receive state. */
  if ((cw_is_adaptive_receive_enabled && !flag)
      || (!cw_is_adaptive_receive_enabled && flag))
    {
      cw_is_adaptive_receive_enabled = flag;

      /* Changing the flag forces a change in low-level parameters. */
      cw_is_in_sync = FALSE;
      cw_sync_parameters_internal ();

      /*
       * If we have just switched to adaptive mode, (re-)initialize the
       * averages array to the current dot/dash lengths, so that initial
       * averages match the current speed.
       */
      if (cw_is_adaptive_receive_enabled)
        {
          cw_reset_adaptive_average_internal (&cw_dot_tracking,
                                              cw_receive_dot_length);
          cw_reset_adaptive_average_internal (&cw_dash_tracking,
                                              cw_receive_dash_length);
        }
    }
}


/**
 * cw_enable_adaptive_receive()
 * cw_disable_adaptive_receive()
 * cw_get_adaptive_receive_state()
 *
 * Enable and disable adaptive receive speeds.  If adaptive speed tracking
 * is enabled, the receive functions will attempt to automatically adjust
 * the receive speed setting to match the speed of the incoming Morse code.
 * If it is disabled, the receive functions will use fixed speed settings,
 * and reject incoming Morse which is not at the expected speed.  The
 * cw_get_adaptive_receive_state function returns TRUE if adaptive speed
 * tracking is enabled, FALSE otherwise.  Adaptive speed tracking uses a
 * moving average of the past four elements as its baseline for tracking
 * speeds.  The default state is adaptive tracking disabled.
 */
void
cw_enable_adaptive_receive (void)
{
  cw_set_adaptive_receive_internal (TRUE);
}

void
cw_disable_adaptive_receive (void)
{
  cw_set_adaptive_receive_internal (FALSE);
}

int
cw_get_adaptive_receive_state (void)
{
  return cw_is_adaptive_receive_enabled;
}


/**
 * cw_validate_timestamp_internal()
 *
 * If an input timestamp is given, validate it for correctness, and if valid,
 * copy it into return_timestamp and return TRUE.  If invalid, return FALSE
 * with errno set to EINVAL.  If an input timestamp is not given (NULL), return
 * TRUE with the current system time in return_timestamp.
 */
static int
cw_validate_timestamp_internal (const struct timeval *timestamp,
                                struct timeval *return_timestamp)
{
  if (timestamp)
    {
      if (timestamp->tv_sec < 0 || timestamp->tv_usec < 0
          || timestamp->tv_usec >= USECS_PER_SEC)
        {
          errno = EINVAL;
          return RC_ERROR;
        }
      *return_timestamp = *timestamp;
    }
  else
    {
      if (gettimeofday (return_timestamp, NULL) != 0)
        {
          perror ("cw: gettimeofday");
          return RC_ERROR;
        }
    }
  return RC_SUCCESS;
}


/*
 * The CW receive functions implement the following state graph:
 *
 *        +----------------- RS_ERR_WORD <-------------------+
 *        |(clear)                ^                          |
 *        |           (delay=long)|                          |
 *        |                       |                          |
 *        +----------------- RS_ERR_CHAR <---------+         |
 *        |(clear)                ^  |             |         |
 *        |                       |  +-------------+         |(error,
 *        |                       |   (delay=short)          | delay=long)
 *        |    (error,delay=short)|                          |
 *        |                       |  +-----------------------+
 *        |                       |  |
 *        +--------------------+  |  |
 *        |             (noise)|  |  |
 *        |                    |  |  |
 *        v    (start tone)    |  |  |  (end tone,noise)
 * --> RS_IDLE ------------> RS_IN_TONE ------------> RS_AFTER_TONE <------- +
 *     |  ^                           ^               | |    | ^ |           |
 *     |  |          (delay=short)    +---------------+ |    | | +-----------+
 *     |  |        +--------------+     (start tone)    |    | |  (not ready,
 *     |  |        |              |                     |    | |   buffer dot,
 *     |  |        +-------> RS_END_CHAR <--------------+    | |   buffer dash)
 *     |  |                   |   |       (delay=short)      | |
 *     |  +-------------------+   |                          | |
 *     |  |(clear)                |                          | |
 *     |  |           (delay=long)|                          | |
 *     |  |                       v                          | |
 *     |  +----------------- RS_END_WORD <-------------------+ |
 *     |   (clear)                        (delay=long)         |(buffer dot,
 *     |                                                       | buffer dash)
 *     +-------------------------------------------------------+
 */
static enum
{ RS_IDLE, RS_IN_TONE, RS_AFTER_TONE, RS_END_CHAR, RS_END_WORD, RS_ERR_CHAR,
  RS_ERR_WORD
}
cw_receive_state = RS_IDLE;


/**
 * cw_compare_timestamps_internal()
 *
 * Compare two timestamps, and return the difference between them in
 * microseconds, taking care to clamp values which would overflow an int.
 * This routine always returns a positive integer in the range 0 to INT_MAX.
 */
static int
cw_compare_timestamps_internal (const struct timeval *earlier,
                                const struct timeval *later)
{
  int delta_usec;

  /*
   * Compare the timestamps, taking care on overflows.
   *
   * At 4 WPM, the dash length is 3*(1200000/4)=900,000 usecs, and the word
   * gap is 2,100,000 usecs.  With the maximum Farnsworth additional delay,
   * the word gap extends to 20,100,000 usecs.  This fits into an int with a
   * lot of room to spare, in fact, an int can represent 2,147,483,647 usecs,
   * or around 33 minutes.  This is way, way longer than we'd ever want to
   * differentiate, so if by some chance we see timestamps farther apart than
   * this, and it ought to be very, very unlikely, then we'll clamp the
   * return value to INT_MAX with a clear conscience.
   *
   * Note: passing nonsensical or bogus timevals in may result in unpredict-
   * able results.  Nonsensical includes timevals with -ve tv_usec, -ve
   * tv_sec, tv_usec >= 1,000,000, etc.  To help in this, we check all
   * incoming timestamps for 'well-formedness'.  However, we assume the
   * gettimeofday() call always returns good timevals.  All in all, timeval
   * could probably be a better thought-out structure.
   */

  /* Calculate an initial delta, possibly with overflow. */
  delta_usec = (later->tv_sec - earlier->tv_sec) * USECS_PER_SEC
               + later->tv_usec - earlier->tv_usec;

  /* Check specifically for overflow, and clamp if it did. */
  if ((later->tv_sec - earlier->tv_sec) > (INT_MAX / USECS_PER_SEC) + 1
      || delta_usec < 0)
    delta_usec = INT_MAX;

  return delta_usec;
}


/**
 * cw_start_receive_tone()
 *
 * Called on the start of a receive tone.  If the timestamp is NULL, the
 * current time is used.  On success, the routine returns TRUE.   On error,
 * it returns FALSE, with errno set to ERANGE if the call is directly after
 * another cw_start_receive_tone call or if an existing received character
 * has not been cleared from the buffer, or EINVAL if the timestamp passed
 * in is invalid.
 */
int
cw_start_receive_tone (const struct timeval *timestamp)
{
  /*
   * If the receive state is not idle or after a tone, this is a state error.
   * A receive tone start can only happen while we are idle, or in the middle
   * of a character.
   */
  if (cw_receive_state != RS_IDLE && cw_receive_state != RS_AFTER_TONE)
    {
      errno = ERANGE;
      return RC_ERROR;
    }

  /* Validate and save the timestamp, or get one and then save it. */
  if (!cw_validate_timestamp_internal (timestamp, &cw_rr_start_timestamp))
    return RC_ERROR;

  /*
   * If we are in the after tone state, we can measure the inter-element
   * gap by comparing the start timestamp with the last end one, guaranteed
   * set by getting to the after tone state via cw_end_receive tone, or in
   * extreme cases, by cw_receive_buffer_element_internal.
   *
   * Do that, then, and update the relevant statistics.
   */
  if (cw_receive_state == RS_AFTER_TONE)
    {
      int space_usec;

      space_usec = cw_compare_timestamps_internal (&cw_rr_end_timestamp,
                                                   &cw_rr_start_timestamp);
      cw_add_receive_statistic_internal (STAT_END_ELEMENT, space_usec);
    }

  /* Set state to indicate we are inside a tone. */
  cw_receive_state = RS_IN_TONE;

  if (cw_is_debugging_internal (CW_DEBUG_RECEIVE_STATES))
    fprintf (stderr, "cw: receive state ->%d\n", cw_receive_state);

  return RC_SUCCESS;
}


/**
 * cw_identify_receive_tone_internal()
 *
 * Analyses a tone using the ranges provided by the low level timing
 * parameters.  On success, it returns TRUE and sends back either a dot or
 * a dash in representation.  On error, it returns FALSE with errno set to
 * ENOENT if the tone is not recognizable as either a dot or a dash,
 * and sets the receive state to one of the error states, depending on
 * the tone length passed in.
 *
 * Note; for adaptive timing, the tone should _always_ be recognized as
 * a dot or a dash, because the ranges will have been set to cover 0 to
 * INT_MAX.
 */
static int
cw_identify_receive_tone_internal (int element_usec, char *representation)
{
  /* Synchronize low level timings if required */
  cw_sync_parameters_internal ();

  /* If the timing was, within tolerance, a dot, return dot to the caller.  */
  if (element_usec >= cw_dot_range_minimum
      && element_usec <= cw_dot_range_maximum)
    {
      *representation = CW_DOT_REPRESENTATION;
      return RC_SUCCESS;
    }

  /* Do the same for a dash. */
  if (element_usec >= cw_dash_range_minimum
      && element_usec <= cw_dash_range_maximum)
    {
      *representation = CW_DASH_REPRESENTATION;
      return RC_SUCCESS;
    }

  /*
   * This element is not a dot or a dash, so we have an error case.  Depending
   * on the timestamp difference, we pick which of the error states to move
   * to, and move to it.  The comparison is against the expected end-of-char
   * delay.  If it's larger, then fix at word error, otherwise settle on char
   * error.
   *
   * Note that we should never reach here for adaptive timing receive.
   */
  cw_receive_state = element_usec > cw_eoc_range_maximum
                     ? RS_ERR_WORD : RS_ERR_CHAR;

  if (cw_is_debugging_internal (CW_DEBUG_RECEIVE_STATES))
    fprintf (stderr, "cw: receive state ->%d\n", cw_receive_state);

  /* Return ENOENT to the caller. */
  errno = ENOENT;
  return RC_ERROR;
}


/**
 * cw_update_adaptive_tracking_internal()
 *
 * Updates the averages of dot and dash lengths, and recalculates the
 * adaptive threshold for the next receive tone.
 */
static void
cw_update_adaptive_tracking_internal (int element_usec, char element)
{
  int average_dot, average_dash;

  /* We are not going to tolerate being called in fixed speed mode. */
  if (!cw_is_adaptive_receive_enabled)
    return;

  /*
   * We will update the information held for either dots or dashes.  Which we
   * pick depends only on what the representation of the character was
   * identified as earlier.
   */
  if (element == CW_DOT_REPRESENTATION)
    cw_update_adaptive_average_internal (&cw_dot_tracking, element_usec);
  else if (element == CW_DASH_REPRESENTATION)
    cw_update_adaptive_average_internal (&cw_dash_tracking, element_usec);

  /*
   * Recalculate the adaptive threshold from the values currently held in the
   * moving averages.  The threshold is calculated as (avg dash length -
   * avg dot length) / 2 + avg dot_length.
   */
  average_dot = cw_get_adaptive_average_internal (&cw_dot_tracking);
  average_dash = cw_get_adaptive_average_internal (&cw_dash_tracking);
  cw_adaptive_receive_threshold = (average_dash - average_dot) / 2
                                  + average_dot;

  /*
   * Resynchronize the low level timing data following recalculation.  If the
   * resultant recalculated speed is outside the limits, clamp the speed to
   * the limit value and recalculate again.
   *
   * Resetting the speed directly really means unsetting adaptive mode,
   * resyncing to calculate the new threshold, which unfortunately recalcu-
   * lates everything else according to fixed speed; so, we then have to reset
   * adaptive and resyncing one more time, to get all other timing parameters
   * back to where they should be.
   */
  cw_is_in_sync = FALSE;
  cw_sync_parameters_internal ();
  if (cw_receive_speed < CW_MIN_SPEED || cw_receive_speed > CW_MAX_SPEED)
    {
      cw_receive_speed = cw_receive_speed < CW_MIN_SPEED
                         ? CW_MIN_SPEED : CW_MAX_SPEED;
      cw_is_adaptive_receive_enabled = FALSE;
      cw_is_in_sync = FALSE;
      cw_sync_parameters_internal ();
      cw_is_adaptive_receive_enabled = TRUE;
      cw_is_in_sync = FALSE;
      cw_sync_parameters_internal ();
    }
}


/**
 * cw_end_receive_tone()
 *
 * Called on the end of a receive tone.  If the timestamp is NULL, the
 * current time is used.  On success, the routine adds a dot or dash to
 * the receive representation buffer, and returns TRUE.  On error, it
 * returns FALSE, with errno set to ERANGE if the call was not preceded by
 * a cw_start_receive_tone call, EINVAL if the timestamp passed in is not
 * valid, ENOENT if the tone length was out of bounds for the permissible
 * dot and dash lengths and fixed speed receiving is selected, ENOMEM if
 * the representation buffer is full, or EAGAIN if the tone was shorter
 * than the threshold for noise and was therefore ignored.
 */
int
cw_end_receive_tone (const struct timeval *timestamp)
{
  int status, element_usec;
  char representation;
  struct timeval saved_end_timestamp;

  /* The receive state is expected to be inside a tone. */
  if (cw_receive_state != RS_IN_TONE)
    {
      errno = ERANGE;
      return RC_ERROR;
    }

  /*
   * Take a safe copy of the current end timestamp, in case we need to put
   * it back if we decide this tone is really just noise.
   */
  saved_end_timestamp = cw_rr_end_timestamp;

  /* Save the timestamp passed in, or get one. */
  if (!cw_validate_timestamp_internal (timestamp, &cw_rr_end_timestamp))
    return RC_ERROR;

  /* Compare the timestamps to determine the length of the tone. */
  element_usec = cw_compare_timestamps_internal (&cw_rr_start_timestamp,
                                                 &cw_rr_end_timestamp);

  /*
   * If the tone length is shorter than any noise canceling threshold that
   * has been set, then ignore this tone.  This means reverting to the state
   * before the call to cw_start_receive_tone.  Now, by rights, we should use
   * an extra state, RS_IN_FIRST_TONE, say, so that we know whether to go
   * back to the idle state, or to after tone.  But to make things a touch
   * simpler, here we can just look at the current receive buffer pointer.
   * If it's zero, we came from idle, otherwise we came from after tone.
   */
  if (cw_noise_spike_threshold > 0
      && element_usec <= cw_noise_spike_threshold)
    {
      cw_receive_state = cw_rr_current == 0 ? RS_IDLE : RS_AFTER_TONE;

      /*
       * Put the end tone timestamp back to how it was when we came in to
       * the routine.
       */
      cw_rr_end_timestamp = saved_end_timestamp;

      if (cw_is_debugging_internal (CW_DEBUG_RECEIVE_STATES))
        fprintf (stderr, "cw: receive state ->%d\n", cw_receive_state);

      errno = EAGAIN;
      return RC_ERROR;
    }

  /*
   * At this point, we have to make a decision about the element just
   * received.  We'll use a routine that compares ranges to tell us what it
   * thinks this element is.  If it can't decide, it will hand us back an
   * error which we return to the caller.  Otherwise, it returns a character,
   * dot or dash, for us to buffer.
   */
  status = cw_identify_receive_tone_internal (element_usec, &representation);
  if (!status)
    return RC_ERROR;

  /*
   * Update the averaging buffers so that the adaptive tracking of received
   * Morse speed stays up to date.  But only do this if we have set adaptive
   * receiving; don't fiddle about trying to track for fixed speed receive.
   */
  if (cw_is_adaptive_receive_enabled)
    cw_update_adaptive_tracking_internal (element_usec, representation);

  /*
   * Update dot and dash timing statistics.  It may seem odd to do this after
   * calling cw_update_adaptive_tracking_internal, rather than before, as
   * this function changes the ideal values we're measuring against.  But if
   * we're on a speed change slope, the adaptive tracking smoothing will
   * cause the ideals to lag the observed speeds.  So by doing this here, we
   * can at least ameliorate this effect, if not eliminate it.
   */
  if (representation == CW_DOT_REPRESENTATION)
    cw_add_receive_statistic_internal (STAT_DOT, element_usec);
  else
    cw_add_receive_statistic_internal (STAT_DASH, element_usec);

  /* Add the representation character to the receive buffer. */
  cw_receive_representation_buffer[cw_rr_current++] = representation;

  /*
   * We just added a representation to the receive buffer.  If it's full,
   * then we have to do something, even though it's unlikely.  What we'll do
   * is make a unilateral declaration that if we get this far, we go to
   * end-of-char error state automatically.
   */
  if (cw_rr_current == RECEIVE_CAPACITY - 1)
    {
      cw_receive_state = RS_ERR_CHAR;

      if (cw_is_debugging_internal (CW_DEBUG_RECEIVE_STATES))
        fprintf (stderr, "cw: receive state ->%d\n", cw_receive_state);

      errno = ENOMEM;
      return RC_ERROR;
    }

  /* All is well.  Move to the more normal after-tone state. */
  cw_receive_state = RS_AFTER_TONE;

  if (cw_is_debugging_internal (CW_DEBUG_RECEIVE_STATES))
    fprintf (stderr, "cw: receive state ->%d\n", cw_receive_state);

  return RC_SUCCESS;
}


/**
 * cw_receive_buffer_element_internal()
 *
 * Adds either a dot or a dash to the receive representation buffer.  If
 * the timestamp is NULL, the current timestamp is used.  The receive state
 * is updated as if we had just received a call to cw_end_receive_tone.
 */
static int
cw_receive_buffer_element_internal (const struct timeval *timestamp,
                                    char element)
{
  /*
   * The receive state is expected to be idle or after a tone in order to
   * use this routine.
   */
  if (cw_receive_state != RS_IDLE && cw_receive_state != RS_AFTER_TONE)
    {
      errno = ERANGE;
      return RC_ERROR;
    }

  /*
   * This routine functions as if we have just seen a tone end, yet without
   * really seeing a tone start.  To keep timing information for routines
   * that come later, we need to make sure that the end of tone timestamp is
   * set here.  This is because the receive representation routine looks at
   * the time since the last end of tone to determine whether we are at the
   * end of a word, or just at the end of a character.  It doesn't matter that
   * the start of tone timestamp is never set - this is just for timing the
   * tone length, and we don't need to do that since we've already been told
   * whether this is a dot or a dash.
   */
  if (!cw_validate_timestamp_internal (timestamp, &cw_rr_end_timestamp))
    return RC_ERROR;

  /* Add the element to the receive representation buffer. */
  cw_receive_representation_buffer[cw_rr_current++] = element;

  /*
   * We just added an element to the receive buffer.  As above, if it's full,
   * then we have to do something, even though it's unlikely to actually be
   * full.
   */
  if (cw_rr_current == RECEIVE_CAPACITY - 1)
    {
      cw_receive_state = RS_ERR_CHAR;

      if (cw_is_debugging_internal (CW_DEBUG_RECEIVE_STATES))
        fprintf (stderr, "cw: receive state ->%d\n", cw_receive_state);

      errno = ENOMEM;
      return RC_ERROR;
    }

  /*
   * Since we effectively just saw the end of a tone, move to the after-tone
   * state.
   */
  cw_receive_state = RS_AFTER_TONE;

  if (cw_is_debugging_internal (CW_DEBUG_RECEIVE_STATES))
    fprintf (stderr, "cw: receive state ->%d\n", cw_receive_state);

  return RC_SUCCESS;
}


/**
 * cw_receive_buffer_dot()
 * cw_receive_buffer_dash()
 *
 * Adds either a dot or a dash to the receive representation buffer.  If
 * the timestamp is NULL, the current timestamp is used.  These routines
 * are for callers that have already determined whether a dot or dash was
 * received by a method other than calling the routines cw_start_receive_tone
 * and cw_end_receive_tone.  On success, the relevant element is added to
 * the receive representation buffer.  On error, the routines return FALSE,
 * with errno set to ERANGE if preceded by a cw_start_receive_tone call
 * with no matching cw_end_receive_tone or if an error condition currently
 * exists within the receive buffer, or ENOMEM if the receive representation
 * buffer is full.
 */
int
cw_receive_buffer_dot (const struct timeval *timestamp)
{
  return cw_receive_buffer_element_internal (timestamp, CW_DOT_REPRESENTATION);
}

int
cw_receive_buffer_dash (const struct timeval *timestamp)
{
  return cw_receive_buffer_element_internal (timestamp, CW_DASH_REPRESENTATION);
}


/**
 * cw_receive_representation()
 *
 * Returns the current buffered representation from the receive buffer.
 * On success, the function returns TRUE, and fills in representation with the
 * contents of the current representation buffer.  On error, it returns FALSE,
 * with errno set to ERANGE if not preceded by a cw_end_receive_tone call,
 * a prior successful cw_receive_representation call, or a prior
 * cw_receive_buffer_dot or cw_receive_buffer_dash, EINVAL if the timestamp
 * passed in is invalid, or EAGAIN if the call is made too early to determine
 * whether a complete representation has yet been placed in the buffer
 * (that is, less than the inter-character gap period elapsed since the last
 * cw_end_receive_tone or cw_receive_buffer_dot/dash call).  is_end_of_word
 * indicates that the delay after the last tone received is longer that the
 * inter-word gap, and is_error indicates that the representation was
 * terminated by an error condition.
 */
int
cw_receive_representation (const struct timeval *timestamp,
                           char *representation, int *is_end_of_word,
                           int *is_error)
{
  int space_usec;
  struct timeval now_timestamp;

  /*
   * If the the receive state indicates that we have in our possession a
   * completed representation at the end of word, just [re-]return it.
   */
  if (cw_receive_state == RS_END_WORD || cw_receive_state == RS_ERR_WORD)
    {
      if (is_end_of_word)
        *is_end_of_word = TRUE;
      if (is_error)
        *is_error = (cw_receive_state == RS_ERR_WORD);
      *representation = '\0';
      strncat (representation, cw_receive_representation_buffer, cw_rr_current);
      return RC_SUCCESS;
    }

  /*
   * If the receive state is also not end-of-char, and also not after a tone,
   * then we are idle or in a tone; in these cases, we return ERANGE.
   */
  if (cw_receive_state != RS_AFTER_TONE
      && cw_receive_state != RS_END_CHAR && cw_receive_state != RS_ERR_CHAR)
    {
      errno = ERANGE;
      return RC_ERROR;
    }

  /*
   * We now know the state is after a tone, or end-of-char, perhaps with
   * error.  For all three of these cases, we're going to [re-]compare the
   * timestamp with the end of tone timestamp.  This could mean that in the
   * case of end-of-char, we revise our opinion on later calls to end-of-word.
   * This is correct, since it models reality.
   */

  /*
   * If we weren't supplied with one, get the current timestamp for comparison
   * against the latest end timestamp.
   */
  if (!cw_validate_timestamp_internal (timestamp, &now_timestamp))
    return RC_ERROR;

  /*
   * Now we need to compare the timestamps to determine the length of the
   * inter-tone gap.
   */
  space_usec = cw_compare_timestamps_internal (&cw_rr_end_timestamp,
                                               &now_timestamp);

  /* Synchronize low level timings if required */
  cw_sync_parameters_internal ();

  /*
   * If the timing was, within tolerance, a character space, then that is
   * what we'll call it.  In this case, we complete the representation and
   * return it.
   */
  if (space_usec >= cw_eoc_range_minimum
      && space_usec <= cw_eoc_range_maximum)
    {
      /*
       * If state is after tone, we can validly move at this point to end of
       * char.  If it's not, then we're at end char or at end char with error
       * already, so leave it.  On moving, update timing statistics for an
       * identified end of character.
       */
      if (cw_receive_state == RS_AFTER_TONE)
        {
          cw_add_receive_statistic_internal (STAT_END_CHARACTER, space_usec);
          cw_receive_state = RS_END_CHAR;
        }

      if (cw_is_debugging_internal (CW_DEBUG_RECEIVE_STATES))
        fprintf (stderr, "cw: receive state ->%d\n", cw_receive_state);

      /* Return the representation buffered. */
      if (is_end_of_word)
        *is_end_of_word = FALSE;
      if (is_error)
        *is_error = (cw_receive_state == RS_ERR_CHAR);
      *representation = '\0';
      strncat (representation, cw_receive_representation_buffer, cw_rr_current);
      return RC_SUCCESS;
    }

  /*
   * If the timing indicated a word space, again we complete the representation
   * and return it.  In this case, we also need to inform the client that this
   * looked like the end of a word, not just a character.  And, we don't care
   * about the maximum period, only that it exceeds the low end of the range.
   */
  if (space_usec > cw_eoc_range_maximum)
    {
      /*
       * In this case, we have a transition to an end of word case.  If we
       * were sat in an error case, we need to move to the correct end of word
       * state, otherwise, at after tone, we go safely to the non-error end
       * of word.
       */
      cw_receive_state = cw_receive_state == RS_ERR_CHAR
                         ? RS_ERR_WORD : RS_END_WORD;

      if (cw_is_debugging_internal (CW_DEBUG_RECEIVE_STATES))
        fprintf (stderr, "cw: receive state ->%d\n", cw_receive_state);

      /* Return the representation buffered. */
      if (is_end_of_word)
        *is_end_of_word = TRUE;
      if (is_error)
        *is_error = (cw_receive_state == RS_ERR_WORD);
      *representation = '\0';
      strncat (representation, cw_receive_representation_buffer, cw_rr_current);
      return RC_SUCCESS;
    }

  /*
   * If none of these conditions holds, then we cannot yet make a judgement
   * on what we have in the buffer, so return EAGAIN.
   */
  errno = EAGAIN;
  return RC_ERROR;
}


/**
 * cw_receive_character()
 *
 * Returns the current buffered character from the representation buffer.
 * On success, the function returns TRUE, and fills char *c with the contents
 * of the current representation buffer, translated into a character.  On
 * error, it returns FALSE, with errno set to ERANGE if not preceded by a
 * cw_end_receive_tone call, a prior successful cw_receive_character
 * call, or a cw_receive_buffer_dot or cw_receive_buffer dash call, EINVAL
 * if the timestamp passed in is invalid, or EAGAIN if the call is made too
 * early to determine whether a complete character has yet been placed in the
 * buffer (that is, less than the inter-character gap period elapsed since
 * the last cw_end_receive_tone or cw_receive_buffer_dot/dash call).
 * is_end_of_word indicates that the delay after the last tone received is
 * longer that the inter-word gap, and is_error indicates that the character
 * was terminated by an error condition.
 */
int
cw_receive_character (const struct timeval *timestamp,
                      char *c, int *is_end_of_word, int *is_error)
{
  int status, end_of_word, error;
  char character, representation[RECEIVE_CAPACITY + 1];

  /* See if we can obtain a representation from the receive routines. */
  status = cw_receive_representation (timestamp, representation,
                                      &end_of_word, &error);
  if (!status)
    return RC_ERROR;

  /* Look up the representation using the lookup functions. */
  character = cw_lookup_representation_internal (representation);
  if (!character)
    {
      errno = ENOENT;
      return RC_ERROR;
    }

  /* If we got this far, all is well, so return what we uncovered. */
  if (c)
    *c = character;
  if (is_end_of_word)
    *is_end_of_word = end_of_word;
  if (is_error)
    *is_error = error;
  return RC_SUCCESS;
}


/**
 * cw_clear_receive_buffer()
 *
 * Clears the receive representation buffer to receive tones again.  This
 * routine must be called after successful, or terminating,
 * cw_receive_representation or cw_receive_character calls, to clear the
 * states and prepare the buffer to receive more tones.
 */
void
cw_clear_receive_buffer (void)
{
  cw_rr_current = 0;
  cw_receive_state = RS_IDLE;

  if (cw_is_debugging_internal (CW_DEBUG_RECEIVE_STATES))
    fprintf (stderr, "cw: receive state ->%d\n", cw_receive_state);
}


/**
 * cw_get_receive_buffer_capacity()
 *
 * Returns the number of entries the receive buffer can accommodate.  The
 * maximum number of character written out by cw_receive_representation is
 * the capacity + 1, the extra character being used for the terminating
 * NUL.
 */
int
cw_get_receive_buffer_capacity (void)
{
  return RECEIVE_CAPACITY;
}


/**
 * cw_get_receive_buffer_length()
 *
 * Returns the number of elements currently pending in the receive buffer.
 */
int
cw_get_receive_buffer_length (void)
{
  return cw_rr_current;
}


/**
 * cw_reset_receive()
 *
 * Clear the receive representation buffer, statistics, and any retained
 * receive state.  This function is suitable for calling from an application
 * exit handler.
 */
void
cw_reset_receive (void)
{
  cw_rr_current = 0;
  cw_receive_state = RS_IDLE;

  cw_reset_receive_statistics ();

  if (cw_is_debugging_internal (CW_DEBUG_RECEIVE_STATES))
    fprintf (stderr, "cw: receive state ->%d (reset)\n", cw_receive_state);
}


/*---------------------------------------------------------------------*/
/*  Iambic keyer                                                       */
/*---------------------------------------------------------------------*/

/*
 * Iambic keyer status.  The keyer functions maintain the current known state
 * of the paddles, and latch FALSE-to-TRUE transitions while busy, to form the
 * iambic effect.  For Curtis mode B, the keyer also latches any point where
 * both paddle states are TRUE at the same time.
 */
static volatile int cw_ik_dot_paddle = FALSE,      /* Dot paddle state */
                    cw_ik_dash_paddle = FALSE,     /* Dash paddle state */
                    cw_ik_dot_latch = FALSE,       /* Dot FALSE->TRUE latch */
                    cw_ik_dash_latch = FALSE,      /* Dash FALSE->TRUE latch */
                    cw_ik_curtis_b_latch = FALSE;  /* Curtis Dot&&Dash latch */

/*
 * Iambic keyer "Curtis" mode A/B selector.  Mode A and mode B timings differ
 * slightly, and some people have a preference for one or the other.  Mode A
 * is a bit less timing-critical, so we'll make that the default.
 */
static volatile int cw_ik_curtis_mode_b = FALSE;

/**
 * cw_enable_iambic_curtis_mode_b()
 * cw_disable_iambic_curtis_mode_b()
 * cw_get_iambic_curtis_mode_b_state()
 *
 * Normally, the iambic keying functions will emulate Curtis 8044 Keyer
 * mode A.  In this mode, when both paddles are pressed together, the last
 * dot or dash being sent on release is completed, and nothing else is sent.
 * In mode B, when both paddles are pressed together, the last dot or dash
 * being sent on release is completed, then an opposite element is also sent.
 * Some operators prefer mode B, but timing is more critical in this mode.
 * The default mode is Curtis mode A.
 */
void
cw_enable_iambic_curtis_mode_b (void)
{
  cw_ik_curtis_mode_b = TRUE;
}

void
cw_disable_iambic_curtis_mode_b (void)
{
  cw_ik_curtis_mode_b = FALSE;
}

int
cw_get_iambic_curtis_mode_b_state (void)
{
  return cw_ik_curtis_mode_b;
}


/*
 * The CW keyer functions implement the following state graph:
 *
 *        +-----------------------------------------------------+
 *        |          (all latches clear)                        |
 *        |                                     (dot latch)     |
 *        |                          +--------------------------+
 *        |                          |                          |
 *        |                          v                          |
 *        |      +-------------> KS_IN_DOT_[A|B] -------> KS_AFTER_DOT_[A|B]
 *        |      |(dot paddle)       ^            (delay)       |
 *        |      |                   |                          |(dash latch/
 *        |      |                   +------------+             | _B)
 *        v      |                                |             |
 * --> KS_IDLE --+                   +--------------------------+
 *        ^      |                   |            |
 *        |      |                   |            +-------------+(dot latch/
 *        |      |                   |                          | _B)
 *        |      |(dash paddle)      v            (delay)       |
 *        |      +-------------> KS_IN_DASH_[A|B] -------> KS_AFTER_DASH_[A|B]
 *        |                          ^                          |
 *        |                          |                          |
 *        |                          +--------------------------+
 *        |                                     (dash latch)    |
 *        |          (all latches clear)                        |
 *        +-----------------------------------------------------+
 */
static volatile enum
{ KS_IDLE, KS_IN_DOT_A, KS_IN_DASH_A, KS_AFTER_DOT_A, KS_AFTER_DASH_A,
  KS_IN_DOT_B, KS_IN_DASH_B, KS_AFTER_DOT_B, KS_AFTER_DASH_B
}
cw_keyer_state = KS_IDLE;


/**
 * cw_keyer_clock_internal()
 *
 * Informs the internal keyer states that the itimer expired, and we received
 * SIGALRM.
 */
static void
cw_keyer_clock_internal (void)
{
  /* Synchronize low level timing parameters if required. */
  cw_sync_parameters_internal ();

  /* Decide what to do based on the current state. */
  switch (cw_keyer_state)
    {
    /* Ignore calls if our state is idle. */
    case KS_IDLE:
      return;

    /*
     * If we were in a dot, turn off tones and begin the after-dot delay.  Do
     * much the same if we are in a dash.  No routine status checks are made
     * since we are in a signal handler, and can't readily return error codes
     * to the client.
     */
    case KS_IN_DOT_A:
    case KS_IN_DOT_B:
      cw_sound_internal (TONE_SILENT);
      cw_key_control_internal (FALSE);
      cw_request_timeout_internal (cw_end_of_ele_delay, NULL);
      cw_keyer_state = cw_keyer_state == KS_IN_DOT_A
                       ? KS_AFTER_DOT_A : KS_AFTER_DOT_B;

      if (cw_is_debugging_internal (CW_DEBUG_KEYER_STATES))
        fprintf (stderr, "cw: keyer ->%d\n", cw_keyer_state);
      break;

    case KS_IN_DASH_A:
    case KS_IN_DASH_B:
      cw_sound_internal (TONE_SILENT);
      cw_key_control_internal (FALSE);
      cw_request_timeout_internal (cw_end_of_ele_delay, NULL);
      cw_keyer_state = cw_keyer_state == KS_IN_DASH_A
                       ? KS_AFTER_DASH_A : KS_AFTER_DASH_B;

      if (cw_is_debugging_internal (CW_DEBUG_KEYER_STATES))
        fprintf (stderr, "cw: keyer ->%d\n", cw_keyer_state);
      break;

    /*
     * If we have just finished a dot or a dash and its post-element delay,
     * then reset the latches as appropriate.  Next, if in a _B state, go
     * straight to the opposite element state.  If in an _A state, check the
     * latch states; if the opposite latch is set TRUE, then do the iambic
     * thing and alternate dots and dashes.  If the same latch is TRUE,
     * repeat.  And if nothing is true, then revert to idling.
     */
    case KS_AFTER_DOT_A:
    case KS_AFTER_DOT_B:
      if (!cw_ik_dot_paddle)
        cw_ik_dot_latch = FALSE;
      if (cw_keyer_state == KS_AFTER_DOT_B)
        {
          cw_sound_internal (generator->frequency);
          cw_key_control_internal (TRUE);
          cw_request_timeout_internal (cw_send_dash_length, NULL);
          cw_keyer_state = KS_IN_DASH_A;
        }
      else if (cw_ik_dash_latch)
        {
          cw_sound_internal (generator->frequency);
          cw_key_control_internal (TRUE);
          cw_request_timeout_internal (cw_send_dash_length, NULL);
          if (cw_ik_curtis_b_latch)
            {
              cw_ik_curtis_b_latch = FALSE;
              cw_keyer_state = KS_IN_DASH_B;
            }
          else
            cw_keyer_state = KS_IN_DASH_A;
        }
      else if (cw_ik_dot_latch)
        {
          cw_sound_internal (generator->frequency);
          cw_key_control_internal (TRUE);
          cw_request_timeout_internal (cw_send_dot_length, NULL);
          cw_keyer_state = KS_IN_DOT_A;
        }
      else
        {
          cw_keyer_state = KS_IDLE;
          cw_schedule_finalization_internal ();
        }

      if (cw_is_debugging_internal (CW_DEBUG_KEYER_STATES))
        fprintf (stderr, "cw: keyer ->%d\n", cw_keyer_state);
      break;

    case KS_AFTER_DASH_A:
    case KS_AFTER_DASH_B:
      if (!cw_ik_dash_paddle)
        cw_ik_dash_latch = FALSE;
      if (cw_keyer_state == KS_AFTER_DASH_B)
        {
          cw_sound_internal (generator->frequency);
          cw_key_control_internal (TRUE);
          cw_request_timeout_internal (cw_send_dot_length, NULL);
          cw_keyer_state = KS_IN_DOT_A;
        }
      else if (cw_ik_dot_latch)
        {
          cw_sound_internal (generator->frequency);
          cw_key_control_internal (TRUE);
          cw_request_timeout_internal (cw_send_dot_length, NULL);
          if (cw_ik_curtis_b_latch)
            {
              cw_ik_curtis_b_latch = FALSE;
              cw_keyer_state = KS_IN_DOT_B;
            }
          else
            cw_keyer_state = KS_IN_DOT_A;
        }
      else if (cw_ik_dash_latch)
        {
          cw_sound_internal (generator->frequency);
          cw_key_control_internal (TRUE);
          cw_request_timeout_internal (cw_send_dash_length, NULL);
          cw_keyer_state = KS_IN_DASH_A;
        }
      else
        {
          cw_keyer_state = KS_IDLE;
          cw_schedule_finalization_internal ();
        }

      if (cw_is_debugging_internal (CW_DEBUG_KEYER_STATES))
        fprintf (stderr, "cw: keyer ->%d\n", cw_keyer_state);
      break;
    }
}


/**
 * cw_notify_keyer_paddle_event()
 *
 * Informs the internal keyer states that the keyer paddles have changed
 * state.  The new paddle states are recorded, and if either transition from
 * FALSE to TRUE, paddle latches, for iambic functions, are also set.
 * On success, the routine returns TRUE.  On error, it returns FALSE, with
 * errno set to EBUSY if the tone queue or straight key are using the sound
 * card, console speaker, or keying system.
 *
 * If appropriate, this routine starts the keyer functions sending the
 * relevant element.  Element send and timing occurs in the background, so
 * this routine returns almost immediately.  See cw_keyer_element_wait and
 * cw_keyer_wait for details about how to check the current status of
 * iambic keyer background processing.
 */
int
cw_notify_keyer_paddle_event (int dot_paddle_state,
                              int dash_paddle_state)
{
  /*
   * If the tone queue or the straight key are busy, this is going to conflict
   * with our use of the sound card, console sounder, and keying system.  So
   * return an error status in this case.
   */
  if (cw_is_straight_key_busy () || cw_is_tone_busy ())
    {
      errno = EBUSY;
      return RC_ERROR;
    }

  /* Clean up and save the paddle states passed in. */
  cw_ik_dot_paddle = (dot_paddle_state != 0);
  cw_ik_dash_paddle = (dash_paddle_state != 0);

  /*
   * Update the paddle latches if either paddle goes TRUE.  The latches are
   * checked in the signal handler, so if the paddles go back to FALSE during
   * this element, the item still gets actioned.  The signal handler is also
   * responsible for clearing down the latches.
   */
  if (cw_ik_dot_paddle)
    cw_ik_dot_latch = TRUE;
  if (cw_ik_dash_paddle)
    cw_ik_dash_latch = TRUE;

  /*
   * If in Curtis mode B, make a special check for both paddles TRUE at the
   * same time.  This flag is checked by the signal handler, to determine
   * whether to add mode B trailing timing elements.
   */
  if (cw_ik_curtis_mode_b && cw_ik_dot_paddle && cw_ik_dash_paddle)
    cw_ik_curtis_b_latch = TRUE;

  if (cw_is_debugging_internal (CW_DEBUG_KEYER_STATES))
    fprintf (stderr, "cw: keyer paddles %d,%d, latches %d,%d, curtis_b %d\n",
             cw_ik_dot_paddle, cw_ik_dash_paddle,
             cw_ik_dot_latch, cw_ik_dash_latch, cw_ik_curtis_b_latch);

  /* If the current state is idle, give the state process a nudge. */
  if (cw_keyer_state == KS_IDLE)
    {
      if (cw_ik_dot_paddle)
        {
          /* Pretend we just finished a dash. */
          cw_keyer_state = cw_ik_curtis_b_latch
                           ? KS_AFTER_DASH_B : KS_AFTER_DASH_A;
          cw_request_timeout_internal (0, cw_keyer_clock_internal);
        }
      else if (cw_ik_dash_paddle)
        {
          /* Pretend we just finished a dot. */
          cw_keyer_state = cw_ik_curtis_b_latch
                           ? KS_AFTER_DOT_B : KS_AFTER_DOT_A;
          cw_request_timeout_internal (0, cw_keyer_clock_internal);
        }
    }

  if (cw_is_debugging_internal (CW_DEBUG_KEYER_STATES))
    fprintf (stderr, "cw: keyer ->%d\n", cw_keyer_state);

  return RC_SUCCESS;
}


/**
 * cw_notify_keyer_dot_paddle_event()
 * cw_notify_keyer_dash_paddle_event()
 *
 * Convenience functions to alter the state of just one of the two iambic
 * keyer paddles.  The other paddle state of the paddle pair remains unchanged.
 *
 * See cw_keyer_paddle_event for details of iambic keyer background processing,
 * and how to check its status.
 */
int
cw_notify_keyer_dot_paddle_event (int dot_paddle_state)
{
  return cw_notify_keyer_paddle_event (dot_paddle_state, cw_ik_dash_paddle);
}

int
cw_notify_keyer_dash_paddle_event (int dash_paddle_state)
{
  return cw_notify_keyer_paddle_event (cw_ik_dot_paddle, dash_paddle_state);
}


/**
 * cw_get_keyer_paddles()
 *
 * Returns the current saved states of the two paddles.
 */
void
cw_get_keyer_paddles (int *dot_paddle_state, int *dash_paddle_state)
{
  if (dot_paddle_state)
    *dot_paddle_state = cw_ik_dot_paddle;
  if (dash_paddle_state)
    *dash_paddle_state = cw_ik_dash_paddle;
}


/**
 * cw_get_keyer_paddle_latches()
 *
 * Returns the current saved states of the two paddle latches.  A paddle
 * latches is set to TRUE when the paddle state becomes true, and is
 * cleared if the paddle state is FALSE when the element finishes sending.
 */
void
cw_get_keyer_paddle_latches (int *dot_paddle_latch_state,
                             int *dash_paddle_latch_state)
{
  if (dot_paddle_latch_state)
    *dot_paddle_latch_state = cw_ik_dot_latch;
  if (dash_paddle_latch_state)
    *dash_paddle_latch_state = cw_ik_dash_latch;
}


/**
 * cw_is_keyer_busy()
 *
 * Indicates if the keyer is busy; returns TRUE if the keyer is going through
 * a dot or dash cycle, FALSE if the keyer is idle.
 */
int
cw_is_keyer_busy (void)
{
  return cw_keyer_state != KS_IDLE;
}


/**
 * cw_wait_for_keyer_element()
 *
 * Waits until the end of the current element, dot or dash, from the keyer.
 * This routine returns TRUE on success.  On error, it returns FALSE, with
 * errno set to EDEADLK if SIGALRM is blocked.
 */
int
cw_wait_for_keyer_element (void)
{
  int status;

  /* Check that SIGALRM is not blocked. */
  status = cw_check_signal_mask_internal ();
  if (!status)
    return RC_ERROR;

  /*
   * First wait for the state to move to idle (or just do nothing if it's
   * not), or to one of the after- states.
   */
  while (cw_keyer_state != KS_IDLE
         && cw_keyer_state != KS_AFTER_DOT_A
         && cw_keyer_state != KS_AFTER_DOT_B
         && cw_keyer_state != KS_AFTER_DASH_A
         && cw_keyer_state != KS_AFTER_DASH_B)
    cw_wait_for_signal_internal ();

  /*
   * Now wait for the state to move to idle (unless it is, or was, already),
   * or one of the in- states, at which point we know we're actually at the
   * end of the element we were in when we entered this routine.
   */
  while (cw_keyer_state != KS_IDLE
         && cw_keyer_state != KS_IN_DOT_A
         && cw_keyer_state != KS_IN_DOT_B
         && cw_keyer_state != KS_IN_DASH_A
         && cw_keyer_state != KS_IN_DASH_B)
    cw_wait_for_signal_internal ();

  return RC_SUCCESS;
}


/**
 * cw_wait_for_keyer()
 *
 * Waits for the current keyer cycle to complete.  The routine returns TRUE on
 * success.  On error, it returns FALSE, with errno set to EDEADLK if SIGALRM
 * is blocked or if either paddle state is TRUE.
 */
int
cw_wait_for_keyer (void)
{
  int status;

  /* Check that SIGALRM is not blocked. */
  status = cw_check_signal_mask_internal ();
  if (!status)
    return RC_ERROR;

  /*
   * Check that neither paddle is TRUE; if either is, then the signal cycle
   * is going to continue forever, and we'll never return from this routine.
   */
  if (cw_ik_dot_paddle || cw_ik_dash_paddle)
    {
      errno = EDEADLK;
      return RC_ERROR;
    }

  /* Wait for the keyer state to go idle. */
  while (cw_keyer_state != KS_IDLE)
    cw_wait_for_signal_internal ();

  return RC_SUCCESS;
}


/**
 * cw_reset_keyer()
 *
 * Clear all keyer latches and paddle states, return to Curtis 8044 Keyer
 * mode A, and return to silence.  This function is suitable for calling from
 * an application exit handler.
 */
void
cw_reset_keyer (void)
{
  cw_ik_dot_paddle = FALSE;
  cw_ik_dash_paddle = FALSE;
  cw_ik_dot_latch = FALSE;
  cw_ik_dash_latch = FALSE;
  cw_ik_curtis_b_latch = FALSE;
  cw_ik_curtis_mode_b = FALSE;

  cw_keyer_state = KS_IDLE;

  /* Silence sound and stop any background soundcard tone generation. */
  cw_sound_internal (TONE_SILENT);
  cw_schedule_finalization_internal ();

  if (cw_is_debugging_internal (CW_DEBUG_KEYER_STATES))
    fprintf (stderr, "cw: keyer ->%d (reset)\n", cw_keyer_state);
}


/*---------------------------------------------------------------------*/
/*  Straight key                                                       */
/*---------------------------------------------------------------------*/

/*
 * Period of constant tone generation after which we need another timeout,
 * to ensure that the soundcard doesn't run out of data.
 */
static const int STRAIGHT_KEY_TIMEOUT = 500000;

/* Straight key status; just a key-up or key-down indication. */
static volatile int cw_sk_key_down = FALSE;


/**
 * cw_straight_key_clock_internal()
 *
 * Soundcard tone data is only buffered to last about a second on each
 * cw_generate_sound_internal() call, and holding down the straight key for
 * longer than this could cause a soundcard data underrun.  To guard against
 * this, a timeout is generated every half-second or so while the straight
 * key is down.  The timeout generates a chunk of sound data for the soundcard.
 */
static void
cw_straight_key_clock_internal (void)
{
  if (cw_sk_key_down)
    {
      /* Generate a quantum of tone data, and request another timeout. */
      cw_generate_sound_internal ();
      cw_request_timeout_internal (STRAIGHT_KEY_TIMEOUT, NULL);
    }
}


/**
 * cw_notify_straight_key_event()
 *
 * Informs the library that the straight key has changed state.  This routine
 * returns TRUE on success.  On error, it returns FALSE, with errno set to
 * EBUSY if the tone queue or iambic keyer are using the sound card, console
 * speaker, or keying control system.  If key_state indicates no change of
 * state, the call is ignored.
 */
int
cw_notify_straight_key_event (int key_state)
{
  /*
   * If the tone queue or the keyer are busy, we can't use the sound card,
   * console sounder, or the key control system.
   */
  if (cw_is_tone_busy () || cw_is_keyer_busy ())
    {
      errno = EBUSY;
      return RC_ERROR;
    }

  /* If the key state did not change, ignore the call. */
  if ((cw_sk_key_down && !key_state) || (!cw_sk_key_down && key_state))
    {
      /* Save the new key state. */
      cw_sk_key_down = (key_state != 0);

      if (cw_is_debugging_internal (CW_DEBUG_STRAIGHT_KEY))
        fprintf (stderr, "cw: straight key state ->%s\n",
                 cw_sk_key_down ? "DOWN" : "UP");

      /*
       * Do tones and keying, and set up timeouts and soundcard activities to
       * match the new key state.
       */
      if (cw_sk_key_down)
        {
          cw_sound_internal (generator->frequency);
          cw_key_control_internal (TRUE);

          /* Start timeouts to keep soundcard tones running. */
          cw_request_timeout_internal (STRAIGHT_KEY_TIMEOUT,
                                       cw_straight_key_clock_internal);
        }
      else
        {
          cw_sound_internal (TONE_SILENT);
          cw_key_control_internal (FALSE);

          /*
           * Indicate that we have finished with timeouts, and also with the
           * soundcard too.  There's no way of knowing when straight keying
           * is completed, so the only thing we can do here is to schedule
           * release on each key up event.
           */
          cw_schedule_finalization_internal ();
        }
    }

  return RC_SUCCESS;
}


/**
 * cw_get_straight_key_state()
 *
 * Returns the current saved state of the straight key; TRUE if the key is
 * down, FALSE if up.
 */
int
cw_get_straight_key_state (void)
{
  return cw_sk_key_down;
}


/**
 * cw_is_straight_key_busy()
 *
 * Returns TRUE if the straight key is busy, FALSE if not.  This routine is
 * just a pseudonym for cw_get_straight_key_state, and exists to fill a hole
 * in the API naming conventions.
 */
int
cw_is_straight_key_busy (void)
{
  return cw_sk_key_down;
}


/**
 * cw_reset_straight_key()
 *
 * Clears the straight key state, and returns to silence.  This function is
 * suitable for calling from an application exit handler.
 */
void
cw_reset_straight_key (void)
{
  cw_sk_key_down = FALSE;

  /* Silence sound and stop any background soundcard tone generation. */
  cw_sound_internal (TONE_SILENT);
  cw_schedule_finalization_internal ();

  if (cw_is_debugging_internal (CW_DEBUG_STRAIGHT_KEY))
    fprintf (stderr, "cw: straight key state ->%s (reset)\n", "UP");
}





#ifdef CWLIB_MAIN

/* for stand-alone testing */
int main(void)
{
	fprintf(stderr, "console:\n");
	cw_generator_new(CW_AUDIO_CONSOLE, NULL);
	cw_reset_send_receive_parameters();
	cw_set_send_speed(22);
	cw_generator_start();

	cw_send_string("morse");
	cw_wait_for_tone_queue();

	cw_generator_stop();
	cw_generator_delete();






	fprintf(stderr, "OSS:\n");
	cw_generator_new(CW_AUDIO_OSS, NULL);
	cw_reset_send_receive_parameters();
	cw_set_send_speed(22);
	cw_generator_start();

	cw_send_string("morse");
	cw_wait_for_tone_queue();

	cw_generator_stop();
	cw_generator_delete();

	return 0;
}

#endif




int cw_oss_open_device(const char *device)
{
	int parameter = 0;
	/* Open the given soundcard device file, for write only. */
	int soundcard = open(device, O_WRONLY);
	if (soundcard == -1) {
		fprintf(stderr, "cw: open ");
		perror(device);
		return RC_ERROR;
        }

	parameter = 0; /* ignored */
	if (ioctl(soundcard, SNDCTL_DSP_SYNC, &parameter) == -1) {
		perror("cw: ioctl SNDCTL_DSP_SYNC");
		close(soundcard);
		return RC_ERROR;
        }

	parameter = 0; /* ignored */
	if (ioctl(soundcard, SNDCTL_DSP_POST, &parameter) == -1) {
		perror("cw: ioctl SNDCTL_DSP_POST");
		close(soundcard);
		return RC_ERROR;
        }

	/* Set the audio format to 8-bit unsigned. */
	parameter = CW_OSS_FORMAT;
	if (ioctl(soundcard, SNDCTL_DSP_SETFMT, &parameter) == -1) {
		perror("cw: ioctl SNDCTL_DSP_SETFMT");
		close(soundcard);
		return RC_ERROR;
        }

	if (parameter != CW_OSS_FORMAT) {
		errno = ERR_NO_SUPPORT;
		perror("cw: sound AFMT_U8 not supported");
		close(soundcard);
		return RC_ERROR;
        }

	/* Set up mono mode - a single audio channel. */
	parameter = CW_OSS_CHANNELS;
	if (ioctl(soundcard, SNDCTL_DSP_CHANNELS, &parameter) == -1) {
		perror("cw: ioctl SNDCTL_DSP_CHANNELS");
		close(soundcard);
		return RC_ERROR;
        }
	if (parameter != CW_OSS_CHANNELS) {
		errno = ERR_NO_SUPPORT;
		perror("cw: sound mono not supported");
		close(soundcard);
		return RC_ERROR;
        }

	/*
	 * Set up a standard sampling rate based on the notional correct value,
	 * and retain the one we actually get in the library variable.
	 */
	cw_sound_sample_rate = CW_SAMPLE_RATE;
	if (ioctl(soundcard, SNDCTL_DSP_SPEED, &cw_sound_sample_rate) == -1) {
		perror("cw: ioctl SNDCTL_DSP_SPEED");
		close(soundcard);
		return RC_ERROR;
        }
	if (cw_sound_sample_rate != CW_SAMPLE_RATE) {
		fprintf(stderr, "cw: dsp sample_rate -> %d\n", cw_sound_sample_rate);
        }



	audio_buf_info bufff;
	if (ioctl(soundcard, SNDCTL_DSP_GETOSPACE, &bufff) == -1) {
		perror("cw: ioctl SNDCTL_DSP_SYNC");
		close(soundcard);
		return RC_ERROR;
        } else {
		/*
		fprintf(stderr, "before:\n");
		fprintf(stderr, "buff.fragments = %d\n", bufff.fragments);
		fprintf(stderr, "buff.fragsize = %d\n", bufff.fragsize);
		fprintf(stderr, "buff.bytes = %d\n", bufff.bytes);
		fprintf(stderr, "buff.fragstotal = %d\n", bufff.fragstotal);
		*/
	}


#if CW_OSS_SET_FRAGMENT
	/*
	 * Live a little dangerously, by trying to set the fragment size of the
	 * card.  We'll try for a relatively short fragment of 128 bytes.  This
	 * gives us a little better granularity over the amounts of audio data
	 * we write periodically to the soundcard output buffer.  We may not get
	 * the requested fragment size, and may be stuck with the default.  The
	 * argument has the format 0xMMMMSSSS - fragment size is 2^SSSS, and
	 * setting 0x7fff for MMMM allows as many fragments as the driver can
	 * support.
	 */
	/* parameter = 0x7fff << 16 | CW_OSS_SETFRAGMENT; */
	parameter = 0x0032 << 16 | CW_OSS_SETFRAGMENT;
	/* parameter = 0x00230006; */
	if (ioctl(soundcard, SNDCTL_DSP_SETFRAGMENT, &parameter) == -1) {
		perror("cw: ioctl SNDCTL_DSP_SETFRAGMENT");
		close(soundcard);
		return RC_ERROR;
        }
	if (cw_is_debugging_internal(CW_DEBUG_SOUND)) {
		fprintf(stderr, "fragment size is %d\n", parameter & 0x0000ffff);
	}

	/* Query fragment size just to get the driver buffers set. */
	if (ioctl(soundcard, SNDCTL_DSP_GETBLKSIZE, &parameter) == -1) {
		perror("cw: ioctl SNDCTL_DSP_GETBLKSIZE");
		close(soundcard);
		return RC_ERROR;
        }

	if (parameter != (1 << CW_OSS_SETFRAGMENT)) {
		fprintf(stderr, "cw: OSS fragment size not set, %d\n", parameter);
        }

#endif
#if CW_OSS_SET_POLICY
	parameter = 5;
	if (ioctl(soundcard, SNDCTL_DSP_POLICY, &parameter) == -1) {
		perror("cw: ioctl SNDCTL_DSP_POLICY");
		close(soundcard);
		return RC_ERROR;
        }
#endif



	if (ioctl(soundcard, SNDCTL_DSP_GETOSPACE, &bufff) == -1) {
		perror("cw: ioctl SNDCTL_DSP_SYNC");
		close(soundcard);
		return RC_ERROR;
        } else {
		/*
		fprintf(stderr, "after:\n");
		fprintf(stderr, "buff.fragments = %d\n", bufff.fragments);
		fprintf(stderr, "buff.fragsize = %d\n", bufff.fragsize);
		fprintf(stderr, "buff.bytes = %d\n", bufff.bytes);
		fprintf(stderr, "buff.fragstotal = %d\n", bufff.fragstotal);
		*/
	}

	/*
	 * Save the opened file descriptor in a library variable.  Do it now
	 * rather than later since the volume setting functions try to use it.
	 */
	generator->audio_sink = soundcard;
#if 0
	/* Set the mixer volume to zero, so the card is silent initially. */
	if (!cw_set_sound_pcm_volume_internal(0)) {
		close(generator->audio_sink);
		generator->audio_sink = -1;
		return RC_ERROR;
        }
#endif
	if (cw_is_debugging_internal(CW_DEBUG_SOUND)) {
		fprintf(stderr, "cw: dsp opened\n");
	}

	/* Note sound as now open for business. */
	generator->sound_system = CW_AUDIO_OSS;

	generator->debug_sink = open("/tmp/cw_file.raw", O_WRONLY | O_NONBLOCK);

	return RC_SUCCESS;
}





int cw_oss_close_device(void)
{
	close(generator->audio_sink);
	generator->audio_sink = -1;

	if (generator->debug_sink != -1) {
		close(generator->debug_sink);
		generator->debug_sink = -1;
	}

	return RC_SUCCESS;
}





int cw_generator_new(int sound_system, const char *device)
{
	generator = (cw_gen_t *) malloc(sizeof (cw_gen_t));
	if (generator == NULL) {
		fprintf(stderr, "cw: malloc\n");
		return RC_ERROR;
	}

	if (sound_system == CW_AUDIO_CONSOLE) {
		/* TODO: check return values */
		cw_set_console_device(device);
		cw_console_open_device(cw_console_device);
		generator->sound_system = sound_system;
		return RC_SUCCESS;
	} else if (sound_system == CW_AUDIO_OSS) {
		/* TODO: check return values */
		cw_set_soundcard_device(device);
		cw_oss_open_device(cw_oss_device);
		generator->sound_system = sound_system;
		return RC_SUCCESS;
	} else {
		cw_generator_delete();
		fprintf(stderr, "cw: unsupported sound system\n");
		return RC_ERROR;
	}
}





void cw_generator_delete(void)
{
	if (generator != NULL) {
		if (generator->sound_system == CW_AUDIO_CONSOLE) {
			cw_console_close_device();
			free(cw_console_device);
			cw_console_device = NULL;
		} else if (generator->sound_system == CW_AUDIO_OSS) {
			cw_oss_close_device();
			free(cw_oss_device);
			cw_oss_device = NULL;
		} else {
			fprintf(stderr, "cw: missed sound system %d\n", generator->sound_system);
		}
		generator->sound_system = CW_AUDIO_NONE;
		free(generator);
		generator = NULL;
	}
	return;
}





int cw_generator_start(void)
{
	generator->frequency = INITIAL_FREQUENCY;
	generator->volume = INITIAL_VOLUME;
	generator->phase_offset = 0.0;
	generator->phase = 0.0;
	generator->sample_rate = CW_SAMPLE_RATE;
	/* both values being zero here means that generator
	   has started working, but its output is silence;
	   set .slope to a positive value to start generating
	   a sound with non-zero amplitude */
	generator->slope = 0;
	generator->amplitude = 0;

	generator->generate = 1;

	if (generator->sound_system == CW_AUDIO_CONSOLE) {
		;
	} else if (generator->sound_system == CW_AUDIO_OSS) {
		int rv = pthread_create(&(generator->thread), NULL,
					cw_oss_generator_write_sinewave,
					(void *) generator);
		if (rv != 0) {
			fprintf(stderr, "ERROR: failed to create generator thread\n");
			return RC_ERROR;
		} else {
			/* for some yet unknown reason you have to
			   put usleep() here, otherwise a generator
			   may work incorrectly */
			usleep(100000);
			return RC_SUCCESS;
		}
	} else {
		fprintf(stderr, "cw: unsupported sound system %d\n", generator->sound_system);
	}

	return RC_SUCCESS;
}



void cw_generator_stop(void)
{
	if (generator->sound_system == CW_AUDIO_CONSOLE) {
		/* sine wave generation should have been stopped
		   by a code generating dots/dashes, but
		   just in case... */
		ioctl(generator->audio_sink, KIOCSOUND, 0);
	} else if (generator->sound_system == CW_AUDIO_OSS) {
		generator->slope = -CW_OSS_GENERATOR_SLOPE;

		/* time needed between initiating stop sequence and
		   ending write() to device and closing the device */
		int usleep_time = CW_SAMPLE_RATE / (2 * CW_OSS_GENERATOR_BUF_SIZE);
		usleep_time /= 1000000;
		usleep(usleep_time * 1.2);

		generator->generate = 0;
	} else {
		;
	}

	return;
}





void *cw_oss_generator_write_sinewave(void *arg)
{
	cw_gen_t *gen = (cw_gen_t *) arg;
	short buf[CW_OSS_GENERATOR_BUF_SIZE];

	while (gen->generate) {

		int i = 0;
		double phase = 0.0;
		/* Create a fragment's worth of shaped wave data. */
		for (i = 0; i < CW_OSS_GENERATOR_BUF_SIZE; i++) {
			phase = (2.0 * M_PI
				 * (double) gen->frequency * (double) i / (double) gen->sample_rate)
				+ gen->phase_offset;
			int amplitude = cw_oss_generator_calculate_amplitude(gen);

			buf[i] = amplitude * sin(phase);
		}

		/* Compute the phase of the last generated sample
		   (or is it phase of first sample in next series?). */
		phase = (2.0 * M_PI * (double) gen->frequency * (double) i / (double) gen->sample_rate) + gen->phase_offset;

		/* Extract the normalized phase offset. */
		int n_periods = floor(phase / (2.0 * M_PI));
		gen->phase_offset = phase - n_periods * 2.0 * M_PI;

		if (write(gen->audio_sink, buf, sizeof (buf)) != sizeof (buf)) {
			perror("Audio write");
			exit(-1);
		}
		write(gen->debug_sink, buf, sizeof (buf));
	} /* while() */

	return NULL;
}





static const long int VOLUME_RANGE = (1 << 15); /* 2^15 = 32768 */


int cw_oss_generator_calculate_amplitude(cw_gen_t *gen)
{
	int volume = (gen->volume * VOLUME_RANGE) / 100;
	if (gen->slope == 0) {
		;
	} else if (gen->slope < 0) {
		if (gen->amplitude > 0) {
			gen->amplitude += gen->slope; /* yes, += */
		} else if (gen->amplitude < 0) {
			gen->amplitude = 0;
			gen->slope = 0;
		} else { /* gen->amplitude == 0 */
			gen->slope = 0;
		}
	} else { /* gen->slope > 0 */
		if (gen->amplitude < volume) {
			gen->amplitude += gen->slope;
		} else if (gen->amplitude > volume) {
			gen->amplitude = volume;
			gen->slope = 0;
		} else { /* gen->amplitude == volume; */
			gen->slope = 0;
		}
	}

	/* because VOLUME_RANGE may not be exact multiple
	   of gen->slope, gen->amplitude may be sometimes out
	   of range; this may produce audible clicks;
	   remove values out of range */
	if (gen->amplitude > VOLUME_RANGE) {
		gen->amplitude = VOLUME_RANGE;
	} else if (gen->amplitude < 0) {
		gen->amplitude = 0;
	} else {
		;
	}

	return gen->amplitude;
}


