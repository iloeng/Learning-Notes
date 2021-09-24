/* Tcl in ~ 500 lines of code by Salvatore antirez Sanfilippo. BSD licensed */

enum {PICOL_OK, PICOL_ERR, PICOL_RETURN, PICOL_BREAK, PICOL_CONTINUE};
enum {PT_ESC,PT_STR,PT_CMD,PT_VAR,PT_SEP,PT_EOL,PT_EOF};

struct picolParser {
    char *text;
    char *p; /* current text position */
    int len; /* remaining length */
    char *start; /* token start */
    char *end; /* token end */
    int type; /* token type, PT_... */
    int insidequote; /* True if inside " " */
};

struct picolVar {
    char *name, *val;
    struct picolVar *next;
};

struct picolInterp; /* forward declaration */
typedef int (*picolCmdFunc)(struct picolInterp *i, int argc, char **argv, void *privdata);

struct picolCmd {
    char *name;
    picolCmdFunc func;
    void *privdata;
    struct picolCmd *next;
};

struct picolCallFrame {
    struct picolVar *vars;
    struct picolCallFrame *parent; /* parent is NULL at top level */
};

struct picolInterp {
    int level; /* Level of nesting */
    struct picolCallFrame *callframe;
    struct picolCmd *commands;
    char *result;
};

/* Exported API */
void picolInitInterp(struct picolInterp *i);
void picolSetResult(struct picolInterp *i, char *s);
struct picolVar *picolGetVar(struct picolInterp *i, char *name);
int picolSetVar(struct picolInterp *i, char *name, char *val);
int picolRegisterCommand(struct picolInterp *i, char *name, picolCmdFunc f, void *privdata);
int picolEval(struct picolInterp *i, char *t);
