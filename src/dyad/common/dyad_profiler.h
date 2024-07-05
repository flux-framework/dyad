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
  #define DYAD_CPP_FUNCTION_UPDATE(key, value) ;
  #define DYAD_CPP_REGION_START(name) CALI_MARK_BEGIN (name);
  #define DYAD_CPP_REGION_END(name) CALI_MARK_END (name);
  #define DYAD_CPP_REGION_UPDATE(name, key, value)            \
      cali::Annotation::Guard cali_update_guard_##name_##key{ \
          cali::Annotation ((key), CALI_ATTR_SKIP_EVENTS).begin ((value))};
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
      size_t cali_updated_entries_##name_capacity = DYAD_PROFILER_NUM_UPDATES; \
      char** cali_updated_entries_##name = malloc(cali_updated_entries_##name_capacity * sizeof(char*)); \
      size_t num_current_cali_updates_##name = 0; \
      CALI_MARK_BEGIN ((name));
  #define DYAD_C_REGION_END(name) \
      for (size_t cali_update_##name_it = 0; cali_update_##name_it < num_current_cali_updates_##name; cali_update_##name_it += 1) { \
        cali_end_byname(cali_updated_entries_##name[cali_update_##name_it]); \
      } \
      CALI_MARK_END ((name)); \
      free(cali_updated_entries_##name);
  #define DYAD_C_REGION_UPDATE_INT(name, key, value) \
      if (num_current_cali_updates_##name >= cali_updated_entires_##name_capacity) { \
        char** realloced_buffer_for_region_##name = realloc(cali_updated_entries_##name, 2 * cali_updated_entries_##name_capacity * sizeof(char*)); \
        if (realloced_buffer_for_region_##name != NULL) { \
          cali_updated_entries_##name = realloced_buffer_for_region_##name; \
          cali_updated_entries_##name_capacity *= 2;
        } \
      } \
      if (num_current_cali_updates_##name < cali_updated_entries_##name_capacity) { \
        cali_begin_int_byname((key), (value)); \
        cali_updated_entries_##name[num_current_cali_updates_##name] = (key); \
        num_current_cali_updates_##name += 1; \
      }
  #define DYAD_C_REGION_UPDATE_STR(name, key, value) \
      if (num_current_cali_updates_##name >= cali_updated_entires_##name_capacity) { \
        char** realloced_buffer_for_region_##name = realloc(cali_updated_entries_##name, 2 * cali_updated_entries_##name_capacity * sizeof(char*)); \
        if (realloced_buffer_for_region_##name != NULL) { \
          cali_updated_entries_##name = realloced_buffer_for_region_##name; \
          cali_updated_entries_##name_capacity *= 2;
        } \
      } \
      if (num_current_cali_updates_##name < cali_updated_entries_##name_capacity) { \
        cali_begin_string_byname((key), (value)); \
        cali_updated_entries_##name[num_current_cali_updates_##name] = (key); \
        num_current_cali_updates_##name += 1; \
      }
  #define DYAD_C_FUNCTION_START() DYAD_C_REGION_START(__func__)
  #define DYAD_C_FUNCTION_END() DYAD_C_REGION_END(__func__)
  #define DYAD_C_FUNCTION_UPDATE_INT(key, value) \
      DYAD_C_REGION_UPDATE_INT(__func__, (key), (value)) 
  #define DYAD_C_FUNCTION_UPDATE_STR(key, value) \
      DYAD_C_REGION_UPDATE_STR(__func__, (key), (value))
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
