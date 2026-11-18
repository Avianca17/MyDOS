#include <stdlib.h>
#include <string.h>

#include "doomdef.h"
#include "doomstat.h"
#include "i_net.h"
#include "i_system.h"

void I_InitNetwork(void) {
    doomcom = malloc(sizeof(*doomcom));
    memset(doomcom, 0, sizeof(*doomcom));

    doomcom->ticdup = 1;
    doomcom->extratics = 0;
    doomcom->id = DOOMCOM_ID;
    doomcom->numplayers = 1;
    doomcom->numnodes = 1;
    doomcom->deathmatch = false;
    doomcom->consoleplayer = 0;
    netgame = false;
}

void I_NetCmd(void) {
    I_Error("Networking is not available in MyDOS DOOM.\n");
}
