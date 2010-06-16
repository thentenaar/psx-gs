/**
 * Gameshark / Action Replay Plugin for PSEmu compatible emulators
 * (C) 2008-2010 Tim Hentenaar <tim@hentenaar.com>
 * Released under the GNU General Public License (v2)
 */

#include "gs.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

/** 
 * State serialization
 *
 * These function handle serialization of the state
 * data between the main and child (gui) processes.
 *
 * The state passed in and out via a file, specified via the 
 * STATE_SERIALIZED environment variable to the child process.
 *
 * I don't like this method, but...
 */
static const char *state_filename = ".epsxe/cfg/libgs.state";

void serialize_state(gs_state_t *state) {
	gs_code_t *tmp; FILE *fp; int i,d; uint32_t len; char *tmpc;

	if (!state) return;
	if (!(tmpc = calloc(sizeof(char),strlen(getenv("HOME")) + strlen(state_filename) + 2))) return;
	snprintf(tmpc,strlen(getenv("HOME")) + strlen(state_filename) + 2,"%s/%s",getenv("HOME"),state_filename);
	
	unlink(tmpc);
	if (!(fp = fopen(tmpc,"w+"))) { free(tmpc); return; }
	free(tmpc); tmpc = NULL;

	/* Dialog key, et al. */
	d = fwrite(&state->config.dialog_key,sizeof(uint8_t),1,fp);
	d = fwrite(&state->config.cheat_toggle_key,sizeof(uint8_t),1,fp);
	d = fwrite(&state->config.real_gpu,sizeof(uint32_t),1,fp);
	d = fwrite(&state->config.gpus,sizeof(uint32_t),1,fp);

	/* GPU List and paths */
	if (state->config.gpu_list) {
		i = -1;
		while (state->config.gpu_list[++i]) {
			len = strlen(state->config.gpu_list[i]);
			d = fwrite(&len,sizeof(uint32_t),1,fp);
			d = fwrite(state->config.gpu_list[i],len,1,fp);
		}
	
		i = -1;
		while (state->config.gpu_paths[++i]) {
			len = strlen(state->config.gpu_paths[i]);
			d = fwrite(&len,sizeof(uint32_t),1,fp);
			d = fwrite(state->config.gpu_paths[i],len,1,fp);
		}
	}

	/* Codes */
	len = 0; for (tmp=state->codes;tmp;tmp=tmp->next) len++;
	d = fwrite(&len,sizeof(uint32_t),1,fp);

	for (tmp=state->codes;tmp;tmp=tmp->next) {
		d = fwrite(&tmp->address,sizeof(uint32_t),1,fp);
		d = fwrite(&tmp->value,sizeof(uint16_t),1,fp);
		d = fwrite(&tmp->type,sizeof(uint8_t),1,fp);
		d = fwrite(&tmp->active,sizeof(uint8_t),1,fp);

		len = strlen(tmp->code);
		d = fwrite(&len,sizeof(uint32_t),1,fp);
		if (len > 0) d = fwrite(tmp->code,len,1,fp);

		len = strlen(tmp->desc);
		d = fwrite(&len,sizeof(uint32_t),1,fp);
		if (len > 0) d = fwrite(tmp->desc,len,1,fp);
	}
	
	fclose(fp);
}

void unserialize_state(gs_state_t *state, int no_gpu_paths) {
	gs_code_t *tmp, *tmp2 = NULL; FILE *fp; char *buf,*tmpc; int d; uint32_t i,j,k,ltmp=0;

	if (!state) return;
	if (!(tmpc = calloc(sizeof(char),strlen(getenv("HOME")) + strlen(state_filename) + 2))) return;
	snprintf(tmpc,strlen(getenv("HOME")) + strlen(state_filename) + 2,"%s/%s",getenv("HOME"),state_filename);

	if (!(fp = fopen(tmpc,"rb"))) { free(tmpc); return; }
	free(tmpc); tmpc = NULL;

	/* Dialog key, et al. */
	d = fread(&state->config.dialog_key,sizeof(uint8_t),1,fp);
	d = fread(&state->config.cheat_toggle_key,sizeof(uint8_t),1,fp);
	d = fread(&state->config.real_gpu,sizeof(uint32_t),1,fp);
	if (!no_gpu_paths) {
		d    = fread(&state->config.gpus,sizeof(uint32_t),1,fp);
		ltmp = state->config.gpus;
	} else d = fread(&ltmp,sizeof(uint32_t),1,fp);

	/* GPU List and paths */
	if (state->config.gpus > 0 || ltmp > 0) {
		if (!no_gpu_paths) {
			if (state->config.gpu_paths) {
				i = -1;
				while (state->config.gpu_paths[++i]) {
					memset(state->config.gpu_paths[i],0,strlen(state->config.gpu_paths[i]));
					free(state->config.gpu_paths[i]);
				} 

				memset(state->config.gpu_paths,0,sizeof(char *)*(i+1));
				free(state->config.gpu_paths);
			}
		
			if (state->config.gpu_list) {
				i = -1;
				while (state->config.gpu_list[++i]) {
					memset(state->config.gpu_list[i],0,strlen(state->config.gpu_list[i]));
					free(state->config.gpu_list[i]);
				} 

				memset(state->config.gpu_list,0,sizeof(char *)*(i+1));
				free(state->config.gpu_list);
				i = 0;
			}
	
			state->config.gpu_list  = calloc(sizeof(char *),state->config.gpus+1);
			state->config.gpu_paths = calloc(sizeof(char *),state->config.gpus+1);
			ltmp = state->config.gpus;
		} 

		for (j=0;j<2;j++) {
			for (i=0;i<ltmp;i++) {
				k = 0; d = fread(&k,sizeof(uint32_t),1,fp);
				if (k > 0 && (buf = calloc(1,k+1))) {
					d = fread(buf,k,1,fp);
					if (!no_gpu_paths)
						(j ? state->config.gpu_paths : state->config.gpu_list)[i] = buf;
					else free(buf);
				} else if (k > 0) fseek(fp,k,SEEK_CUR);
			}
		}
	}

	/* Codes */
	for (tmp=state->codes;tmp;tmp=tmp2) {
		if (tmp->desc) {
			memset(tmp->desc,0,strlen(tmp->desc));
			free(tmp->desc);
		}

		if (tmp->code) {
			memset(tmp->code,0,strlen(tmp->code));
			free(tmp->code);
		}

		tmp2 = tmp->next;
		if (tmp->prev) tmp->prev->next = tmp->next;
		if (tmp->next) tmp->next->prev = tmp->prev;
		memset(tmp,0,sizeof(gs_code_t));
		free(tmp);
	} state->codes = NULL;

	if (!feof(fp) && fread(&k,sizeof(uint32_t),1,fp) && k > 0) {
		for (i=0;i<k;i++) {
			tmp = calloc(sizeof(gs_code_t),1);
			d = fread(&tmp->address,sizeof(uint32_t),1,fp);
			d = fread(&tmp->value,sizeof(uint16_t),1,fp);
			d = fread(&tmp->type,sizeof(uint8_t),1,fp);
			d = fread(&tmp->active,sizeof(uint8_t),1,fp);

			j = 0; d = fread(&j,sizeof(uint32_t),1,fp);
			if (j > 0) {
				tmp->code = calloc(sizeof(char),j+1);
				d = fread(tmp->code,j,1,fp);
			}

			j = 0; d = fread(&j,sizeof(uint32_t),1,fp);
			if (j > 0) {
				tmp->desc = calloc(sizeof(char),j+1);
				d = fread(tmp->desc,j,1,fp);
			}
			
			if (!state->codes) {
				state->codes = tmp;
				tmp2 = tmp;
			} else {
				tmp2->next = tmp; 
				tmp->prev = tmp2; 
				tmp2 = tmp;
			}
		}
	}
	
	fclose(fp);
}

/* vi:set ts=4: */
