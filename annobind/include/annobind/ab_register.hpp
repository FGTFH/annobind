#pragma once
#include <nanobind/nanobind.h>

#include <any>
#include <typeindex>
#include <unordered_map>

namespace annobind {
namespace nb = nanobind;
template <typename T, typename... Ts>
nb::class_<T> nb_class_peeled(const nb::class_<T, Ts...>& cls) {
    return nb::class_<T>(cls.ptr(), nb::detail::borrow_t{});
}

class ABRegistry {
   public:
    static ABRegistry& instance() {
        static ABRegistry registry;
        return registry;
    }

    template <typename T, typename... Ts>
        requires(!std::is_enum_v<T>)
    static void register_class(const nb::class_<T, Ts...>& cls) {
        if (!instance().enabled_) return;
        auto& map_ = instance().cmap_;
        map_[std::type_index(typeid(T))] = nb_class_peeled(cls);
    }

    template <typename T>
        requires(std::is_enum_v<T>)
    static void register_enum(const nb::enum_<T>& enm) {
        if (!instance().enabled_) return;
        auto& map_ = instance().cmap_;
        map_[std::type_index(typeid(T))] = enm;
    }

    static void register_module(const nb::module_& m, const std::string& name) {
        if (!instance().enabled_) return;
        auto& map_ = instance().mmap_;
        map_.insert(std::make_pair(name, m));
    }

    template <typename T>
        requires(!std::is_enum_v<T>)
    static nb::class_<T>* find_class() {
        if (!instance().enabled_) return nullptr;
        auto& map_ = instance().cmap_;
        auto it = map_.find(std::type_index(typeid(T)));
        if (it != map_.end()) {
            return std::any_cast<nb::class_<T>>(&it->second);
        }
        return nullptr;
    }

    template <typename T>
        requires(std::is_enum_v<T>)
    static nb::enum_<T>* find_enum() {
        if (!instance().enabled_) return nullptr;
        auto& map_ = instance().cmap_;
        auto it = map_.find(std::type_index(typeid(T)));
        if (it != map_.end()) {
            return std::any_cast<nb::enum_<T>>(&it->second);
        }
        return nullptr;
    }

    static nb::module_* find_module(const std::string& name) {
        if (!instance().enabled_) return nullptr;
        auto& map_ = instance().mmap_;
        auto it = map_.find(name);
        if (it != map_.end()) {
            return &it->second;
        }
        return nullptr;
    }

    template <typename T>
    static bool registered() {
        if (!instance().enabled_) return false;
        auto& map_ = instance().cmap_;
        return map_.find(std::type_index(typeid(T))) != map_.end();
    }

    static bool registered(const std::string& name) {
        if (!instance().enabled_) return false;
        auto& map_ = instance().mmap_;
        return map_.find(name) != map_.end();
    }
    static void enable() { instance().enabled_ = true; }

    static void disable() {
        instance().enabled_ = false;
        instance().cmap_.clear();
        instance().mmap_.clear();
    }

   private:
    // 若nb::enum_有直接从nb::handle构造的构造函数, 这里只需要储存nb::handle
    std::unordered_map<std::type_index, std::any> cmap_;
    std::unordered_map<std::string, nb::module_> mmap_;
    bool enabled_ = false;
};

struct reg_guard {
    reg_guard() { ABRegistry::instance().enable(); }
    ~reg_guard() { ABRegistry::instance().disable(); }
};
}  // namespace annobind
