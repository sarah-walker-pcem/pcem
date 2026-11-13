#include <pcem/defines.h>
#include <pcem/devices.h>
#include <pcem/logging.h>
#include <string.h>
#include <pcem/config.h>

extern void *device_priv[256];
extern device_t *devices[DEV_MAX];
extern device_t *current_device;
extern char *current_device_name;

extern struct device_t *model_getdevice(int model);

int model;
device_t *devices[DEV_MAX];
MODEL *models[ROM_MAX];
VIDEO_CARD *video_cards[GFX_MAX];
SOUND_CARD *sound_cards[SOUND_MAX];
HDD_CONTROLLER *hdd_controllers[HDDCONTROLLERS_MAX];
NETWORK_CARD *network_cards[NETWORK_CARD_MAX];
LPT_DEVICE *lpt_devices[LPT_MAX];
mouse_t *mouse_list[MOUSE_MAX];
joystick_if_t *joystick_list[JOYSTICK_MAX];
device_t *current_device;
char *current_device_name = NULL;
void *device_priv[DEV_MAX];

void (*_sound_speed_changed)();

device_t *model_getdevice(int model) { return models[model]->device; }

device_t *pcem_model_getdevice(int model) {
        if (model < 0 || model >= ROM_MAX || !models[model])
                return NULL;
        return models[model]->device;
}

void device_init() { memset(devices, 0, sizeof(devices)); }

void device_add(device_t *d) { pcem_add_device(d); }

void device_close_all() {
        int c;

        for (c = 0; c < 256; c++) {
                if (devices[c] != NULL) {
                        if (devices[c]->close != NULL)
                                devices[c]->close(device_priv[c]);
                        devices[c] = device_priv[c] = NULL;
                }
        }
}

int device_available(device_t *d) {
#ifdef RELEASE_BUILD
        if (d->flags & DEVICE_NOT_WORKING)
                return 0;
#endif
        if (d->available)
                return d->available();

        return 1;
}

void device_speed_changed() {
        int c;

        for (c = 0; c < 256; c++) {
                if (devices[c] != NULL) {
                        if (devices[c]->speed_changed != NULL) {
                                devices[c]->speed_changed(device_priv[c]);
                        }
                }
        }

        _sound_speed_changed();
}

void device_force_redraw() {
        int c;

        for (c = 0; c < 256; c++) {
                if (devices[c] != NULL) {
                        if (devices[c]->force_redraw != NULL) {
                                devices[c]->force_redraw(device_priv[c]);
                        }
                }
        }
}

void device_add_status_info(char *s, int max_len) {
        int c;

        for (c = 0; c < 256; c++) {
                if (devices[c] != NULL) {
                        if (devices[c]->add_status_info != NULL)
                                devices[c]->add_status_info(s, max_len, device_priv[c]);
                }
        }
}

int device_get_config_int(char *s) {
        device_config_t *config = current_device->config;

        while (config->type != -1) {
                if (!strcmp(s, config->name))
                        return config_get_int(CFG_MACHINE, current_device->name, s, config->default_int);

                config++;
        }
        return 0;
}

char *device_get_config_string(char *s) {
        device_config_t *config = current_device->config;

        while (config->type != -1) {
                if (!strcmp(s, config->name))
                        return config_get_string(CFG_MACHINE, current_device->name, s, config->default_string);

                config++;
        }
        return NULL;
}

int model_get_config_int(char *s) {
        device_t *device = model_getdevice(model);
        device_config_t *config;

        if (!device)
                return 0;

        config = device->config;

        while (config->type != -1) {
                if (!strcmp(s, config->name))
                        return config_get_int(CFG_MACHINE, device->name, s, config->default_int);

                config++;
        }
        return 0;
}

char *model_get_config_string(char *s) {
        device_t *device = model_getdevice(model);
        device_config_t *config;

        if (!device)
                return 0;

        config = device->config;

        while (config->type != -1) {
                if (!strcmp(s, config->name))
                        return config_get_string(CFG_MACHINE, device->name, s, config->default_string);

                config++;
        }
        return NULL;
}

int pcem_device_get_config_int(device_t *device, char *s) {
        device_config_t *config = device->config;

        while (config->type != -1) {
                if (!strcmp(s, config->name))
                        return config_get_int(CFG_MACHINE, device->name, s, config->default_int);

                config++;
        }
        return 0;
}

char *pcem_device_get_config_string(device_t *device, char *s) {
        device_config_t *config = device->config;

        while (config->type != -1) {
                if (!strcmp(s, config->name))
                        return config_get_string(CFG_MACHINE, device->name, s, config->default_string);

                config++;
        }
        return NULL;
}

int pcem_model_get_config_int(device_t *device, char *s) {
        device_config_t *config;

        if (!device)
                return 0;

        config = device->config;

        while (config->type != -1) {
                if (!strcmp(s, config->name))
                        return config_get_int(CFG_MACHINE, device->name, s, config->default_int);

                config++;
        }
        return 0;
}

char *pcem_model_get_config_string(device_t *device, char *s) {
        device_config_t *config;

        if (!device)
                return 0;

        config = device->config;

        while (config->type != -1) {
                if (!strcmp(s, config->name))
                        return config_get_string(CFG_MACHINE, device->name, s, config->default_string);

                config++;
        }
        return NULL;
}

int model_count() {
        int ret = 0;

        while (ret < ROM_MAX && models[ret] != NULL)
                ret++;

        return ret;
}

void pcem_add_model(MODEL *model) {
        int c = model_count();

        if (c >= ROM_MAX - 1) {
                error("pcem_add_model : too many entries, ignoring %s\n", model->name);
                return;
        }
        models[c] = model;
}

int video_count() {
        int ret = 0;

        while (ret < GFX_MAX && video_cards[ret] != NULL)
                ret++;

        return ret;
}

void pcem_add_video(VIDEO_CARD *video) {
        int c = video_count();

        if (c >= GFX_MAX - 1) {
                error("pcem_add_video : too many entries, ignoring %s\n", video->name);
                return;
        }
        video_cards[c] = video;
}

int lpt_count() {
        int ret = 0;

        while (ret < LPT_MAX && lpt_devices[ret] != NULL)
                ret++;

        return ret;
}

void pcem_add_lpt(LPT_DEVICE *lpt) {
        int c = lpt_count();

        if (c >= LPT_MAX - 1) {
                error("pcem_add_lpt : too many entries, ignoring %s\n", lpt->name);
                return;
        }
        lpt_devices[c] = lpt;
}

int sound_count() {
        int ret = 0;

        while (ret < SOUND_MAX && sound_cards[ret] != NULL)
                ret++;

        return ret;
}

void pcem_add_sound(SOUND_CARD *sound) {
        int c = sound_count();

        if (c >= SOUND_MAX - 1) {
                error("pcem_add_sound : too many entries, ignoring %s\n", sound->name);
                return;
        }
        sound_cards[c] = sound;
}

int hdd_controller_count() {
        int ret = 0;

        while (ret < HDDCONTROLLERS_MAX && hdd_controllers[ret] != NULL)
                ret++;

        return ret;
}

void pcem_add_hddcontroller(HDD_CONTROLLER *hddcontroller) {
        int c = hdd_controller_count();

        if (c >= HDDCONTROLLERS_MAX - 1) {
                error("pcem_add_hddcontroller : too many entries, ignoring %s\n", hddcontroller->name);
                return;
        }
        hdd_controllers[c] = hddcontroller;
}

int network_card_count() {
        int ret = 0;

        while (ret < NETWORK_CARD_MAX && network_cards[ret] != NULL)
                ret++;

        return ret;
}

void pcem_add_networkcard(NETWORK_CARD *netcard) {
        int c = network_card_count();

        if (c >= NETWORK_CARD_MAX - 1) {
                error("pcem_add_networkcard : too many entries, ignoring %s\n", netcard->name);
                return;
        }
        network_cards[c] = netcard;
}

int mouse_count() {
        int ret = 0;

        while (ret < MOUSE_MAX && mouse_list[ret] != NULL)
                ret++;

        return ret;
}

void pcem_add_mouse(mouse_t *mouse) {
        int c = mouse_count();

        if (c >= MOUSE_MAX - 1) {
                error("pcem_add_mouse : too many entries, ignoring %s\n", mouse->name);
                return;
        }
        mouse_list[c] = mouse;
}

int joystick_count() {
        int ret = 0;

        while (ret < JOYSTICK_MAX && joystick_list[ret] != NULL)
                ret++;

        return ret;
}

void pcem_add_joystick(joystick_if_t *joystick) {
        int c = joystick_count();

        if (c >= JOYSTICK_MAX - 1) {
                error("pcem_add_joystick : too many entries, ignoring %s\n", joystick->name);
                return;
        }
        joystick_list[c] = joystick;
}

void pcem_add_device(device_t *d) {
        int c = 0;
        void *priv = NULL;

        while (c < DEV_MAX && devices[c] != NULL)
                c++;

        if (c >= DEV_MAX)
                fatal("device_add : too many devices\n");

        current_device = d;
        current_device_name = d->name;

        if (d->init != NULL) {
                priv = d->init();
                if (priv == NULL)
                        fatal("device_add : device init failed\n");
        }

        devices[c] = d;
        device_priv[c] = priv;
        current_device_name = NULL;
}
