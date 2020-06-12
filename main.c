#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <vita2d.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>

#define IMAGE_SWITCH_INTERVAL 300000		// in microseconds
// config file for ps4linkcontrols
#define CFG_FILENAME "ux0:ps4linkcontrols.txt"
#define SCREEN_WIDTH 960
#define SCREEN_HEIGHT 544

#define COUNTOF(x) (sizeof(x)/sizeof((x)[0]))

// ??? ps4linkcontrols only accepts [0, 7] - what is 25 for? ps3link?
//static const int keymaps_blacklist[] = { 25 };

static const int keymaps[] = {
	0, 1, 2, 3, 4, 5, 6, 7,
};
static vita2d_texture *keymap_textures[COUNTOF(keymaps)];
static int controller_type = 1;
static vita2d_pgf *pgf;

static void die(const char *format, ...) {
	char buf[4096];
	va_list va;
	va_start(va, format);
	vsnprintf(buf, sizeof(buf), format, va);

	vita2d_start_drawing();
	vita2d_clear_screen();
	vita2d_pgf_draw_text(pgf, 32, 30,
				RGBA8(0,255,0,255), 1.0f,
				buf);
	vita2d_end_drawing();
	vita2d_swap_buffers();

	sceKernelDelayThread(5*1000000);
	sceKernelExitProcess(0);
}

static int load_ps4linkcontrols_config(void) {
	FILE *f = fopen(CFG_FILENAME, "rt");
	if(!f) { return 0; }
	int r = 0;
	fscanf(f, "%d\n", &r);
	fscanf(f, "%d\n", &controller_type);
	fclose(f);

	if(r < 0 || r > 7) {
		r = 0;
	}

	return r;
}

static int save_ps4linkcontrols_config(int ct) {
	FILE *f = fopen(CFG_FILENAME, "wt");
	if(!f) { return 0; }
	if(fprintf(f, "%d\n%d\n", ct, controller_type) <= 0) {
		return 0;
	}
	fclose(f);
	return 1;
}

int main(int argc, char *argv[]) {
	vita2d_init();
	vita2d_set_clear_color(RGBA8(0xff, 0xff, 0xff, 0xff));
	pgf = vita2d_load_default_pgf();

	// load all images
	for(size_t i = 0; i != COUNTOF(keymaps); ++i) {
		char name[64];
		snprintf(name, sizeof(name), "vs0:app/NPXS10013/keymap/%02d/001.png", keymaps[i]);
		vita2d_texture *texture = vita2d_load_PNG_file(name);
		if(!texture) {
			die("vita2d_load_PNG_file(\"%s\") error\n", name);
			return -1;
		}
		keymap_textures[i] = texture;
	}

	const int saved_keymap = load_ps4linkcontrols_config();
	int current_keymap = 0;
	for(size_t i = 0; i != COUNTOF(keymaps); ++i) {
		if(saved_keymap == keymaps[i]) {
			current_keymap = (int)i;
			break;
		}
	}

	uint64_t last_switch_ts = 0;
	while(1) {
		const uint64_t cur_ts = sceKernelGetProcessTimeWide();

		if(cur_ts - last_switch_ts > IMAGE_SWITCH_INTERVAL) {
			int tr = 0;
			SceCtrlData ctrl;
			sceCtrlPeekBufferPositive(0, &ctrl, 1);

			if(ctrl.buttons & (SCE_CTRL_RIGHT | SCE_CTRL_RTRIGGER)) {
				current_keymap++; tr = 1;
			} else if(ctrl.buttons & (SCE_CTRL_LEFT | SCE_CTRL_LTRIGGER)) {
				current_keymap--; tr = 1;
			}

			if(ctrl.buttons & SCE_CTRL_CIRCLE) {
				break;
			}
			if(ctrl.buttons & SCE_CTRL_CROSS) {
				if(!save_ps4linkcontrols_config(keymaps[current_keymap])) {
					die("save_ps4linkcontrols_config error");
				}
				break;
			}

			if(tr) {
				current_keymap %= COUNTOF(keymaps);
				last_switch_ts = cur_ts;
			}
		}

		vita2d_start_drawing();
		vita2d_clear_screen();

		vita2d_draw_texture(keymap_textures[current_keymap], 0.0f, 0.0f);

		vita2d_pgf_draw_text(pgf, 32, SCREEN_HEIGHT-30,
				RGBA8(0,255,0,255), 1.0f,
				"X to save&exit, O to exit");

		char current_keymap_name[8];
		snprintf(current_keymap_name, sizeof(current_keymap_name),
				"%d", keymaps[current_keymap]);
		vita2d_pgf_draw_text(pgf, SCREEN_WIDTH-32, 32,
				RGBA8(0,255,0,255), 1.0f,
				current_keymap_name);

		vita2d_end_drawing();
		vita2d_swap_buffers();
	}

	// cleanup
	vita2d_fini();
	for(size_t i = 0; i != COUNTOF(keymaps); ++i) {
		vita2d_free_texture(keymap_textures[i]);
	}
	vita2d_free_pgf(pgf);
	sceKernelExitProcess(0);
	return 0;
}
