#include "py/obj.h"
#include "py/runtime.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_psram.h"

STATIC mp_obj_t s3sys_info(void) {
    mp_obj_t dict = mp_obj_new_dict(0);
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_model), mp_obj_new_str("ESP32-S3", 8));
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_revision), mp_obj_new_int(chip_info.revision));
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_cores), mp_obj_new_int(chip_info.cores));
    uint32_t flash_size;
    if (esp_flash_get_size(NULL, &flash_size) == ESP_OK) {
        mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_flash_size_mb), mp_obj_new_int(flash_size / (1024 * 1024)));
    }
    if (esp_psram_is_initialized()) {
        mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_psram_size_mb), mp_obj_new_int(esp_psram_get_size() / (1024 * 1024)));
    } else {
        mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_psram_size_mb), mp_obj_new_int(0));
    }
    return dict;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(s3sys_info_obj, s3sys_info);
STATIC const mp_rom_map_elem_t s3sys_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_s3sys) },
    { MP_ROM_QSTR(MP_QSTR_info), MP_ROM_PTR(&s3sys_info_obj) },
};
STATIC MP_DEFINE_CONST_DICT(s3sys_module_globals, s3sys_module_globals_table);
const mp_obj_module_t s3sys_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&s3sys_module_globals,
};
MP_REGISTER_MODULE(MP_QSTR_s3sys, s3sys_user_cmodule);
