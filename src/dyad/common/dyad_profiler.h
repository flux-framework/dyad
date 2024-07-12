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
#include <stdlib.h>

#ifdef __cplusplus
#include <string>
#include <vector>
#endif
#endif

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
namespace dyad {

class CaliperEntryList
{
  public:

    template <typename T>
    void update(const std::string& key, T& value)
    {
      update(key.c_str(), value);
    }
    
    template <typename T>
    void update(const char* key, T& value)
    {
      cali::Annotation* entry = new cali::Annotation(key, CALI_ATTR_SKIP_EVENTS | CALI_ATTR_UNALIGNED);
      entry->begin(value);
      m_entries.push_back(entry);
    }
    
  protected:

    std::vector<cali::Annotation*> m_entries;
};

class CaliperFunction : public CaliperEntryList
{
  public:

    CaliperFunction(const char* name)
      : m_cali_func(new cali::Function(name))
    {
    }
    
    CaliperFunction(const std::string& name)
      : CaliperFunction(name.c_str())
    {
    }
    
    ~CaliperFunction()
    {
      end();
    }
    
    void end()
    {
      if (m_cali_func != NULL) {
        delete m_cali_func;
      }
      for (size_t i = 0; i < m_entries.size(); i++) {
        if (m_entries[i] != NULL) {
          m_entries[i]->end();
          delete m_entries[i];
          m_entries[i] = NULL;
        }
      }
      m_entries.clear();
    }
    
  private:

    cali::Function* m_cali_func;

};

class CaliperRegion : public CaliperEntryList
{
  public:

    CaliperRegion(const char* name)
      : m_cali_region(new cali::ScopeAnnotation(name))
    {
    }
    
    CaliperRegion(const std::string& name)
      : CaliperRegion(name.c_str())
    {
    }
    
    ~CaliperRegion()
    {
      end();
    }
    
    void end()
    {
      if (m_cali_region != NULL) {
        delete m_cali_region;
      }
      for (size_t i = 0; i < m_entries.size(); i++) {
        if (m_entries[i] != NULL) {
          m_entries[i]->end();
          delete m_entries[i];
          m_entries[i] = NULL;
        }
      }
      m_entries.clear();
    }
    
  private:

    cali::ScopeAnnotation* m_cali_region;

};

}  // namespace dyad

  #define DYAD_CPP_FUNCTION() dyad::CaliperFunction dyad_cali_fn_region{__func__}
  #define DYAD_CPP_FUNCTION_UPDATE(key, value) dyad_cali_fn_region.update((key), (value))
  #define DYAD_CPP_REGION_START(name) dyad::CaliperRegion* dyad_cali_region_##name = new dyad::CaliperRegion(#name)
  #define DYAD_CPP_REGION_END(name) delete dyad_cali_region_##name
  #define DYAD_CPP_REGION_UPDATE(name, key, value) dyad_cali_region_##name->update((key), (value))
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
  struct dyad_profiler_caliper_region_entries {
    cali_id_t* entries;
    size_t size;
    size_t capacity;
  };

  typedef struct dyad_profiler_caliper_region_entries dyad_profiler_caliper_region_entries_t;
  
  static inline dyad_profiler_caliper_region_entries_t* dyad_profiler_caliper_region_start(const char* region_name)
  {
    dyad_profiler_caliper_region_entries_t* entry_list = (dyad_profiler_caliper_region_entries_t*) malloc(sizeof(struct dyad_profiler_caliper_region_entries));
    if (entry_list == NULL) {
      return NULL;
    }
    entry_list->capacity = 10;
    entry_list->size = 0;
    entry_list->entries = (cali_id_t*) malloc(entry_list->capacity * sizeof(cali_id_t));
    if (entry_list->entries == NULL) {
      free(entry_list);
      entry_list = NULL;
    }
    CALI_MARK_BEGIN(region_name);
    return entry_list;
  }
  
  static inline void dyad_profiler_caliper_region_end(dyad_profiler_caliper_region_entries_t** entry_list, const char* region_name)
  {
    CALI_MARK_END(region_name);
    if (entry_list != NULL && *entry_list != NULL) {
      if ((*entry_list)->entries != NULL) {
        for (size_t i = 0; i < (*entry_list)->size; i++) {
          cali_end((*entry_list)->entries[i]);
        }
        free((*entry_list)->entries);
      }
      free(*entry_list);
      *entry_list = NULL;
    }
  }
  
  static inline void dyad_profiler_caliper_region_update_int(dyad_profiler_caliper_region_entries_t* entry_list, const char* key, int value)
  {
    if (entry_list != NULL) {
      if (entry_list->size >= entry_list->capacity) {
        size_t new_capacity = 2 * entry_list->capacity;
        cali_id_t* realloced_cali_id_buf = (cali_id_t*) realloc(entry_list->entries, new_capacity * sizeof(cali_id_t));
        if (realloced_cali_id_buf != NULL) {
          entry_list->entries = realloced_cali_id_buf;
          entry_list->capacity = new_capacity;
        }
      }
      if (entry_list->size < entry_list->capacity) {
        cali_id_t cali_attr_id = cali_create_attribute(key, CALI_TYPE_INT, CALI_ATTR_SKIP_EVENTS | CALI_ATTR_UNALIGNED);
        cali_begin_int(cali_attr_id, value);
        entry_list->entries[entry_list->size] = cali_attr_id;
        entry_list->size += 1;
      }
    }
  }
  
  static inline void dyad_profiler_caliper_region_update_string(dyad_profiler_caliper_region_entries_t* entry_list, const char* key, const char* value)
  {
    if (entry_list != NULL) {
      if (entry_list->size >= entry_list->capacity) {
        size_t new_capacity = 2 * entry_list->capacity;
        cali_id_t* realloced_cali_id_buf = (cali_id_t*) realloc(entry_list->entries, new_capacity * sizeof(cali_id_t));
        if (realloced_cali_id_buf != NULL) {
          entry_list->entries = realloced_cali_id_buf;
          entry_list->capacity = new_capacity;
        }
      }
      if (entry_list->size < entry_list->capacity) {
        cali_id_t cali_attr_id = cali_create_attribute(key, CALI_TYPE_STRING, CALI_ATTR_SKIP_EVENTS | CALI_ATTR_UNALIGNED);
        cali_begin_string(cali_attr_id, value);
        entry_list->entries[entry_list->size] = cali_attr_id;
        entry_list->size += 1;
      }
    }
  }
                                      
  #define DYAD_C_REGION_START(name) \
      dyad_profiler_caliper_region_entries_t* dyad_caliper_entries_for_region_##name = dyad_profiler_caliper_region_start(#name);
  #define DYAD_C_REGION_END(name) \
      dyad_profiler_caliper_region_end(&dyad_caliper_entries_for_region_##name, #name);
  #define DYAD_C_REGION_UPDATE_INT(name, key, value) \
      dyad_profiler_caliper_region_update_int(dyad_caliper_entries_for_region_##name, key, value);
  #define DYAD_C_REGION_UPDATE_STR(name, key, value) \
      dyad_profiler_caliper_region_update_string(dyad_caliper_entries_for_region_##name, key, value);
  #define DYAD_C_FUNCTION_START() \
      dyad_profiler_caliper_region_entries_t* dyad_caliper_entries_fn = dyad_profiler_caliper_region_start(__func__);
  #define DYAD_C_FUNCTION_END() \
      dyad_profiler_caliper_region_end(&dyad_caliper_entries_fn, __func__);
  #define DYAD_C_FUNCTION_UPDATE_INT(key, value) \
      dyad_profiler_caliper_region_update_int(dyad_caliper_entries_fn, key, value);
  #define DYAD_C_FUNCTION_UPDATE_STR(key, value) \
      dyad_profiler_caliper_region_update_string(dyad_caliper_entries_fn, key, value);
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
