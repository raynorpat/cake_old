
#ifndef R_MODULES_H
#define R_MODULES_H

void R_Modules_Init(void);
void R_Modules_Restart(void);

void R_RegisterModule(char *name, void(*start)(void), void(*shutdown)(void), void(*newmap)(void));

void R_Modules_Start(void);
void R_Modules_Shutdown(void);
void R_Modules_NewMap(void);

#endif
