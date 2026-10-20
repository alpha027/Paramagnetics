#ifndef KOKKOS_DEFINES_H_
#define KOKKOS_DEFINES_H_

#include <Kokkos_Core.hpp>
#include <Kokkos_Random.hpp>
#include <Kokkos_Timer.hpp>

// Field dispatch uses host function pointers. Select a host backend explicitly;
// a device-memory backend requires a separate kernel-dispatch and memory audit.
#if defined(PARAMAGNETICS_BACKEND_OPENMP)
#ifndef KOKKOS_ENABLE_OPENMP
#error "ParaMagneticS OpenMP backend requires Kokkos_ENABLE_OPENMP=ON"
#endif
using ExecSpace = Kokkos::OpenMP;
#elif defined(PARAMAGNETICS_BACKEND_SERIAL)
#ifndef KOKKOS_ENABLE_SERIAL
#error "ParaMagneticS Serial backend requires Kokkos_ENABLE_SERIAL=ON"
#endif
using ExecSpace = Kokkos::Serial;
#else
using ExecSpace = Kokkos::DefaultHostExecutionSpace;
#endif
using MemSpace = ExecSpace::memory_space;
using Layout = Kokkos::LayoutRight;

typedef Kokkos::RangePolicy< ExecSpace > range_policy;

// default item view types
typedef Kokkos::View< float*[3], Layout, MemSpace > Float3VectorView;
typedef Kokkos::View< float*[4], Layout, MemSpace > Float4VectorView;
typedef Kokkos::View< bool*, Layout, MemSpace > BoolVectorView;
typedef Kokkos::View< uint8_t*, Layout, MemSpace > UInt8VectorView;
typedef Kokkos::View< uint32_t*, Layout, MemSpace > UInt32VectorView;
typedef Kokkos::View< int32_t*, Layout, MemSpace > Int32VectorView;
typedef Kokkos::View< uint64_t*, Layout, MemSpace > UInt64VectorView;
typedef Kokkos::View< float*, Layout, MemSpace > FloatVectorView;

/*
  Field kernel of a magnet class. A plain function pointer rather than a
  std::function, so that it can be held in a View and called from inside a
  parallel loop.
*/
typedef void (*FieldKernel)(const float* parameters, const float* observation_point,
                            float& b_x, float& b_y, float& b_z);

/*
  Everything a simulator has to know about one magnet of a simulation that
  depends on its type. Resolved once, before the parallel loop, so that the loop
  neither looks the type up in the registry nor branches on it.

  The kernel is wrapped in a struct because a View reads every trailing '*' of
  its data type as a rank, which would take a bare function pointer apart.
*/
struct MagnetKernel {
    FieldKernel kernel;            // field kernel of the shape
    FieldKernel polarization;      // polarization of the shape at a point, may be null
    u_int32_t parameter_offset;    // start of this magnet's parameters in `magnet_parameters`
};

typedef Kokkos::View< MagnetKernel*, Layout, MemSpace > MagnetKernelView;

// random generator type def
typedef typename Kokkos::Random_XorShift64_Pool<ExecSpace> RandPoolType;

#endif
