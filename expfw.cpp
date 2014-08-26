#include <map>
#include <string>
#include <unistd.h>

#include "expfw.h"
#include "exp_flag.h"

#include <iostream>
using namespace std;


namespace exp_flag {


ExpEnvironment* ExpEnvironment::_p_exp_environment = NULL;

bool ExpEnvironment::get_bool(const char* exp_id, FlagID flag_id)
{
    return get_exp_value<bool>(exp_id, flag_id);
}

int32_t ExpEnvironment::get_int32(const char* exp_id, FlagID flag_id)
{
    return get_exp_value<int32_t>(exp_id, flag_id);
}

uint32_t ExpEnvironment::get_uint32(const char* exp_id, FlagID flag_id)
{
    return get_exp_value<uint32_t>(exp_id, flag_id);
}

float ExpEnvironment::get_float(const char* exp_id, FlagID flag_id)
{
    return get_exp_value<float>(exp_id, flag_id);
}


void ExpEnvironment::load_exp_config(const char* file_path)
{
    _p_expid_flag_map = new ExpidFlagMap();

    if (NULL == file_path) 
    {
        LOG(FATAL)<<"exp config file_path = NULL"<<endl;
        return;
    }
     
    ifstream ifs(file_path);
    if (!ifs.is_open())
    {
        LOG(FATAL)<<"open exp config failed"<<endl;
        return;
    }

    string tmp;
    string current_exp_id;
    while (getline(ifs, tmp))
    {
        boost::trim(tmp);
        if (tmp.size() == 0 || '#' == tmp[0]) 
        {//skip blank lines or comment lines
            continue;  
        }

        if (tmp[0] == '[' && tmp[tmp.size()-1] == ']')
        {// get exp_id
            current_exp_id = tmp.substr(1, tmp.size()-2);           
            boost::trim(current_exp_id);
            if (_p_expid_flag_map->find(current_exp_id) == _p_expid_flag_map->end())
            {
                _p_expid_flag_map->insert(
                    pair<string, FlagMap*>(current_exp_id, new FlagMap()));
            }
            else 
            {
                LOG(FATAL)<<"exp_id ["<<current_exp_id<<"] has been used before"<<endl;
            }
            continue; 
        }
        else 
        {// should be exp flag line
            size_t pos = tmp.find('=');
            if (pos == string::npos) 
            {// illegal flag line
                LOG(FATAL)<<"Illegal exp flag line in "<<file_path<<":"
                             <<tmp<<endl;
                continue;
            }
            string flag_name = tmp.substr(0, pos);
            string value = tmp.substr(pos+1, tmp.size()-1);
            boost::trim(flag_name);
            boost::trim(value);
            if (_p_expid_flag_map->find(current_exp_id) == _p_expid_flag_map->end())
            {// don't know this flag belongs which exp_id
                LOG(FATAL)<<"Don't know this flag["<<tmp<<"]"
                            <<"belongs to which expid"<<endl;
                continue; 
            }
            
            if (!set_exp_flag_value(current_exp_id, flag_name, value))
            {// set exp flag value failed
                LOG(FATAL)<<"Set exp flag failed: "<<tmp<<endl;
                continue;
            }
        }
    }
    ifs.close();
}


bool ExpEnvironment::set_exp_flag_value(const string expid, 
                                        const string flag_name, 
                                        const string value)
{
    boost::hash<string> string_hash; 
    FlagID flag_id = string_hash(flag_name);
    if (_p_default_flag_map->find(flag_id) == _p_default_flag_map->end())
    {// this flag is not defined 
        return false;
    }
    
    Flag default_flag = (*_p_default_flag_map)[flag_id];
    Flag flag;
    flag._type = default_flag._type;
    flag._flag_name = default_flag._flag_name; 
    switch (flag._type)
    {
        case FV_TYPE_BOOL: {
            flag._flag_value = type_traits<bool>::raw_to_flag_value(value.c_str());
            break;
        }
        case FV_TYPE_INT32: {
            flag._flag_value = type_traits<int32_t>::raw_to_flag_value(value.c_str());
            break;
        }
        case FV_TYPE_UINT32: {
            flag._flag_value = type_traits<uint32_t>::raw_to_flag_value(value.c_str());
            break;
        }
        case FV_TYPE_FLOAT: {
            flag._flag_value = type_traits<float>::raw_to_flag_value(value.c_str());
            break;
        }
        default: // unknown value type
            return false;
    }

    (*_p_expid_flag_map)[expid]->insert(pair<FlagID, Flag>(flag_id, flag));
    return true;
}

template<class T>
T ExpEnvironment::get_default_value(FlagID flag_id)
{
    if (_p_default_flag_map->find(flag_id) == _p_default_flag_map->end())
    {// this flag is not registered
        LOG(WARNING)<<"Unknown exp flag, return default type value"<<endl;
        return type_traits<T>::default_value();
    }
    FlagValue flag_value = (*_p_default_flag_map)[flag_id]._flag_value;
    return type_traits<T>::flag_value_to_value(flag_value);
}


template<class T>
T ExpEnvironment::get_exp_value(const char* exp_id, const FlagID flag_id)
{
    if (NULL == exp_id || INVALID_FLAG_ID == flag_id)
    {
        LOG(WARNING)<<"NULL exp_id or invalid flag_id, return default flag value."<<endl;
        return get_default_value<T>(flag_id);
    }
    if (_p_expid_flag_map->find(exp_id) != _p_expid_flag_map->end())
    {
        FlagMap *p_flag_map = (*_p_expid_flag_map)[exp_id];
        if (NULL == p_flag_map || p_flag_map->find(flag_id) == p_flag_map->end())
        {
            return get_default_value<T>(flag_id);
        }
        else
        {
            return type_traits<T>::flag_value_to_value((*p_flag_map)[flag_id]._flag_value);
        }
    } 
	
    LOG(WARNING)<<"Unknown exp_id, return default flag value."<<exp_id<<endl;
    return get_default_value<T>(flag_id);
}


ExpEnvironment::~ExpEnvironment()
{
    if (_p_expid_flag_map != NULL)
    {
        ExpidFlagMap::iterator it = _p_expid_flag_map->begin();
        for(; it != _p_expid_flag_map->end(); ++it)
        {
            if (it->second != NULL)
                delete it->second;
        }
        delete _p_expid_flag_map;
    }
}


void ExpEnvironment::print_one_flag(const Flag& flag) const
{
    switch (flag._type)
    {
        case FV_TYPE_BOOL:
            LOG(ERROR)<<"bool  : "<<flag._flag_name<<"="
                      <<type_traits<bool>::flag_value_to_value(flag._flag_value)<<endl;
            break;
        case FV_TYPE_INT32:
            LOG(ERROR)<<"int32 : "<<flag._flag_name<<"="
                      <<type_traits<int32_t>::flag_value_to_value(flag._flag_value)<<endl;
            break;
        case FV_TYPE_UINT32:
            LOG(ERROR)<<"uint32: "<<flag._flag_name<<"="
                      <<type_traits<uint32_t>::flag_value_to_value(flag._flag_value)<<endl;
            break;
        case FV_TYPE_FLOAT:
            LOG(ERROR)<<"float : "<<flag._flag_name<<"="
                      <<type_traits<float>::flag_value_to_value(flag._flag_value)<<endl;
            break;
        default:
            break;
    }
}

void ExpEnvironment::print_one_flagmap(const FlagMap *_p_flag_map) const
{
    FlagMap::const_iterator it = _p_flag_map->begin();
    for(; it != _p_flag_map->end(); ++it)
    {
        print_one_flag(it->second);
    }
}

void ExpEnvironment::print_exp_environment() const
{
    LOG(ERROR)<<"-------- default flag map ------------"<<endl;
    print_one_flagmap(_p_default_flag_map);
    ExpidFlagMap::const_iterator it = _p_expid_flag_map->begin();
    for (; it!=_p_expid_flag_map->end(); ++it)
    {
        LOG(ERROR)<<"-------- "<<it->first<<" ----------"<<endl;
        print_one_flagmap(it->second);
    }    
}
} // end namespace exp_flag
