#include <OpenMesh/Core/Mesh/PolyMesh_ArrayKernelT.hh>
#include <OpenMesh/Core/IO/MeshIO.hh>

#include <iostream>
#include <string>

#include <hmira/algorithms/marching_cubes.hpp>
#include <hmira/algorithms/voxelize.hpp>
#include <hmira/algorithms/fill_holes.hpp>

#include <hmira/meshes/OpenMeshX.hpp>

#include <hmira/grids/ScalarGridT.hh>
#include <hmira/grids/ScalarGrid_traits.h>

#include <boost/program_options.hpp>

namespace po = boost::program_options;

/**
 * \ingroup tests
 * 
 */

int main(int argc, char **argv)
{	
	int x,y,z;
	float x_size, y_size, z_size;
	std::string input_filename;
	std::string output_filename;
	std::string rasterization;
	
	po::variables_map vm;	
	po::options_description desc("Allowed parameters");

	try
	{
		desc.add_options()
		("help,h","produce help message")
		("fill-holes,f","fill holes on the resulting mesh")
		("rasterize,r", po::value<std::string>(&rasterization)->default_value("full"), "type of rasterization [full|faces|edges]")
		("input-file,i", po::value<std::string>(&input_filename), "input file")
		("output-file,o", po::value<std::string>(&output_filename)->default_value("output.obj"), "output file")
		("x-resolution,x", po::value<int>(&x)->default_value(30), "x resolution")
		("y-resolution,y", po::value<int>(&y)->default_value(30), "y resolution")
		("z-resolution,z", po::value<int>(&z)->default_value(30), "z resolution")
		("x-size,X", po::value<float>(&x_size)->default_value(3.f), "X size")
		("y-size,Y", po::value<float>(&y_size)->default_value(3.f), "Y size")
		("z-size,Z", po::value<float>(&z_size)->default_value(3.f), "Z size");
		
		po::positional_options_description p;
		p.add("input-file,i", -1);

		po::store(po::command_line_parser(argc, argv).
			options(desc).positional(p).run(), vm);
		po::notify(vm);
		
		if (vm.count("help"))
		{
			std::cerr << desc << std::endl;
			return 0;
		}
	}
	catch(po::error& e)
	{
		std::cerr << "ERROR: " << e.what() << std::endl;
		return 1;
	}
	
	if (vm.count("input-file"))
	{
		input_filename = vm["input-file"].as<std::string>();
	}
	else
	{
		std::cerr << "provide and input file\n" << desc << std::endl;
		return -1;
	}

	if (rasterization != "full" && rasterization != "faces" && rasterization != "edges")
	{
		std::cerr << "unknown type of rasterization: \"" << rasterization << "\"\n" << desc << std::endl;
		return -1;
	}
	
	auto fill = (vm.count("fill-holes"));
	
	OpenMeshExtended input_mesh, output_mesh;
	if (!OpenMesh::IO::read_mesh(input_mesh, input_filename))
	{
		std::cerr << "error reading file:" << input_filename << std::endl;
		return 1;
	}
	
	H_DEBUG_STDERR("[GRID] : Buiding empty ScalarGridT<float>")
	H_DEBUG_STDERR("resolution:")
	H_DEBUG_STDERR("x: ", x, " cubes")
	H_DEBUG_STDERR("y: ", y, " cubes")
	H_DEBUG_STDERR("z: ", z, " cubes")
	H_DEBUG_STDERR("[GRID] : Buiding empty ScalarGridT<float>")
	H_DEBUG_STDERR("resolution")
	H_DEBUG_STDERR("x: ",x," cubes")
	H_DEBUG_STDERR("y: ",y," cubes")
	H_DEBUG_STDERR("z: ",z," cubes")
	H_DEBUG_STDERR("Grid size:")
	H_DEBUG_STDERR("x: ", x_size, " cubes")
	H_DEBUG_STDERR("y: ", y_size, " cubes")
	H_DEBUG_STDERR("z: ", z_size, " cubes")

	IsoEx::ScalarGridT<float> sg = IsoEx::ScalarGridT<float>(
		OpenMesh::VectorT<float, 3>( 0, 0, 0 ),
		OpenMesh::VectorT<float, 3>( x_size, 0, 0 ),
		OpenMesh::VectorT<float, 3>( 0, y_size, 0 ),
		OpenMesh::VectorT<float, 3>( 0, 0, z_size ),
		x,
		y,
		z);
	
	auto vx = hmira::algorithms::Voxelize<IsoEx::ScalarGridT<float>, OpenMeshExtended, ScalarGrid_traits<float, IsoEx::ScalarGridT>>(sg, input_mesh);

	if (rasterization == "full")
	{
		vx.process_volume();
		H_DEBUG_STDERR( "[VOXELIZER] : rasterizing full volume" )
	}
	else if(rasterization == "faces")
	{
		H_DEBUG_STDERR( "[VOXELIZER] : rasterizing faces" )
		vx.process_faces();
	}
	else if(rasterization == "edges")
	{
		H_DEBUG_STDERR( "[VOXELIZER] : rasterizing edges" )
		vx.process_edges();
	}

	auto mc = hmira::algorithms::MarchingCubes<IsoEx::ScalarGridT<float>, OpenMeshExtended, ScalarGrid_traits<float, IsoEx::ScalarGridT>>(sg, output_mesh);
	mc.process();

	if (fill)
	{
		hmira::algorithms::fill_holes<OpenMeshExtended, advanced_mesh_traits<OpenMeshExtended>>(output_mesh);
	}

	if (!OpenMesh::IO::write_mesh(output_mesh, output_filename)) 
	{
		std::cerr << "write error\n";
		exit(1);
	}
	else
	{
		H_DEBUG_STDERR( "[MESH] : object: " , output_filename , " written" )
	}

	return 0;
}