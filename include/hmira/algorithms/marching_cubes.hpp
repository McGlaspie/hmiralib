#ifndef __MARCHING_CUBES_HPP__
#define __MARCHING_CUBES_HPP__

#include <iostream>
#include <vector>
#include <tuple>
#include <algorithm>
#include <unordered_map>
#include <utility>

#include <boost/fusion/include/for_each.hpp>

#include <tbb/parallel_invoke.h>
#include <tbb/task_scheduler_init.h>
#include <tbb/parallel_reduce.h>
#include <tbb/concurrent_unordered_map.h>
#include <tbb/blocked_range.h>
#include <tbb/spin_mutex.h>
#include <tbb/parallel_for.h>
#include <tbb/task_scheduler_init.h>

#include <hmira/abstract/mesh_traits.hpp>
#include <hmira/concepts/algorithms/marching_cubes.hpp>
#include "marching_cubes_table.h"

#include <boost/config.hpp>
#include <boost/concept_check.hpp>
#include <boost/concept/requires.hpp>

namespace hmira
{

namespace algorithms
{
/**
 * 
 * \class Task
 * \brief Threading Building Blocks purposes
 * 
 * does not cover topology, the fastest possible solution, for GPU purposes
 * parallelized
 */
template <
	typename TGrid,
	typename TMesh,
	typename TGrid_Traits,// = grid_traits<TGrid>, 
	typename TMesh_Traits = mesh_traits<TMesh>
	>
class Task
{
public:
	typedef TGrid						Grid;

	typedef typename TGrid_Traits::Point_descriptor	Point_descriptor;
	typedef typename TGrid_Traits::Point_scalar_type	Point_scalar_type;
	typedef typename TGrid_Traits::Point_properties	Point_properties;
	typedef typename TGrid_Traits::Cube_descriptor		Cube_descriptor;
	typedef typename TGrid_Traits::Cube_iterator		Cube_iterator;

	typedef typename TMesh_Traits::Point			Mesh_Point;
	typedef typename TMesh_Traits::vertex_descriptor	Vertex_descriptor;
	typedef typename TMesh_Traits::face_descriptor		Face_descriptor;

	std::vector<std::tuple<Mesh_Point, Mesh_Point, Mesh_Point>> semi_result;
	const Grid&	grid_;

	Task(const Grid& _grid) : 
		grid_(_grid)
	{
	}
	
	Task( Task& subtask, tbb::split ) : grid_(subtask.grid_)
	{
		
	}
	
	void operator()(const tbb::blocked_range<int> &range)
	{
		for (auto i = range.begin(); i != range.end(); ++i)
		{
			Cube_iterator cube_i(i);
			process_cube(*cube_i);
		}
	}
	
	void join(Task& t)
	{
		semi_result.insert(semi_result.end(), t.semi_result.begin(), t.semi_result.end());
	}

	/**
	 * the method that adds a vertex to the mesh in the middle of two points
	 * of the grid.
	 * 
	 * \param	_p0 first point
	 * \param	_p1 second point
	 * 
	 * \return	the vertex to be added
	 */
	Mesh_Point&&
	add_vertex( Point_descriptor _p0, Point_descriptor _p1) //, TMesh& mm, vertex_hashmap& vertices_, const Grid& gg)
	{
		const Mesh_Point&  p0( TGrid_Traits::get_coords(grid_, _p0 ));
		const Mesh_Point&  p1( TGrid_Traits::get_coords(grid_, _p1 ));

		auto coeff = 0.5f; 
		Mesh_Point p = Mesh_Point((p0 + ( p1-p0 ) * coeff));

		return std::move(p);
	}

	/**
	 * the method that checks the corners of the cube
	 * and adds vertices to the mesh according the configuration
	 * 
	 * 
	 * \param	_cidx the cube to be resolved
	 */
	void
	process_cube( Cube_descriptor _cidx)//, const Grid& gg, TMesh& mm,  vertex_hashmap& vertices_, tbb::spin_mutex& Mutex)
	{
		Point_descriptor	corner[8];
		Mesh_Point		samples[12];
		unsigned int		cubetype( 0 );
		unsigned int		i;


		bool opp_bound = false;
		
		// get point indices of corner vertices
		for ( i=0; i<8; ++i )
			corner[i] = TGrid_Traits::get_cube_corner(grid_, _cidx, i );

		
		// determine cube type
		for ( i=0; i<8; ++i )
		{
			if ( !TGrid_Traits::is_inside( grid_, corner[i] ))
			{
				cubetype |= ( 1<<i );
			}
		}
		
		// trivial reject ?
		if ( cubetype == 0 || cubetype == 255 )
		{
			return;
		}
		// compute samples on cube's edges
		
		if ( cubetype == 0 || cubetype == 255 )
		{
			
		}
		else
		{
			if ( edgeTable[cubetype]&1 )    samples[0]  = add_vertex( corner[0], corner[1] );
			if ( edgeTable[cubetype]&2 )    samples[1]  = add_vertex( corner[1], corner[2] );
			if ( edgeTable[cubetype]&4 )    samples[2]  = add_vertex( corner[3], corner[2] );
			if ( edgeTable[cubetype]&8 )    samples[3]  = add_vertex( corner[0], corner[3] );
			if ( edgeTable[cubetype]&16 )   samples[4]  = add_vertex( corner[4], corner[5] );
			if ( edgeTable[cubetype]&32 )   samples[5]  = add_vertex( corner[5], corner[6] );
			if ( edgeTable[cubetype]&64 )   samples[6]  = add_vertex( corner[7], corner[6] );
			if ( edgeTable[cubetype]&128 )  samples[7]  = add_vertex( corner[4], corner[7] );
			if ( edgeTable[cubetype]&256 )  samples[8]  = add_vertex( corner[0], corner[4] );
			if ( edgeTable[cubetype]&512 )  samples[9]  = add_vertex( corner[1], corner[5] );
			if ( edgeTable[cubetype]&1024 ) samples[10] = add_vertex( corner[2], corner[6] );
			if ( edgeTable[cubetype]&2048 ) samples[11] = add_vertex( corner[3], corner[7] );

			
			for ( i=0; triTable[cubetype][0][i] != -1; i+=3 )
			{
				semi_result.push_back(std::make_tuple(
					samples[triTable[cubetype][0][i  ]],
					samples[triTable[cubetype][0][i+1]],
					samples[triTable[cubetype][0][i+2]]));
			}
		}
	}
	
};

/**
 * 
 * \ingroup algorithms
 * 
 * \class MarchingCubes
 * \brief converts the voxel representation into the mesh
 * 
 * inspired by the solution of rwth-aachen
 */
template <
	typename TGrid,
	typename TMesh,
	typename TGrid_Traits,// = grid_traits<TGrid>, 
	typename TMesh_Traits = mesh_traits<TMesh>
	>
class MarchingCubes
{

public:
	typedef TGrid						Grid;

	typedef typename TGrid_Traits::Point_descriptor	Point_descriptor;
	typedef typename TGrid_Traits::Point_scalar_type	Point_scalar_type;
	typedef typename TGrid_Traits::Point_properties	Point_properties;
	typedef typename TGrid_Traits::Cube_descriptor		Cube_descriptor;
	typedef typename TGrid_Traits::Cube_iterator		Cube_iterator;

	typedef typename TMesh_Traits::Point			Mesh_Point;
	typedef typename TMesh_Traits::vertex_descriptor	Vertex_descriptor;
	typedef typename TMesh_Traits::face_descriptor		Face_descriptor;

	const Grid&	grid_;
	TMesh&            mesh_;
	//tbb::spin_mutex Mutex;

// ========== FUNCTORS
	
/**
 * \struct VertexPairHash
 * \brief the hash for the purpose of storing vertices in unordered_map
 */
struct VertexPairHash {
	size_t operator() (const std::pair<Point_descriptor, Point_descriptor>& points) const { 
		auto k1 = points.first;
		auto k2 = points.second;
		return (k1 << 31) + k2;//result;
	}
};

/**
 * \struct VertexPairEqual
 * \brief the definition of equality in the vertex_hashmap
 */
struct VertexPairEqual {
	bool operator() (
		const std::pair<Point_descriptor, Point_descriptor>& point1,
		const std::pair<Point_descriptor, Point_descriptor>& point2) const 
	{ 
		return (point1.first == point2.first) 
			&& (point1.second == point2.second);
	}
};

	typedef std::unordered_map
		<
			std::pair<Point_descriptor, Point_descriptor>,
			Vertex_descriptor,
			VertexPairHash,
			VertexPairEqual
		> vertex_hashmap; 

	vertex_hashmap vertices_;
	
	MarchingCubes( const Grid& _grid, TMesh& _mesh)
	: grid_( _grid ),
	mesh_( _mesh )
	{
		boost::function_requires<hmira::concepts::algorithms::MarchingCubesConcept<TGrid, TMesh, TGrid_Traits, TMesh_Traits> >();
	}
	
	/**
	 * starts a <b>Marching Cubes<b/> algorithm
	 * with no parallelization
	 * 
	 * the result is the mesh with topology
	 */
	void process()
		{
		for ( auto cube = grid_.begin(); cube!=grid_.end(); ++cube )
		{
			process_cube( *cube );
		}
	}

	/**
	 * starts a <b>Marching Cubes<b/> algorithm
	 * using parallelization <b>Threading Building Blocks</b>
	 * 
	 * the result is the mesh with no topology, just the raw set of faces
	 * 
	 */
	void parallel_process()
	{
		Task<TGrid, TMesh, TGrid_Traits, TMesh_Traits> task(grid_);
		
		auto cubes_count = 
				TGrid_Traits::get_x_resolution(grid_) * 
				TGrid_Traits::get_y_resolution(grid_) * 
				TGrid_Traits::get_z_resolution(grid_);
		
		tbb::parallel_reduce(tbb::blocked_range<int>(
				0,
				cubes_count,
				cubes_count / tbb::task_scheduler_init::default_num_threads()),
			task);
		
		std::cerr << "[MARCHING CUBES] : " << "num of threads: " << tbb::task_scheduler_init::default_num_threads() << std::endl;
		
		for (auto triplet : task.semi_result )
		{
			auto a = TMesh_Traits::create_vertex(std::get<0>(triplet), mesh_);
			auto b = TMesh_Traits::create_vertex(std::get<1>(triplet), mesh_);
			auto c = TMesh_Traits::create_vertex(std::get<2>(triplet), mesh_);
			TMesh_Traits::create_face(a, b, c, mesh_);
		}
	}
	

	void PCube(int r, const Grid& _gg, const TMesh& _mm, const vertex_hashmap& vertices_, const tbb::spin_mutex& _Mutex)
	{
		TMesh& mm = const_cast<TMesh&>(_mm);
		tbb::spin_mutex& Mutex = const_cast<tbb::spin_mutex&>(_Mutex);
		vertex_hashmap& vertices = const_cast<vertex_hashmap&>(vertices_);
		Cube_iterator c(r);
		process_cube(*c, _gg, mm, vertices, Mutex);
	}
	
	void process_grid_part(int a, int b, const Grid& gg, TMesh& mm, vertex_hashmap& vertices_, tbb::spin_mutex& Mutex)
	{
		Cube_iterator ca(a);
		Cube_iterator cb(b);
		
		std::cout << "som tu" << std::endl;
		
		tbb::parallel_for<int>(a, b, 1, [=](size_t i) {PCube(i, gg, mm, vertices_, Mutex);});
	}

	int marching_cubes(Grid& g, TMesh& m)
	{
		for ( auto cube : grid_ )
			process_cube( cube );

	}

	/**
	 * 
	 * \param _p0	first point
	 * \param _p1	second point
	 * \param gg	Voxel Map that is converted to the mesh
	 * 
	 * \return	the interpolated value of the points
	 */
	Point_scalar_type
	interpolate( Point_descriptor _p0, Point_descriptor _p1 , const Grid& gg)
	{
		auto s0 = TGrid_Traits::scalar_value( gg, _p0 );
		auto s1 = TGrid_Traits::scalar_value( gg, _p1 );

		// 0-1 metric case
 		if (
			((s0 == 0) && (s1 == -1))||
			((s0 == -1) && (s1 == 0)))
		{
			return 0.5f;
		}

		//no scalar type exists
		if ( (s0 == 0) && (s1 == 0))
			return 0.5f;

		return -s0 / (s1 - s0);
	}

	/**
	 * \brief helper for for_each in tuple
	 */
	template <typename T_Tuple, size_t size>
	struct interpolate_helper 
	{
		static void interpolate( const T_Tuple & t0, const T_Tuple & t1, Point_scalar_type coeff, T_Tuple & t )
		{
			interpolate_helper<T_Tuple,size-1>::interpolate( t0, t1, coeff, t );
			std::get<size-1>( t ) =  ( 1.0f - coeff ) * std::get<size-1>( t0 ) + coeff * std::get<size-1>( t1 );
			return;
		}
	};

	/**
	 * \brief helper for for_each in tuple
	 */
	template <typename T_Tuple>
	struct interpolate_helper<T_Tuple,0> 
	{
		static void interpolate(const T_Tuple &, const T_Tuple &, Point_scalar_type, T_Tuple & )
		{
			return;
		}
	};


	/**
	 * method that performs for_each over a tuple and interpolates the values set in the member of a tuple
	 * 
	 * \param	t0 tuple value for the first point
	 * \param	t1 tuple value for the second point
	 * \param	coeff the interpolate coefficient
	 * 
	 * \return	interpolated tuple by the <b>coeff</b>
	 */
	template <typename T_Tuple>
	T_Tuple interpolate_tuple( const T_Tuple& t0, const T_Tuple& t1, Point_scalar_type coeff )
	{
		T_Tuple t;
		interpolate_helper<T_Tuple, std::tuple_size<T_Tuple>::value>::interpolate( t0, t1, coeff, t);
		return t;
	}

	bool
	set_interpolated_properties(
		Point_descriptor _p0,
		Point_descriptor _p1,
		Vertex_descriptor vh,
		Point_scalar_type coeff
		)
	{
		auto t0 = TGrid_Traits::get_point_properties(grid_, _p0);
		auto t1 = TGrid_Traits::get_point_properties(grid_, _p1);

		auto t = interpolate_tuple(t0, t1, coeff);

		TMesh_Traits::set_property(mesh_, vh, t);

	}

	/**
	 * the method that adds a vertex to the mesh between two points
	 * of the grid according the interpolated value of the points.
	 * 
	 * \param	_p0 first point
	 * \param	_p1 second point
	 * 
	 * \return	the vertex to be added
	 */
	Vertex_descriptor
	add_vertex( Point_descriptor _p0, Point_descriptor _p1) //, TMesh& mm, vertex_hashmap& vertices_, const Grid& gg)
	{
		const Mesh_Point&  p0( TGrid_Traits::get_coords(grid_, _p0 ));
		const Mesh_Point&  p1( TGrid_Traits::get_coords(grid_, _p1 ));

		Vertex_descriptor vh;

		auto coeff = interpolate(_p0, _p1, grid_);
		Mesh_Point p = Mesh_Point((p0 + ( p1-p0 ) * coeff));

		//avoid flipped edges
		if (_p1 < _p0) 
		{
			std::swap(_p0, _p1);
		}

		//check if vertex is already in mesh

		if (vertices_[std::make_pair(_p0, _p1)].is_valid())
			return vertices_[std::make_pair(_p0, _p1)];


		vh = TMesh_Traits::create_vertex(p, mesh_);

		vertices_[std::make_pair(_p0, _p1)] = vh;

		return vh;
	}

	void
	process_cube( Cube_descriptor _cidx)
	{
		Point_descriptor	corner[8];
		Vertex_descriptor	samples[12];
		unsigned int		cubetype( 0 );
		unsigned int		i;


		bool opp_bound = false;
		
		// get point indices of corner vertices
		for ( i=0; i<8; ++i )
			corner[i] = TGrid_Traits::get_cube_corner(grid_, _cidx, i );

		
		// determine cube type
		for ( i=0; i<8; ++i )
		{
			if ( !TGrid_Traits::is_inside( grid_, corner[i] ))
			{
				cubetype |= ( 1<<i );
			}
		}
		
		// trivial reject ?
		if ( cubetype == 0 || cubetype == 255 )
		{
			return;
		}
		// compute samples on cube's edges
			{
		if ( cubetype == 0 || cubetype == 255 )
		{
			
		}
		else
		{
		if ( edgeTable[cubetype]&1 )    samples[0]  = add_vertex( corner[0], corner[1] );
		if ( edgeTable[cubetype]&2 )    samples[1]  = add_vertex( corner[1], corner[2] );
		if ( edgeTable[cubetype]&4 )    samples[2]  = add_vertex( corner[3], corner[2] );
		if ( edgeTable[cubetype]&8 )    samples[3]  = add_vertex( corner[0], corner[3] );
		if ( edgeTable[cubetype]&16 )   samples[4]  = add_vertex( corner[4], corner[5] );
		if ( edgeTable[cubetype]&32 )   samples[5]  = add_vertex( corner[5], corner[6] );
		if ( edgeTable[cubetype]&64 )   samples[6]  = add_vertex( corner[7], corner[6] );
		if ( edgeTable[cubetype]&128 )  samples[7]  = add_vertex( corner[4], corner[7] );
		if ( edgeTable[cubetype]&256 )  samples[8]  = add_vertex( corner[0], corner[4] );
		if ( edgeTable[cubetype]&512 )  samples[9]  = add_vertex( corner[1], corner[5] );
		if ( edgeTable[cubetype]&1024 ) samples[10] = add_vertex( corner[2], corner[6] );
		if ( edgeTable[cubetype]&2048 ) samples[11] = add_vertex( corner[3], corner[7] );

		// connect samples by triangles
		for ( i=0; triTable[cubetype][0][i] != -1; i+=3 )
		TMesh_Traits::create_face(
			samples[triTable[cubetype][0][i  ]],
			samples[triTable[cubetype][0][i+1]],
			samples[triTable[cubetype][0][i+2]],
			mesh_ );
		}
				
			}
		}
};


}//algorithms

}//hmira

#endif // __MARCHING_CUBES_HPP__
