#include "cpp_streamer_factory.hpp"
#include "logger.hpp"

#include <dlfcn.h>
#include <stdio.h>

namespace cpp_streamer
{
using MAKE_STREAMER_FUN_PTR = void*(*)();
using DESTROY_STREAMER_FUN_PTR = void(*)(void*);

std::string CppStreamerFactory::lib_path_ = DEF_LIB_PATH;
Logger* CppStreamerFactory::s_logger_ = nullptr;
std::map<std::string, void*> CppStreamerFactory::name2handle_;

void CppStreamerFactory::SetLibPath(const std::string& path) {
    lib_path_ = path;
}

void* CppStreamerFactory::GetHandle(const std::string& streamer_name) {
    std::string name = CppStreamerFactory::lib_path_;
    void* handle = nullptr;

    name += "/lib";
    name += streamer_name;
    #ifdef _WIN32
    name += ".dll";
    #elif __linux__
    name += ".so";
    #elif __APPLE__
    name += ".dylib";
    #endif

    LogInfof(s_logger_, "try to dlopen %s", name.c_str());

    if (name2handle_.find(streamer_name) == name2handle_.end()) {
        handle = dlopen(name.c_str(), RTLD_LAZY);
        if (handle == nullptr) {
            LogErrorf(s_logger_, "dlopen %s error:%s", name.c_str(), dlerror());
            return nullptr;
        }
        name2handle_[streamer_name] = handle;
    } else {
        handle = name2handle_[streamer_name];
    }
    return handle;
}

CppStreamerInterface* CppStreamerFactory::MakeStreamer(const std::string& streamer_name) {
    char* err_msg = nullptr;
    char make_func_name[80];
    void* handle = nullptr;

    handle = GetHandle(streamer_name);
    
    snprintf(make_func_name, sizeof(make_func_name), "make_%s_streamer", streamer_name.c_str());
    LogInfof(s_logger_, "call function:%s", make_func_name);

    MAKE_STREAMER_FUN_PTR maker_fun = (MAKE_STREAMER_FUN_PTR)dlsym(handle, make_func_name);
    if ((err_msg = dlerror()) != NULL) {
        LogErrorf(s_logger_, "Symbol %s not found: %s", make_func_name, err_msg);
        return nullptr;
    }
    CppStreamerInterface* ret = (CppStreamerInterface*)maker_fun();

    return ret;
}

void CppStreamerFactory::DestroyStreamer(const std::string& streamer_name, CppStreamerInterface* streamer) {
    char* err_msg = nullptr;
    void* handle = nullptr;

    handle = GetHandle(streamer_name);

    DESTROY_STREAMER_FUN_PTR destroy_fun = (DESTROY_STREAMER_FUN_PTR)dlsym(handle, "destroy_streamer");
    if ((err_msg = dlerror()) != NULL) {
        LogErrorf(s_logger_, "Symbol destroy_streamer not found: %d", err_msg);
        return;
    }

    destroy_fun((void*)streamer);

    return;
}

void CppStreamerFactory::ReleaseAll() {
    for (auto& item : name2handle_) {
        item.second = nullptr;
    }
}
}
