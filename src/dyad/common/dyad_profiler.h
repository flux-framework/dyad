#ifndef DYAD_COMMON_DYAD_PROFILER_H
#define DYAD_COMMON_DYAD_PROFILER_H
#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#ifdef DYAD_PROFILER_DLIO_PROFILER  // DLIO_PROFILER
#include <dlio_profiler/dlio_profiler.h>
#endif
#ifdef DYAD_PROFILER_CALIPER
#include <caliper/cali.h>
#endif

// Number of updates that can be added to a single profiling region.
// Currently here because it's required to have for Caliper support.
#define DYAD_PROFILER_NUM_UPDATES 10

// clang-format off
#ifdef __cplusplus
#ifdef DYAD_PROFILER_NONE
  #define DYAD_CPP_FUNCTION() DYAD_NOOP_MACRO
  #define DYAD_CPP_FUNCTION_UPDATE(key, value) DYAD_NOOP_MACRO
  #define DYAD_CPP_REGION_START(name) DYAD_NOOP_MACRO
  #define DYAD_CPP_REGION_END(name) DYAD_NOOP_MACRO
  #define DYAD_CPP_REGION_UPDATE(name, key, value) DYAD_NOOP_MACRO
#elif defined(DYAD_PROFILER_PERFFLOW_ASPECT)  // PERFFLOW_ASPECT
  #define DYAD_CPP_FUNCTION() DYAD_NOOP_MACRO
  #define DYAD_CPP_FUNCTION_UPDATE(key, value) DYAD_NOOP_MACRO
  #define DYAD_CPP_REGION_START(name) DYAD_NOOP_MACRO
  #define DYAD_CPP_REGION_END(name) DYAD_NOOP_MACRO
  #define DYAD_CPP_REGION_UPDATE(name, key, value) DYAD_NOOP_MACRO
#elif defined(DYAD_PROFILER_CALIPER)  // CALIPER
  #define DYAD_CPP_FUNCTION() CALI_CXX_MARK_FUNCTION;
  #define DYAD_CPP_FUNCTION_UPDATE(key, value) DYAD_CPP_REGION_UPDATE(__func__, (key), (value));
  #define DYAD_CPP_REGION_START(name) CALI_MARK_BEGIN (name);
  #define DYAD_CPP_REGION_END(name) CALI_MARK_END (name);
  // Cannot currently implement this with Caliper because one of the following must be true
  //   * Caliper annotation must begin before region begins and end after region ends
  //   * Caliper annotation must be created with CALI_ATTR_UNALIGNED, and Caliper annotation must
  //     begin after region begins and end after region ends
  // The first bullet is incompatible with how DFTracer/DLIO Profiler works. The second bullet
  // cannot be done with Caliper's C++ RAII wrappers.
  //
  // I've talked with David Boehme about this, and he may add an API to enable this.
  // But, until then, this macro cannot be defined.
  //
  // TODO implement if/when David adds an API that is compatible with these macros
  #define DYAD_CPP_REGION_UPDATE(name, key, value) DYAD_NOOP_MACRO
#elif defined(DYAD_PROFILER_DLIO_PROFILER)  // DLIO_PROFILER
  #define DYAD_CPP_FUNCTION() DLIO_PROFILER_CPP_FUNCTION ()
  #define DYAD_CPP_FUNCTION_UPDATE(key, value) DLIO_PROFILER_CPP_FUNCTION_UPDATE ((key), (value))
  #define DYAD_CPP_REGION_START(name) DLIO_PROFILER_CPP_REGION_START ((name))
  #define DYAD_CPP_REGION_END(name) DLIO_PROFILER_CPP_REGION_END ((name))
  #define DYAD_CPP_REGION_UPDATE(name, key, value) \
      DLIO_PROFILER_CPP_REGION_DYN_UPDATE ((name), (key), (value))
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DYAD_PROFILER_NONE
  #define DYAD_C_REGION_START(name) DYAD_NOOP_MACRO
  #define DYAD_C_REGION_END(name) DYAD_NOOP_MACRO
  #define DYAD_C_REGION_UPDATE_INT(name, key, value) DYAD_NOOP_MACRO
  #define DYAD_C_REGION_UPDATE_STR(name, key, value) DYAD_NOOP_MACRO
  #define DYAD_C_FUNCTION_START() DYAD_NOOP_MACRO
  #define DYAD_C_FUNCTION_END() DYAD_NOOP_MACRO
  #define DYAD_C_FUNCTION_UPDATE_INT(key, value) DYAD_NOOP_MACRO
  #define DYAD_C_FUNCTION_UPDATE_STR(key, value) DYAD_NOOP_MACRO
#elif defined(DYAD_PROFILER_PERFFLOW_ASPECT)  // PERFFLOW_ASPECT
  #define DYAD_C_REGION_START(name) DYAD_NOOP_MACRO
  #define DYAD_C_REGION_END(name) DYAD_NOOP_MACRO
  #define DYAD_C_REGION_UPDATE_INT(name, key, value) DYAD_NOOP_MACRO
  #define DYAD_C_REGION_UPDATE_STR(name, key, value) DYAD_NOOP_MACRO
  #define DYAD_C_FUNCTION_START() DYAD_NOOP_MACRO
  #define DYAD_C_FUNCTION_END() DYAD_NOOP_MACRO
  #define DYAD_C_FUNCTION_UPDATE_INT(key, value) DYAD_NOOP_MACRO
  #define DYAD_C_FUNCTION_UPDATE_STR(key, value) DYAD_NOOP_MACRO
#elif defined(DYAD_PROFILER_CALIPER)  // CALIPER
  #define DYAD_C_REGION_START(name) \
      size_t cali_updated_entries_##name##_capacity = DYAD_PROFILER_NUM_UPDATES; \
      cali_id_t* cali_updated_entries_##name = malloc(cali_updated_entries_##name##_capacity * sizeof(cali_id_t)); \
      size_t num_current_cali_updates_##name = 0; \
      CALI_MARK_BEGIN ((name));
  #define DYAD_C_REGION_END(name) \
      CALI_MARK_END ((name)); \
      for (size_t cali_update_##name##_it = 0; cali_update_##name##_it < num_current_cali_updates_##name; cali_update_##name##_it += 1) { \
        cali_end(cali_updated_entries_##name[cali_update_##name##_it]); \
      } \
      free(cali_updated_entries_##name);
  #define DYAD_C_REGION_UPDATE_INT(name, key, value) \
      if (num_current_cali_updates_##name >= cali_updated_entries_##name##_capacity) { \
        cali_id_t* realloced_buffer_for_region_##name = realloc(cali_updated_entries_##name, 2 * cali_updated_entries_##name##_capacity * sizeof(cali_id_t)); \
        if (realloced_buffer_for_region_##name != NULL) { \
          cali_updated_entries_##name = realloced_buffer_for_region_##name; \
          cali_updated_entries_##name##_capacity *= 2;
        } \
      } \
      if (num_current_cali_updates_##name < cali_updated_entries_##name##_capacity) { \
        cali_id_t cali_attr_region_##name##_name_##key = cali_create_attribute((key), CALI_TYPE_INT, CALI_ATTR_SKIP_EVENTS | CALI_ATTR_UNALIGNED); \
        cali_begin_int(cali_attr_region_##name##_name_##key, (value)); \
        cali_updated_entries_##name[num_current_cali_updates_##name] = cali_attr_region_##name##_name_##key; \
        num_current_cali_updates_##name += 1; \
      }
  #define DYAD_C_REGION_UPDATE_STR(name, key, value) \
      if (num_current_cali_updates_##name >= cali_updated_entires_##name##_capacity) { \
        cali_id_t* realloced_buffer_for_region_##name = realloc(cali_updated_entries_##name, 2 * cali_updated_entries_##name##_capacity * sizeof(cali_id_t)); \
        if (realloced_buffer_for_region_##name != NULL) { \
          cali_updated_entries_##name = realloced_buffer_for_region_##name; \
          cali_updated_entries_##name##_capacity *= 2;
        } \
      } \
      if (num_current_cali_updates_##name < cali_updated_entries_##name##_capacity) { \
        cali_id_t cali_attr_region_##name##_name_##key = cali_create_attribute((key), CALI_TYPE_STRING, CALI_ATTR_SKIP_EVENTS | CALI_ATTR_UNALIGNED); \
        cali_begin_string(cali_attr_region_##name##_name_##key, (value)); \
        cali_updated_entries_##name[num_current_cali_updates_##name] = cali_attr_region_##name##_name_##key; \
        num_current_cali_updates_##name += 1; \
      }
  #define DYAD_C_FUNCTION_START() \
      size_t cali_updated_entries_fn_capacity = DYAD_PROFILER_NUM_UPDATES; \
      cali_id_t* cali_updated_entries_fn = malloc(cali_updated_entries_fn_capacity * sizeof(cali_id_t)); \
      size_t num_current_cali_updates_fn = 0; \
      CALI_MARK_FUNCTION_BEGIN;
  #define DYAD_C_FUNCTION_END() \
      CALI_MARK_FUNCTION_END; \
      for (size_t cali_update_fn_it = 0; cali_update_fn_it < num_current_cali_updates_fn; cali_update_fn_it += 1) { \
        cali_end(cali_updated_entries_fn[cali_update_fn_it]); \
      } \
      free(cali_updated_entries_fn);
  #define DYAD_C_FUNCTION_UPDATE_INT(key, value) \
      if (num_current_cali_updates_fn >= cali_updated_entries_fn_capacity) { \
        cali_id_t* realloced_buffer_for_region_fn = realloc(cali_updated_entries_fn, 2 * cali_updated_entries_fn_capacity * sizeof(cali_id_t)); \
        if (realloced_buffer_for_region_fn != NULL) { \
          cali_updated_entries_fn = realloced_buffer_for_region_fn; \
          cali_updated_entries_fn_capacity *= 2;
        } \
      } \
      if (num_current_cali_updates_fn < cali_updated_entries_fn_capacity) { \
        cali_id_t cali_attr_region_fn_name_##key = cali_create_attribute((key), CALI_TYPE_INT, CALI_ATTR_SKIP_EVENTS | CALI_ATTR_UNALIGNED); \
        cali_begin_int(cali_attr_region_fn_name_##key, (value)); \
        cali_updated_entries_fn[num_current_cali_updates_fn] = cali_attr_region_fn_name_##key; \
        num_current_cali_updates_fn += 1; \
      } 
  #define DYAD_C_FUNCTION_UPDATE_STR(key, value) \
      if (num_current_cali_updates_fn >= cali_updated_entries_fn_capacity) { \
        cali_id_t* realloced_buffer_for_region_fn = realloc(cali_updated_entries_fn, 2 * cali_updated_entries_fn_capacity * sizeof(cali_id_t)); \
        if (realloced_buffer_for_region_fn != NULL) { \
          cali_updated_entries_fn = realloced_buffer_for_region_fn; \
          cali_updated_entries_fn_capacity *= 2;
        } \
      } \
      if (num_current_cali_updates_fn < cali_updated_entries_fn_capacity) { \
        cali_id_t cali_attr_region_fn_name_##key = cali_create_attribute((key), CALI_TYPE_STRING, CALI_ATTR_SKIP_EVENTS | CALI_ATTR_UNALIGNED); \
        cali_begin_string(cali_attr_region_fn_name_##key, (value)); \
        cali_updated_entries_fn[num_current_cali_updates_fn] = cali_attr_region_fn_name_##key; \
        num_current_cali_updates_fn += 1; \
      }
#elif defined(DYAD_PROFILER_DLIO_PROFILER)  // DLIO_PROFILER
  #define DYAD_C_REGION_START(name) DLIO_PROFILER_C_REGION_START ((name))
  #define DYAD_C_REGION_END(name) DLIO_PROFILER_C_REGION_END ((name))
  #define DYAD_C_REGION_UPDATE_INT(name, key, value) \
      DLIO_PROFILER_C_REGION_UPDATE_INT ((name), (key), (value))
  #define DYAD_C_REGION_UPDATE_STR(name, key, value) \
      DLIO_PROFILER_C_REGION_UPDATE_STR ((name), (key), (value))
  #define DYAD_C_FUNCTION_START() DLIO_PROFILER_C_FUNCTION_START ()
  #define DYAD_C_FUNCTION_END() DLIO_PROFILER_C_FUNCTION_END ()
  #define DYAD_C_FUNCTION_UPDATE_INT(key, value) DLIO_PROFILER_C_FUNCTION_UPDATE_INT ((key), (value))
  #define DYAD_C_FUNCTION_UPDATE_STR(key, value) DLIO_PROFILER_C_FUNCTION_UPDATE_STR ((key), (value))
#endif
// clang-format on

#ifdef __cplusplus
}
#endif

#endif  // DYAD_COMMON_DYAD_PROFILER_H
