/**
 * Gameshark / Action Replay Plugin for PSEmu compatible emulators
 * (C) 2008-2010 Tim Hentenaar <tim@hentenaar.com>
 * Released under the GNU General Public License (v2)
 */

#include "gs.h"
#include "state.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>

/* Our state */
static gs_state_t *state = NULL;
static const char *config_path = ".epsxe/cfg/gsconfig";

/* PSEmu Constants and callbacks */
static const char *libname = "PSX-GS";
static const char *libinfo = "PSX-GS\n"
                             "Copyright (C) 2008-2010 Tim Hentenaar <tim@hentenaar.com>\n"
                             "Licensed under the GNU General Public License (v2)";

const char *PSEgetLibName() {
	return libname;
}

uint32_t PSEgetLibType() {
	return 2;	/* Lib types: 1: CDR, 2: GPU, 3: SPU, 4: PAD? */
}

uint32_t PSEgetLibVersion() {
	return (VERSION_MAJOR << 8) | VERSION_MINOR;
}

const char *GPUgetLibInfos() {
	return libinfo;
}

/**
 *
 * GPU Interface Routines 
 *
 * Most of these routines are wrappers for the real GPU.
 * The few that do anything are where all the magic happens.
 *
 * GPUdmaChain gives us the base address that we need to calculate where
 * to write values.
 *
 * GPUupdateLace allows us to write the values at VBLANK time, thus
 * making codes more accurate. There may be a slight performance hit if 
 * too many codes are active simultaniously...
 *
 * GPUkeypressed is for key press interception.
 */
int32_t GPUinit() {
	void *gpu; DIR *dir; int ret; char *tmp,*tmp2,*naam=NULL; struct dirent *de;
	pthread_attr_t attr;

	if (state) GPUshutdown();
	if (!(state = malloc(sizeof(gs_state_t)))) return -1;

	memset(state,0,sizeof(gs_state_t));
	unserialize_state(state,1);

	/* Probe for GPUs */
	gpu = getenv("HOME");
	tmp = malloc(strlen(gpu) + 17);
	snprintf(tmp,strlen(gpu) + 17,"%s/.epsxe/plugins",gpu);
	if ((dir = opendir(tmp))) {
		while ((de = readdir(dir))) {
			if (!strncmp((de->d_name + strlen(de->d_name) - 3),".so",3) || 
			    (strlen(strchr(de->d_name,'.')) > 4 && !strncmp(strchr(de->d_name,'.'),".so.",4))) {
			    	tmp2 = malloc(strlen(tmp) + strlen(de->d_name) + 2);
				snprintf(tmp2,strlen(tmp) + strlen(de->d_name) + 2,"%s/%s",tmp,de->d_name);

			    	if ((gpu = dlopen(tmp2,RTLD_LAZY))) {
					if (dlsym(gpu,"GPUinit")) {
						naam = ((char *(*)())dlsym(gpu,"PSEgetLibName"))();
						if (strlen(naam) != strlen(libname) ||
						    strncmp(naam,libname,strlen(naam))) {
						    	state->config.gpus++;

						    	if (!state->config.gpu_list) 
								state->config.gpu_list = (char **)calloc(2,sizeof(char *));
							else    state->config.gpu_list = (char **)realloc(state->config.gpu_list,
													  sizeof(char *)*(state->config.gpus + 1));

							if (!state->config.gpu_paths) 
								state->config.gpu_paths = (char **)calloc(2,sizeof(char *));
							else    state->config.gpu_paths = (char **)realloc(state->config.gpu_paths,
													   sizeof(char *)*(state->config.gpus + 1));

							state->config.gpu_list[state->config.gpus-1]  = strdup(naam);
							state->config.gpu_paths[state->config.gpus-1] = strdup(tmp2);
							state->config.gpu_list[state->config.gpus]    = NULL;
							state->config.gpu_paths[state->config.gpus]   = NULL;
						}
					} dlclose(gpu);
				} free(tmp2);
			} 
		} 
		closedir(dir);
	} free(tmp);

	/* Load the real GPU and needed symbols */
	if (!state->config.gpu_list || !state->config.gpu_paths) {
		memset(state,0,sizeof(gs_state_t));
		free(state); state = NULL;
		return -1;
	}

	if (!(gpu = dlopen(state->config.gpu_paths[state->config.real_gpu],RTLD_LAZY))) {
		if (state->config.gpu_paths) {
			ret = -1;
			while (state->config.gpu_paths[++ret]) {
				memset(state->config.gpu_paths[ret],0,strlen(state->config.gpu_paths[ret]));
				free(state->config.gpu_paths[ret]);
			} 

			memset(state->config.gpu_paths,0,sizeof(char *)*(ret+1));
			free(state->config.gpu_paths);
			ret = 0;
		}

		if (state->config.gpu_list) {
			ret = -1;
			while (state->config.gpu_list[++ret]) {
				memset(state->config.gpu_list[ret],0,strlen(state->config.gpu_list[ret]));
				free(state->config.gpu_list[ret]);
			} 

			memset(state->config.gpu_list,0,sizeof(char *)*(ret+1));
			free(state->config.gpu_list);
			ret = 0;
		}

		return -1;
	}

	state->gpu.handle          = gpu;
	state->gpu.init            = (int32_t (*)())dlsym(gpu,"GPUinit");
	state->gpu.shutdown        = (int32_t (*)())dlsym(gpu,"GPUshutdown");
	state->gpu.open            = (int32_t (*)(uint32_t))dlsym(gpu,"GPUopen");
	state->gpu.close           = (int32_t (*)())dlsym(gpu,"GPUclose");
	state->gpu.dma_chain       = (int32_t (*)(void *,uint32_t))dlsym(gpu,"GPUdmaChain");
	state->gpu.update_lace     = (void (*)())dlsym(gpu,"GPUupdateLace");
	state->gpu.configure       = (int32_t (*)())dlsym(gpu,"GPUconfigure");
	state->gpu.about           = (void (*)())dlsym(gpu,"GPUabout");
	state->gpu.test            = (int32_t (*)())dlsym(gpu,"GPUtest");
	state->gpu.make_snapshot   = (void (*)())dlsym(gpu,"GPUmakeSnapshot");
	state->gpu.read_status     = (uint32_t (*)())dlsym(gpu,"GPUreadStatus");
	state->gpu.write_status    = (void (*)(uint32_t))dlsym(gpu,"GPUwriteStatus");
	state->gpu.read_data       = (uint32_t (*)())dlsym(gpu,"GPUreadData");
	state->gpu.write_data      = (void (*)(uint32_t))dlsym(gpu,"GPUwriteData");
	state->gpu.read_data_mem   = (void (*)(void *,int32_t))dlsym(gpu,"GPUreadDataMem");
	state->gpu.write_data_mem  = (void (*)(void *,int32_t))dlsym(gpu,"GPUwriteDataMem");
	state->gpu.display_text    = (void (*)(char *))dlsym(gpu,"GPUdisplayText");
	state->gpu.display_flags   = (void (*)(uint32_t))dlsym(gpu,"GPUdisplayFlags");
	state->gpu.freeze          = (int32_t (*)(uint32_t,void *))dlsym(gpu,"GPUfreeze");
	state->gpu.get_screen_pic  = (void (*)(char *))dlsym(gpu,"GPUgetScreenPic");
	state->gpu.show_screen_pic = (void (*)(char *))dlsym(gpu,"GPUshowScreenPic");
	state->gpu.set_mode        = (void (*)(uint32_t))dlsym(gpu,"GPUsetMode");
	state->gpu.get_mode        = (int32_t (*)())dlsym(gpu,"GPUgetMode");

	/* Init the real GPU */
	if (state->gpu.init) return state->gpu.init();
	return 0;
}

int32_t GPUshutdown() {
	gs_code_t *tmp,*tmp2; char *ctmp; int32_t ret = 0;

	if (state) {
		/* Destroy the list of codes */
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
		}

		/* Destroy config variables */
		if (state->config.gpu_paths) {
			ret = -1;
			while (state->config.gpu_paths[++ret]) {
				memset(state->config.gpu_paths[ret],0,strlen(state->config.gpu_paths[ret]));
				free(state->config.gpu_paths[ret]);
			} 

			memset(state->config.gpu_paths,0,sizeof(char *)*(ret+1));
			free(state->config.gpu_paths);
			ret = 0;
		}

		if (state->config.gpu_list) {
			ret = -1;
			while (state->config.gpu_list[++ret]) {
				memset(state->config.gpu_list[ret],0,strlen(state->config.gpu_list[ret]));
				free(state->config.gpu_list[ret]);
			} 

			memset(state->config.gpu_list,0,sizeof(char *)*(ret+1));
			free(state->config.gpu_list);
			ret = 0;
		}

		/* Shutdown the real GPU and destroy the state */
		if (state->gpu.shutdown) ret = state->gpu.shutdown();
		if (state->gpu.handle)   dlclose(state->gpu.handle);
		memset(state,0,sizeof(gs_state_t));
		free(state); state = NULL;
	}

	return ret;
}

int32_t GPUdmaChain(void *base_addr, uint32_t addr) {
	if (!state) return -1;
	state->base_addr = base_addr;

	if (state->gpu.dma_chain) return state->gpu.dma_chain(base_addr,addr);
	return 0;
}

void GPUupdateLace() {
	gs_code_t *tmp; uint16_t v; int flag = 1, n_flag = 0;
	uint8_t slide_count, i, slide_incr; uint32_t slide_addr; uint16_t slide_val,slide_val_incr;
	if (!state) return;

	if (state->base_addr) {
		/* Apply Codes */
		for (tmp=state->codes;tmp;tmp=tmp->next) {
			if (tmp->active) {
				if (tmp->type >= GS_WORD_EQ) {
					/* Get the value referenced */
					v = *(uint16_t *)(state->base_addr + tmp->address);
					
					/* Check the condition */
					n_flag = flag; flag = 0;
					switch (tmp->type) {
						case GS_WORD_EQ: if (v == tmp->value) flag = 1; break;
						case GS_WORD_NE: if (v != tmp->value) flag = 1; break;
						case GS_WORD_LT: if (v < tmp->value)  flag = 1; break;
						case GS_WORD_GT: if (v > tmp->value)  flag = 1; break;
						case GS_BYTE_EQ: if ((v & 0xff) == (tmp->value & 0xff)) flag = 1; break;
						case GS_BYTE_NE: if ((v & 0xff) != (tmp->value & 0xff)) flag = 1; break;
						case GS_BYTE_LT: if ((v & 0xff) < (tmp->value & 0xff))  flag = 1; break;
						case GS_BYTE_GT: if ((v & 0xff) > (tmp->value & 0xff))  flag = 1; break;
					} flag &= n_flag; n_flag = 0;
				} else if (flag) {
					/* Apply the code */
					v = *(uint16_t *)(state->base_addr + tmp->address);
									
					if (tmp->type == GS_SLIDE) {
						slide_count = (tmp->address >> 8) & 0xff;
						slide_incr  = tmp->address & 0xff;
						slide_val   = tmp->value;
						tmp = tmp->next;
						slide_addr     = tmp->address;
						slide_val_incr = tmp->value;

						for (i=0;i<slide_count;i++) {
							*(uint16_t *)(state->base_addr + slide_addr) = slide_val & 0xffff;
							slide_addr = (slide_addr + slide_incr) & 0xffffff;
							slide_val  = (slide_val + slide_val_incr) & 0xffff;
						}
					} else { /* Check type for field length */
						if (tmp->type >= GS_BYTE_EQ || (tmp->type & 0xf0) == 0x20 || (tmp->type & 0xff) == 0x30) {
							v &= 0xff;
							*(uint8_t *)(state->base_addr + tmp->address) = (tmp->value & 0xff);
						} else  *(uint16_t *)(state->base_addr + tmp->address) = tmp->value;
					}
				} else flag = 1;	
			} else flag = 1;
		}
	}

	if (state->gpu.update_lace) state->gpu.update_lace();
}

#define ENV_VAR_SIZE(X) (strlen(getenv(X)) + strlen(X) + 2)

void GPUkeypressed(uint16_t key) {
	pid_t pid = 0; int status; char dialog_id[14], *disp, *xauth, *path, *home;
	char *nenv[] = { NULL, NULL, NULL, NULL, NULL }; gs_code_t *tmp;

	if (!state) return;

	/* Check Key */
	if (key == state->config.dialog_key) { /* Show cheat dialog */
		serialize_state(state);
		if (!(pid = vfork())) {
			path  = calloc(sizeof(char),strlen(getenv("HOME")) + strlen(config_path) + 2);
			disp  = calloc(sizeof(char),ENV_VAR_SIZE("DISPLAY"));
			xauth = calloc(sizeof(char),ENV_VAR_SIZE("XAUTHORITY"));
			home  = calloc(sizeof(char),ENV_VAR_SIZE("HOME"));

			snprintf(path,strlen(getenv("HOME")) + strlen(config_path) + 2,"%s/%s",getenv("HOME"),config_path);
			snprintf(dialog_id,14,"DIALOG_ID=%d",DIALOG_CHEAT);
			snprintf(disp,ENV_VAR_SIZE("DISPLAY"),"DISPLAY=%s",getenv("DISPLAY"));
			snprintf(xauth,ENV_VAR_SIZE("XAUTHORITY"),"XAUTHORITY=%s",getenv("XAUTHORITY"));
			snprintf(home,ENV_VAR_SIZE("HOME"),"HOME=%s",getenv("HOME"));

			nenv[0] = dialog_id;
			nenv[1] = disp;
			nenv[2] = xauth;
			nenv[3] = home;

			execve(path,NULL,nenv);
			free(path); free(disp); free(xauth); free(home);
			exit(EXIT_FAILURE);
		} else waitpid(pid,&status,0);
		unserialize_state(state,0);
	} else if (key == state->config.cheat_toggle_key) /* Toggle all */
		for (tmp=state->codes;tmp;tmp=tmp->next) tmp->active = (tmp->active ? 0 : 1);
	
	if (state->gpu.key_pressed) state->gpu.key_pressed(key);
}

int32_t GPUconfigure() {
	pid_t pid = 0; int status; char dialog_id[14], *disp, *xauth, *path, *home;
	char *nenv[] = { NULL, NULL, NULL, NULL, NULL };

	/* Attempt to initialize state, and return -1 on failure. */
	if (!state) GPUinit();
	if (!state) return -1;

	serialize_state(state);
	if (!(pid = vfork())) {
		path  = calloc(sizeof(char),strlen(getenv("HOME")) + strlen(config_path) + 2);
		disp  = calloc(sizeof(char),ENV_VAR_SIZE("DISPLAY"));
		xauth = calloc(sizeof(char),ENV_VAR_SIZE("XAUTHORITY"));
		home  = calloc(sizeof(char),ENV_VAR_SIZE("HOME"));

		snprintf(path,strlen(getenv("HOME")) + strlen(config_path) + 2,"%s/%s",getenv("HOME"),config_path);
		snprintf(dialog_id,14,"DIALOG_ID=%d",DIALOG_CHEAT);
		snprintf(disp,ENV_VAR_SIZE("DISPLAY"),"DISPLAY=%s",getenv("DISPLAY"));
		snprintf(xauth,ENV_VAR_SIZE("XAUTHORITY"),"XAUTHORITY=%s",getenv("XAUTHORITY"));
		snprintf(home,ENV_VAR_SIZE("HOME"),"HOME=%s",getenv("HOME"));

		nenv[0] = dialog_id;
		nenv[1] = disp;
		nenv[2] = xauth;
		nenv[3] = home;

		execve(path,NULL,nenv);
		free(path); free(disp); free(xauth); free(home);
		exit(EXIT_FAILURE);
	} else waitpid(pid,&status,0);
	unserialize_state(state,0);
	return 0;
}

void GPUabout() {
	pid_t pid = 0; int status; char dialog_id[14], *disp, *xauth, *path, *home;
	char *nenv[] = { NULL, NULL, NULL, NULL, NULL };
	if (!state) return;

	serialize_state(state);
	if (!(pid = vfork())) {
		path  = calloc(sizeof(char),strlen(getenv("HOME")) + strlen(config_path) + 2);
		disp  = calloc(sizeof(char),ENV_VAR_SIZE("DISPLAY"));
		xauth = calloc(sizeof(char),ENV_VAR_SIZE("XAUTHORITY"));
		home  = calloc(sizeof(char),ENV_VAR_SIZE("HOME"));

		snprintf(path,strlen(getenv("HOME")) + strlen(config_path) + 2,"%s/%s",getenv("HOME"),config_path);
		snprintf(dialog_id,14,"DIALOG_ID=%d",DIALOG_CHEAT);
		snprintf(disp,ENV_VAR_SIZE("DISPLAY"),"DISPLAY=%s",getenv("DISPLAY"));
		snprintf(xauth,ENV_VAR_SIZE("XAUTHORITY"),"XAUTHORITY=%s",getenv("XAUTHORITY"));
		snprintf(home,ENV_VAR_SIZE("HOME"),"HOME=%s",getenv("HOME"));

		nenv[0] = dialog_id;
		nenv[1] = disp;
		nenv[2] = xauth;
		nenv[3] = home;

		execve(path,NULL,nenv);
		free(path); free(disp); free(xauth); free(home);
		exit(EXIT_FAILURE);
	} else waitpid(pid,&status,0);
	unserialize_state(state,0);
}

int32_t GPUtest() {
	if (!state) return -1;
	return 0;
}

/**************** Wrappers that we don't really use. ****************/
int32_t GPUopen(int32_t gpu) {
	if (state && state->gpu.open) return state->gpu.open(gpu);
	return -1;
}

int32_t GPUclose() {
	if (state && state->gpu.close) return state->gpu.close();
	return -1;
}

void GPUmakeSnapshot() {
	if (state && state->gpu.make_snapshot) state->gpu.make_snapshot();
	return;
}

uint32_t GPUreadStatus() {
	if (state && state->gpu.read_status) return state->gpu.read_status();
	return 0;
}

void GPUwriteStatus(uint32_t status) {
	if (state && state->gpu.write_status) state->gpu.write_status(status);
	return;
}

uint32_t GPUreadData() {
	if (state && state->gpu.read_data) return state->gpu.read_data();
	return 0;
}

void GPUwriteData(uint32_t data) {
	if (state && state->gpu.write_data) state->gpu.write_data(data);
	return;
}

void GPUreadDataMem(void *mem,int32_t size) {
	if (state && state->gpu.read_data_mem) return state->gpu.read_data_mem(mem,size);
	return;
}

void GPUwriteDataMem(void *mem,int32_t size) {
	if (state && state->gpu.write_data_mem) return state->gpu.write_data_mem(mem,size);
	return;
}

void GPUdisplayText(char *text) {
	if (state && state->gpu.display_text) state->gpu.display_text(text);
	return;
}

void GPUdisplayFlags(uint32_t flags) {
	if (state && state->gpu.display_flags) return state->gpu.display_flags(flags);
	return;
}

int32_t GPUfreeze(uint32_t freeze_data,void *freeze) {
	if (state && state->gpu.freeze) return state->gpu.freeze(freeze_data,freeze);
	return;
}

void GPUgetScreenPic(char *mem) {
	if (state && state->gpu.get_screen_pic) return state->gpu.get_screen_pic(mem);
	return;
}

void GPUshowScreenPic(char *mem) {
	if (state && state->gpu.show_screen_pic) return state->gpu.show_screen_pic(mem);
	return;
}

void GPUsetMode(uint32_t mode) {
	if (state && state->gpu.set_mode) return state->gpu.set_mode(mode);
	return;
}

int32_t GPUgetMode() {
	if (state && state->gpu.get_mode) return state->gpu.get_mode();
	return 0;
}

/* vi:set ts=4: */
