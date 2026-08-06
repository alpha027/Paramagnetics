#include <greeter/io/ForceIO.h>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_map>


namespace greeter {

    ForceIO::ForceIO() {}

    ForceIO::~ForceIO() {}

    bool ForceIO::hasForceSection(const nlohmann::json& data) {
        return data.contains("force");
    }

    std::vector<int64_t> ForceIO::readMagnetIds(const nlohmann::json& data) {

        std::vector<int64_t> ids;

        if (!data.contains("magnets")) {
            return ids;
        }

        int64_t index = 0;

        for (const auto& magnet : data["magnets"]) {
            if (magnet.contains("id")) {
                ids.push_back(magnet["id"].get<int64_t>());
            } else {
                ids.push_back(index);
            }
            index++;
        }

        return ids;
    }

    greeter::MeshingSpec ForceIO::readMeshing(const nlohmann::json& meshing) {

        greeter::MeshingSpec spec;

        if (meshing.is_number()) {
            const int64_t total = meshing.get<int64_t>();
            if (total < 1) {
                throw std::invalid_argument("Force meshing must be a positive integer");
            }
            spec.total = (uint32_t) total;
            return spec;
        }

        if (meshing.is_array() && meshing.size() == 3) {
            spec.explicit_split = true;
            for (size_t i = 0; i < 3; i++) {
                const int64_t n = meshing[i].get<int64_t>();
                if (n < 1) {
                    throw std::invalid_argument("Force meshing must be a positive integer");
                }
                spec.n[i] = (uint32_t) n;
            }
            return spec;
        }

        throw std::invalid_argument(
            "Force meshing must be a positive integer or an array of three positive integers");
    }

    namespace {

        const float PIVOT_CENTROID = std::numeric_limits<float>::quiet_NaN();

        uint32_t resolveMagnetId(const std::unordered_map<int64_t, uint32_t>& index_of_id,
                                 const nlohmann::json& id) {

            auto it = index_of_id.find(id.get<int64_t>());

            if (it == index_of_id.end()) {
                throw std::invalid_argument(
                    "Force section refers to the unknown magnet id " + id.dump());
            }

            return it->second;
        }

        bool namesAnArrangement(const nlohmann::json& reference) {
            return reference.is_object() && reference.contains("arrangement");
        }

        const std::vector<uint32_t>& resolveArrangementId(
            const std::vector<greeter::ArrangementMembers>& arrangements,
            const nlohmann::json& id) {

            const int64_t wanted = id.get<int64_t>();

            for (const auto& arrangement : arrangements) {
                if (arrangement.id == wanted) {
                    return arrangement.members;
                }
            }

            throw std::invalid_argument(
                "Force section refers to the unknown arrangement id " + id.dump());
        }

        /*
          One entry of a "targets" or "sources" list, which names either a
          single magnet by its id or every member of an arrangement at once.
        */
        void appendReference(const std::unordered_map<int64_t, uint32_t>& index_of_id,
                             const std::vector<greeter::ArrangementMembers>& arrangements,
                             const nlohmann::json& reference,
                             std::vector<uint32_t>& indices) {

            if (namesAnArrangement(reference)) {
                for (const auto& member :
                     resolveArrangementId(arrangements, reference["arrangement"])) {
                    indices.push_back(member);
                }
                return;
            }

            indices.push_back(resolveMagnetId(index_of_id, reference));
        }

        std::array<float, 3> readPivot(const nlohmann::json& pivot) {

            if (pivot.is_string()) {
                if (pivot.get<std::string>() != "centroid") {
                    throw std::invalid_argument(
                        "Force pivot must be \"centroid\" or an array of three numbers");
                }
                return {PIVOT_CENTROID, PIVOT_CENTROID, PIVOT_CENTROID};
            }

            if (pivot.is_array() && pivot.size() == 3) {
                return {pivot[0].get<float>(), pivot[1].get<float>(), pivot[2].get<float>()};
            }

            throw std::invalid_argument(
                "Force pivot must be \"centroid\" or an array of three numbers");
        }

    }  // namespace

    greeter::ForceConfig ForceIO::read(const nlohmann::json& data) {

        // The ids of a file that lists every magnet are the ids of its
        // "magnets" array. As soon as an arrangement generates magnets that
        // are in no array of the file, that stops being true, and reading the
        // force section against a short id map would quietly leave the
        // generated magnets out of "all" and mislabel them in the output.
        if (data.contains("arrangements") && !data["arrangements"].empty()) {
            throw std::invalid_argument(
                "A file with arrangements has to be read through SceneIO, so that the "
                "generated magnets are part of the id map");
        }

        return read(data, readMagnetIds(data), {});
    }

    greeter::ForceConfig ForceIO::read(
        const nlohmann::json& data,
        const std::vector<int64_t>& ids,
        const std::vector<greeter::ArrangementMembers>& arrangements) {

        if (!hasForceSection(data)) {
            throw std::invalid_argument("The JSON file has no force section");
        }

        const nlohmann::json& force = data["force"];

        std::unordered_map<int64_t, uint32_t> index_of_id;
        for (size_t i = 0; i < ids.size(); i++) {
            index_of_id[ids[i]] = (uint32_t) i;
        }

        // Defaults shared by all targets
        greeter::MeshingSpec default_meshing;
        if (force.contains("meshing")) {
            default_meshing = readMeshing(force["meshing"]);
        }

        std::array<float, 3> default_pivot = {PIVOT_CENTROID, PIVOT_CENTROID, PIVOT_CENTROID};
        if (force.contains("pivot")) {
            default_pivot = readPivot(force["pivot"]);
        }

        std::vector<uint32_t> default_sources;
        if (force.contains("sources") && !force["sources"].is_string()) {
            for (const auto& source : force["sources"]) {
                appendReference(index_of_id, arrangements, source, default_sources);
            }
        }

        greeter::ForceConfig config;

        if (force.contains("eps")) {
            config.eps = force["eps"].get<float>();
        }

        if (force.contains("meshreport")) {
            config.mesh_report = force["meshreport"].get<bool>();
        }

        // Targets: "all", a list of magnet ids, or a list of objects
        if (!force.contains("targets") ||
            (force["targets"].is_string() && force["targets"].get<std::string>() == "all")) {

            for (size_t i = 0; i < ids.size(); i++) {
                config.targets.push_back((uint32_t) i);
                config.meshing.push_back(default_meshing);
                config.pivots.push_back(default_pivot);
                config.sources.push_back(default_sources);
            }

        } else if (force["targets"].is_array()) {

            for (const auto& target : force["targets"]) {

                // An entry names one magnet or, for an arrangement, all of its
                // members at once, and they then share what the entry says
                // about meshing, pivot and sources.
                std::vector<uint32_t> indices;
                greeter::MeshingSpec meshing = default_meshing;
                std::array<float, 3> pivot = default_pivot;
                std::vector<uint32_t> sources = default_sources;

                if (target.is_object()) {

                    if (namesAnArrangement(target)) {
                        for (const auto& member :
                             resolveArrangementId(arrangements, target["arrangement"])) {
                            indices.push_back(member);
                        }
                    } else if (target.contains("id")) {
                        indices.push_back(resolveMagnetId(index_of_id, target["id"]));
                    } else {
                        throw std::invalid_argument(
                            "A force target object needs an id or an arrangement");
                    }

                    if (target.contains("meshing")) {
                        meshing = readMeshing(target["meshing"]);
                    }

                    if (target.contains("pivot")) {
                        pivot = readPivot(target["pivot"]);
                    }

                    if (target.contains("sources") && !target["sources"].is_string()) {
                        sources.clear();
                        for (const auto& source : target["sources"]) {
                            appendReference(index_of_id, arrangements, source, sources);
                        }
                    }

                } else {

                    indices.push_back(resolveMagnetId(index_of_id, target));
                }

                for (const auto& index : indices) {
                    config.targets.push_back(index);
                    config.meshing.push_back(meshing);
                    config.pivots.push_back(pivot);
                    config.sources.push_back(sources);
                }
            }

        } else {
            throw std::invalid_argument(
                "Force targets must be \"all\" or an array of magnet ids or target objects");
        }

        if (config.targets.empty()) {
            throw std::invalid_argument("The force section has no target");
        }

        // A pivot is only read from the configuration when it is not the centroid
        // of the target, which is marked by a not-a-number entry.
        config.centroid_pivot = true;
        for (const auto& pivot : config.pivots) {
            if (!std::isnan(pivot[0])) {
                config.centroid_pivot = false;
            }
        }

        return config;
    }

}  // namespace greeter
