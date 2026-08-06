#ifndef ARRANGEMENT_IO_H
#define ARRANGEMENT_IO_H

#include <greeter/Magnet.h>
#include <nlohmann/json.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>


namespace greeter {

/*
  Which magnets of a collection an arrangement generated. The force section
  refers to a whole arrangement through this, so the record lives in a header
  light enough for ForceIO to include, rather than with the collection itself.
*/
struct ArrangementMembers {
    int64_t id = 0;
    std::vector<uint32_t> members;  // indices into the collection
};

/*
  Helpers shared by every arrangement.

  An arrangement decides where its members sit and how they are turned. What a
  member *is* stays the business of the magnet readers, so an arrangement
  carries an "element" written in the schema of a magnet minus its placement,
  and buildElement hands that to the reader of its own type together with the
  placement the arrangement worked out. That is what lets any arrangement be
  built out of any magnet type, including ones added later, without either
  knowing about the other.

  A member is first placed in the frame of the arrangement, and the arrangement
  is then placed in the world by its own "position" and "orientation". The
  magnetization of an element is read in the frame of that element and is
  carried along by its orientation, so a member is moved as a rigid body, its
  geometry and its polarization together.
*/
class ArrangementIO {

    public:

        ArrangementIO();
        ~ArrangementIO();

        // The "parameters" object, which every arrangement has.
        static const nlohmann::json& readParameters(const nlohmann::json& arrangement);

        /*
          Placement of the assembly as a whole. Left out, it sits at the origin
          unturned. The quaternion is normalised, because composing a member
          placement with one that is not would scale the positions of the
          members rather than only turn them.
        */
        static void readTransform(const nlohmann::json& parameters,
                                  float* position, float* orientation);

        // Carry a member placement given in the frame of the arrangement over
        // into the world frame.
        static void compose(const float* arrangement_position,
                            const float* arrangement_orientation,
                            const float* local_position,
                            const float* local_orientation,
                            float* position, float* orientation);

        // The "element" of an arrangement, which is a magnet without a placement.
        static const nlohmann::json& readElement(const nlohmann::json& parameters);

        static std::unique_ptr<greeter::Magnet> buildElement(
            const nlohmann::json& element,
            const float* position, const float* orientation);

        /*
          A count of members along the three local axes. A single number counts
          along the first axis only, so that a row does not have to be written
          as [n, 1, 1].
        */
        static void readCounts(const nlohmann::json& parameters,
                               const std::string& name, uint32_t* counts);

        // A length along the three local axes, a single number meaning all three.
        static void readSpacing(const nlohmann::json& parameters,
                                const std::string& name, float* spacing);

        // A number of members, for an arrangement that runs along one line or
        // round one circle rather than over a lattice.
        static uint32_t readCount(const nlohmann::json& parameters, const std::string& name);

        // A required length, which has to be strictly positive.
        static float readLength(const nlohmann::json& parameters, const std::string& name);

        // A required number that may be of either sign but not zero.
        static float readNonZero(const nlohmann::json& parameters, const std::string& name);

        /*
          A whole number that may be negative, for the order of a Halbach ring.
          The polarization has to come back to itself after one turn round the
          ring, which is what makes it whole.
        */
        static int64_t readWholeNumber(const nlohmann::json& parameters,
                                       const std::string& name, const int64_t& fallback);

        // Whether the arrangement sits on its position or starts there.
        static bool readCentered(const nlohmann::json& parameters);
};

}  // namespace greeter

#endif // ARRANGEMENT_IO_H
