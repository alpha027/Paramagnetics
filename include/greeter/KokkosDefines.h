#ifndef _greeter_kokkos_Defines_h_
#define _greeter_kokkos_Defines_h_

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

// random generator type def
typedef typename Kokkos::Random_XorShift64_Pool<> RandPoolType;

#endif
