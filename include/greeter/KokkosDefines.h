#ifndef KOKKOS_DEFINES_H_
#define KOKKOS_DEFINES_H_

#include <Kokkos_Core.hpp>
#include <Kokkos_Random.hpp>
#include <Kokkos_Timer.hpp>

//#define CUDASPACE 0
#define OPENMPSPACE 1
#define CUDAUVM 0
#define SERIAL 0
#define THREADS 0


////////////////////////////////// case OPENMPSPACE ////////////////////////////
#if OPENMPSPACE

typedef Kokkos::OpenMP ExecSpace;
typedef Kokkos::OpenMP MemSpace;
typedef Kokkos::LayoutRight Layout;

#endif

////////////////////////////////// case CUDASPACE //////////////////////////////
#if CUDASPACE

typedef Kokkos::Cuda ExecSpace;
typedef Kokkos::CudaSpace MemSpace;
typedef Kokkos::LayoutLeft Layout;

#endif

////////////////////////////////// case SERIAL /////////////////////////////////
#if SERIAL

typedef Kokkos::Serial ExecSpace;
typedef Kokkos::HostSpace MemSpace;

#endif

////////////////////////////////// case THREADS ////////////////////////////////
#if THREADS

typedef Kokkos::Threads ExecSpace;
typedef Kokkos::HostSpace MemSpace;

#endif

////////////////////////////////// case CUDAUVM ////////////////////////////////
#if CUDAUVM

typedef Kokkos::Cuda ExecSpace;
typedef Kokkos::CudaUVMSpace MemSpace;
typedef Kokkos::LayoutLeft Layout;

#endif

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
typedef typename Kokkos::Random_XorShift64_Pool<> RandPoolType;

#endif
