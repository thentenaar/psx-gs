#ifndef GS_H
#define GS_H
/**
 * Gameshark / Action Replay Plugin for PSEmu compatible emulators
 * Copyright (C) 2008-2013 Tim Hentenaar <tim@hentenaar.com>
 * Released under the GNU General Public License (v2)
 */

#include <stdint.h>

#define VERSION_MAJOR	1
#define VERSION_MINOR	0

/**
 * GS / PAR Code Types
 *
 * Unsupported code types:
 *
 * These aren't supported because they're not really applicable / useful:
 * 	0xC0, 0xC1 - Code activation delays. Not really useful
 *
 * These aren't supported because they rely on buttons being pressed:
 * 	0xD4 - 16-bit next if buttons eq 
 * 	0xD5 - 16-bit all codes on if buttons eq
 * 	0xD6 - 16-bit all codes off if buttons eq
 *
 *
 */
enum {
	GS_NOP            = 0x00,
	GS_WORD_INC_ONCE  = 0x10,
	GS_WORD_DEC_ONCE  = 0x11,
	GS_BYTE_INC_ONCE  = 0x20,
	GS_BYTE_DEC_ONCE  = 0x21,
	GS_BYTE_CONST_WR  = 0x30,
	GS_SLIDE          = 0x50,
	GS_WORD_CONST_WR  = 0x80,
	GS_ALL_CODES_TRIG = 0xC0,
	GS_ACTIVATE_DELAY = 0xC1,
	GS_MEMCOPY        = 0xC2,
	GS_WORD_EQ        = 0xD0,
	GS_WORD_NE        = 0xD1,
	GS_WORD_LT        = 0xD2,
	GS_WORD_GT        = 0xD3,
	GS_NEXT_BUTTON_EQ = 0xD4,
	GS_WORD_CODES_ON  = 0xD5,
	GS_WORD_CODES_OFF = 0xD6,
	GS_BYTE_EQ        = 0xE0,
	GS_BYTE_NE        = 0xE1,
	GS_BYTE_LT        = 0xE2,
	GS_BYTE_GT        = 0xE3
};

/* Code list */
typedef struct code {
	uint32_t address;
	uint16_t value;
	uint8_t  type;
	uint8_t  active;
	char *code;
	char *desc;
	struct code *next;
	struct code *prev;
} gs_code_t;

/* Gameshark plugin state information */
typedef struct state {
	struct gpu { /* The real GPU's functions */
		void *handle;
		int32_t  (*init)();
		int32_t  (*shutdown)();
		int32_t  (*open)(uint32_t);
		int32_t  (*close)();
		int32_t  (*dma_chain)(void *,uint32_t);
		void     (*update_lace)();
		int32_t  (*configure)();
		void     (*about)();
		int32_t  (*test)();
		void     (*make_snapshot)();
		uint32_t (*read_status)();
		void     (*write_status)(uint32_t);
		uint32_t (*read_data)();
		void     (*write_data)(uint32_t);
		void     (*read_data_mem)(void *,int32_t);
		void     (*write_data_mem)(void *,int32_t);
		void     (*display_text)(char *);
		void     (*display_flags)(uint32_t);
		int32_t  (*freeze)(uint32_t,void *);
		void     (*get_screen_pic)(char *);
		void     (*show_screen_pic)(char *);
		void     (*key_pressed)(uint16_t);
		void     (*set_mode)(uint32_t);
		int32_t  (*get_mode)();
	} gpu;

	struct config { /* Config values */
		uint16_t dialog_key;
		uint16_t cheat_toggle_key;
		uint32_t real_gpu;
		uint32_t gpus;
		char **gpu_list;
		char **gpu_paths;
	} config;

	void *base_addr;
	gs_code_t *codes;
} gs_state_t;

#endif	/* GS_H */

/* vi:set ts=4: */
