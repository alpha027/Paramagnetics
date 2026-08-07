#ifndef SCENE_IO_H
#define SCENE_IO_H

#include <greeter/MagnetCollection.h>
#include <greeter/io/ArrangementIO.h>
#include <nlohmann/json.hpp>
#include <cstdint>
#include <vector>


namespace greeter {

/*
  Everything an input file describes: the magnets, the id each of them answers
  to, and which of them each arrangement generated.

  The ids used to be recovered by walking the "magnets" array of the file a
  second time, which only works while the file lists every magnet one by one.
  An arrangement generates magnets that are in the collection but in no array
  of the file, so the collection and its ids now come out of one pass and stay
  in step by construction.
*/
struct Scene {

    greeter::MagnetCollection collection;

    // One id per magnet, in the order of the collection.
    std::vector<int64_t> magnet_ids;

    // The members of each arrangement, in the order the arrangements appear.
    std::vector<greeter::ArrangementMembers> arrangements;
};

class SceneIO {

    public:

        SceneIO();
        ~SceneIO();

        /*
          Reads a file into a scene. The magnets the file lists come first, in
          the order it lists them and keeping the ids it gives them, so adding
          an arrangement never renames a magnet that was already there. The
          members of the arrangements follow, numbered on from the highest id
          the file used.
        */
        static greeter::Scene read(const nlohmann::json& data);
};

}  // namespace greeter

#endif // SCENE_IO_H
