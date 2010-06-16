#ifndef STATE_H
#define STATE_H

/**
 * Gameshark / Action Replay Plugin for PSEmu compatible emulators
 * (C) 2008 Tim Hentenaar <tim@hentenaar.com>
 * Released under the GNU General Public License (v2)
 */

#include "gs.h"

void serialize_state(gs_state_t *state);
void unserialize_state(gs_state_t *state, int no_gpu_paths);

#endif	/* STATE_H */
