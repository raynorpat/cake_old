// cmdlib.h

#ifndef __CMDLIB__
#define __CMDLIB__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>

#if _MSC_VER
# pragma warning(disable : 4996)
#endif

#ifndef __BYTEBOOL__
#define __BYTEBOOL__
typedef enum {false, true} qboolean;
typedef unsigned char byte;
#endif

// the dec offsetof macro doesn't work very well...
#define myoffsetof(type,identifier) ((size_t)&((type *)0)->identifier)


// set these before calling CheckParm
extern int myargc;
extern char **myargv;

int Q_strncasecmp (char *s1, char *s2, int n);
int Q_strcasecmp (char *s1, char *s2);

int Q_filelength (FILE *f);

double I_FloatTime (void);

void	Error (char *error, ...);
int		CheckParm (char *check);
void ParseCommandLine (char *lpCmdLine);

FILE	*SafeOpenWrite (char *filename);
FILE	*SafeOpenRead (char *filename);
void	SafeRead (FILE *f, void *buffer, int count);
void	SafeWrite (FILE *f, void *buffer, int count);

int		LoadFile (char *filename, void **bufferptr);
int		LoadFileNoCrash (char *filename, void **bufferptr);
void	SaveFile (char *filename, void *buffer, int count);

void 	DefaultExtension (char *path, char *extension);
void 	DefaultPath (char *path, char *basepath);
void 	StripFilename (char *path);
void 	StripExtension (char *path);

void 	ExtractFilePath (char *path, char *dest);
void	ExtractFileName (char *path, char *dest);
void 	ExtractFileBase (char *path, char *dest);
void	ExtractFileExtension (char *path, char *dest);

int 	ParseNum (char *str);

void	StringTolower (char *string);
void	StringToupper (char *string);

short	BigShort (short l);
short	LittleShort (short l);
int		BigLong (int l);
int		LittleLong (int l);
float	BigFloat (float l);
float	LittleFloat (float l);


char *COM_Parse (char *data);

extern	char		com_token[1024];
extern	qboolean	com_eof;

#define	MAX_NUM_ARGVS	32
extern	int		argc;
extern	char	*argv[MAX_NUM_ARGVS];

#endif
