#ifndef ARRANGEMENT_FACTORY_IO_H
#define ARRANGEMENT_FACTORY_IO_H

#include <greeter/Magnet.h>
#include <nlohmann/json.hpp>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>


namespace greeter {

/*
  Registry of the arrangements, keyed by the "type" of the "arrangements"
  section of the input file. An arrangement expands into the magnets it is made
  of, which the scene then holds like any other magnet, so nothing downstream
  of the reader knows that an arrangement was involved.

  The arrangements of this library are registered by the constructor, see
  MagneticFieldMethodFactory for why they cannot register themselves.
*/
class ArrangementFactoryIO {

    public:

        static ArrangementFactoryIO& getInstance() {
            static ArrangementFactoryIO instance;
            return instance;
        }

        using MethodFunction = std::function<std::vector<std::unique_ptr<Magnet>>(
                                                 const nlohmann::json& arrangement)>;

        bool registerExpand(const std::string& key, MethodFunction _method);

        std::vector<std::unique_ptr<Magnet>> expand(
            const std::string& key, const nlohmann::json& arrangement) const;

        bool isRegistered(const std::string& key) const;

        void displayRegistered() const;

    private:

        ArrangementFactoryIO();  // Private constructor, registers the built in arrangements
        ~ArrangementFactoryIO() = default;
        ArrangementFactoryIO(const ArrangementFactoryIO&) = delete;
        ArrangementFactoryIO& operator=(const ArrangementFactoryIO&) = delete;

        std::unordered_map<std::string, MethodFunction> registry;
};

}  // namespace greeter

#endif // ARRANGEMENT_FACTORY_IO_H
