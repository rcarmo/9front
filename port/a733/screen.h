/* screen.h — declarations for screen/cursor/mouse subsystem */
extern Memimage *gscreen;
extern void* screeninit(int, int, int);
extern void flushmemscreen(Rectangle);

extern void swcursorinit(void);
extern void swcursorhide(int);
extern void swcursordraw(Point);
extern void swcursorload(Cursor*);
extern void swcursoravoid(Rectangle);

extern void mousetrack(int, int, int, ulong);
extern void absmousetrack(int, int, int, ulong);
extern QLock drawlock;

extern void mousectl(Cmdbuf*);
extern void cursoron(void);
extern void cursoroff(void);
extern void setcursor(Cursor*);
extern void blankscreen(int);

extern void getcolor(ulong, ulong*, ulong*, ulong*);
extern int  setcolor(ulong, ulong, ulong, ulong);
extern Memdata* attachscreen(Rectangle*, ulong*, int*, int*, int*);
#define ishwimage(i) 1  /* for ../port/devdraw.c */
extern void mouseredraw(void);
