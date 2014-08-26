#ifndef EXPFW_H_ 
#define EXPFW_H_ 

#include <map>
#include <vector>
#include <string>
#include <fstream>
#include <stdlib.h>
#include <boost/algorithm/string/trim.hpp>
#include <boost/functional/hash.hpp>

#include "glog/logging.h"

using std::string;
using std::pair;
using std::map;
using std::vector;
using std::ifstream;
using std::endl;

namespace exp_flag {

typedef int FlagID;
const FlagID INVALID_FLAG_ID = -1; 


enum FlagValueType {
    FV_TYPE_BOOL = 0,
    FV_TYPE_INT32 = 1,
    FV_TYPE_UINT32 = 2,
    FV_TYPE_FLOAT = 3,
    FV_TYPE_MAX_INDEX = 3,
};

typedef union {
    bool _bool_value;
    int32_t _int32_value;
    uint32_t _uint32_value;
    float _float_value;
    const char* _raw_value;
} FlagValue;


///////////////////////////////////////////////////////////////////
// Define several type_traits here for type-related functions.
///////////////////////////////////////////////////////////////////
template<class T>
struct type_traits{ };

template<>
struct type_traits<bool> {
    typedef bool value_type;
    static FlagValueType to_type() { return FV_TYPE_BOOL; }
    static FlagValue value_to_flag_value(value_type value)
    {
        FlagValue ret;
        ret._bool_value = value;
        return ret;
    }
    static value_type flag_value_to_value(FlagValue fv)
    {
        return fv._bool_value;
    }
    static value_type default_value() { return false; }
    static FlagValue raw_to_flag_value(const char* v)
    {
        FlagValue ret;
        if (strcmp(v, "true") == 0 || strcmp(v, "1") == 0)
        {
            ret._bool_value = true;
        } 
        else if (strcmp(v, "false") == 0 || strcmp(v, "0") == 0)
        {
            ret._bool_value = false;
        }
        else 
        {
            ret._bool_value = default_value(); // does not match usual bool values
        }
        return ret;  
    }
};

template<>
struct type_traits<int32_t> {
    typedef int32_t value_type;
    static FlagValueType to_type() { return FV_TYPE_INT32; }
    static FlagValue value_to_flag_value(value_type value)
    {
        FlagValue ret;
        ret._int32_value = value;
        return ret;
    }
    static value_type flag_value_to_value(FlagValue fv)
    {
        return fv._int32_value;
    }
    static value_type default_value() { return 0; }
    static FlagValue raw_to_flag_value(const char* v)
    {
        FlagValue ret;
        if (NULL == v) 
        {
            ret._int32_value = default_value();
            return ret;
        }
        char *end = NULL;
        errno = 0;
        ret._int32_value = static_cast<int32_t>(strtoll(v, &end, 10));
        // if parse fails, then set value as default value
        if (errno || end != v + strlen(v))
        {
            ret._int32_value = default_value();
        }
        return ret;
    }
};

template<>
struct type_traits<uint32_t> {
    typedef uint32_t value_type;
    static FlagValueType to_type() { return FV_TYPE_UINT32; }
    static FlagValue value_to_flag_value(value_type value)
    {
        FlagValue ret;
        ret._uint32_value = value;
        return ret;
    }
    static value_type flag_value_to_value(FlagValue fv)
    {
        return fv._uint32_value;
    }
    static value_type default_value() { return 0u; }
    static FlagValue raw_to_flag_value(const char* v)
    {
        FlagValue ret;
        if (NULL == v)
        {
            ret._uint32_value = default_value();
            return ret;
        }
        char *end = NULL;
        errno = 0;
        ret._uint32_value = static_cast<uint32_t>(strtoull(v, &end, 10));
        if (errno || end != v + strlen(v))
        {
            ret._uint32_value = default_value();
        }
        return ret;
    }
};

template<>
struct type_traits<float> {
    typedef float value_type;
    static FlagValueType to_type() { return FV_TYPE_FLOAT; }
    static FlagValue value_to_flag_value(value_type value)
    {
        FlagValue ret;
        ret._float_value = value;
        return ret;
    }
    static value_type flag_value_to_value(FlagValue fv)
    {
        return fv._float_value;
    }
    static value_type default_value() { return 0.0f; }
    static FlagValue raw_to_flag_value(const char* v)
    {
        FlagValue ret; 
        if (NULL == v)
        {
            ret._float_value = 0.0f;
            return ret;
        }
        char *end = NULL;
        errno = 0;
        ret._float_value = strtod(v, &end);
        if (errno || end != v + strlen(v))
        {
            ret._float_value = default_value();
        }
        return ret;
    }
};


//////////////////////////////////////////////////////////////////
// data structures store flags
//////////////////////////////////////////////////////////////////

// store one flag
struct Flag{
    FlagValue _flag_value;
    FlagValueType _type;
    string _flag_name;
};

typedef map<FlagID, Flag> FlagMap;

class FlagRegistry
{
public:
    template<class T>
    static FlagID register_flag(const char* flag_name, const T value)
    {
        if (NULL == flag_name) 
        {
            LOG(FATAL) << "empty flag name" << endl;
            return INVALID_FLAG_ID;
        }
        
        boost::hash<string> string_hash;
        FlagID flag_id = string_hash(flag_name);        
        FlagMap &default_flag_map = get_default_flag_map();
        if (default_flag_map.find(flag_id) != default_flag_map.end())
        {
            // error log, flag_id conflict.
            LOG(FATAL) << "exp flag ["<<flag_name<<"]"
                       << "is conflict with ["<<default_flag_map[flag_id]._flag_name<<"]"<<endl;
            return INVALID_FLAG_ID;
        }
    
        Flag flag;
        flag._flag_value = type_traits<T>::value_to_flag_value(value);
        flag._type = type_traits<T>::to_type(); 
        flag._flag_name = flag_name; 
    
        default_flag_map.insert(pair<FlagID, Flag>(flag_id, flag));
    
        return flag_id;
    }

    static FlagMap& get_default_flag_map()
    {
        static FlagMap default_flag_map;
        return default_flag_map;
    }
};


class ExpEnvironment {
public:
    typedef map<string, FlagMap*> ExpidFlagMap;

    static ExpEnvironment* get_instance()
    {
        if (NULL == _p_exp_environment)
        {
            _p_exp_environment = new ExpEnvironment();
        }
        return _p_exp_environment;
    }

    ~ExpEnvironment();
  
    bool get_bool(const char* exp_id, FlagID flag_id);
    int32_t get_int32(const char* exp_id, FlagID flag_id);
    uint32_t get_uint32(const char* exp_id, FlagID flag_id);
    float get_float(const char* exp_id, FlagID flag_id);

    // for test
    void print_one_flag(const Flag& flag) const;
    void print_one_flagmap(const FlagMap *_p_flag_map) const;
    void print_exp_environment() const;
private:
    ExpEnvironment() 
    {
        _p_default_flag_map = &(FlagRegistry::get_default_flag_map());
        load_exp_config("exp.conf");
    }

    void load_exp_config(const char* file_path);
    bool set_exp_flag_value(const string expid, const string flag_name, const string value);

    template<class T>
    T get_default_value(FlagID flag_id);
    
    template<class T>
    T get_exp_value(const char* exp_id, const FlagID flag_id);


    // disallow copy and assignment
    ExpEnvironment(const ExpEnvironment&);
    ExpEnvironment& operator=(const ExpEnvironment&);

private:
    static ExpEnvironment *_p_exp_environment;
    ExpidFlagMap *_p_expid_flag_map;
    FlagMap *_p_default_flag_map; 
};



// macros used to define and declare flags and flag map
#define DEFINE_BOOL(name, value)   \
            FlagID EXP_BOOL_##name = FlagRegistry::register_flag<bool>(#name, value);
#define DECLARE_BOOL(name)   \
            extern FlagID EXP_BOOL_##name;

#define DEFINE_INT32(name, value)   \
            FlagID EXP_INT32_##name = FlagRegistry::register_flag<int32_t>(#name, value);
#define DECLARE_INT32(name) \
            extern FlagID EXP_INT32_##name;

#define DEFINE_UINT32(name, value)   \
            FlagID EXP_UINT32_##name = FlagRegistry::register_flag<uint32_t>(#name, value);
#define DECLARE_UINT32(name) \
            extern FlagID EXP_UINT32_##name;

#define DEFINE_FLOAT(name, value)   \
            FlagID EXP_FLOAT_##name = FlagRegistry::register_flag<float>(#name, value);
#define DECLARE_FLOAT(name) \
            extern FlagID EXP_FLOAT_##name; 


}// end namespace exp_flag


#endif 
