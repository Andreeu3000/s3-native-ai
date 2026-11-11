#include "py/obj.h"
#include "py/runtime.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"

STATIC mp_obj_t s3mem_info(void) {
    mp_obj_t dict = mp_obj_new_dict(0);
    size_t free_int = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t total_psram = esp_psram_get_size();
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_free_internal_bytes), mp_obj_new_int(free_int));
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_free_psram_bytes), mp_obj_new_int(free_psram));
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_total_psram_bytes), mp_obj_new_int(total_psram));
    return dict;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(s3mem_info_obj, s3mem_info);
STATIC const mp_rom_map_elem_t s3mem_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_s3mem) },
    { MP_ROM_QSTR(MP_QSTR_info), MP_ROM_PTR(&s3mem_info_obj) },
};
STATIC MP_DEFINE_CONST_DICT(s3mem_module_globals, s3mem_module_globals_table);
const mp_obj_module_t s3mem_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&s3mem_module_globals,
};
MP_REGISTER_MODULE(MP_QSTR_s3mem, s3mem_user_cmodule);
