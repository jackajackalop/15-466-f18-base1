#include "WalkMesh.hpp"
#include <iostream>
#include <fstream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

WalkMesh::WalkMesh(std::string const &filename) {
	std::ifstream inFile(filename, std::ios::in);
	bool tri = false;
	char oneline[256];
	while(inFile){
		inFile.getline(oneline, 256);
		if(!tri && strncmp(oneline, "triangles", 9) == 0) 
			tri = true;
		else{
			//string splitting code from 
			//https://www.geeksforgeeks.org/tokenizing-a-string-cpp/
			char *token = strtok(oneline, " ");
			std::vector<std::string> pts;
			while(token != NULL){
				pts.emplace_back(token);
				token = strtok(NULL, " ");
			}
			if(!tri){
				float a = std::stof(pts[0]);
				float b = std::stof(pts[1]);
				float c = std::stof(pts[2]);
				vertices.emplace_back(glm::vec3(a, b, c));
			}else{
				uint32_t a = std::stoi(pts[0]);
				uint32_t b = std::stoi(pts[1]);
				uint32_t c = std::stoi(pts[2]);
				triangles.emplace_back(glm::uvec3(a, b, c));
				next_vertex[glm::uvec2(a, b)] = c;
				next_vertex[glm::uvec2(a, c)] = b;
				next_vertex[glm::uvec2(c, b)] = a;
			}
		}
	}
	inFile.close();
}

WalkMesh::WalkPoint WalkMesh::start(glm::vec3 const &world_point) const {
	WalkPoint closest;
	//TODO: iterate through triangles
	//TODO: for each triangle, find closest point on triangle to world_point
	//TODO: if point is closest, closest.triangle gets the current triangle, closest.weights gets the barycentric coordinates
	return closest;
}

void WalkMesh::walk(WalkPoint &wp, glm::vec3 const &step) const {
	//TODO: project step to barycentric coordinates to get weights_step
	glm::vec3 weights_step;

	//TODO: when does wp.weights + t * weights_step cross a triangle edge?
	float t = 1.0f;

	if (t >= 1.0f) { //if a triangle edge is not crossed
		//TODO: wp.weights gets moved by weights_step, nothing else needs to be done.

	} else { //if a triangle edge is crossed
		//TODO: wp.weights gets moved to triangle edge, and step gets reduced
		//if there is another triangle over the edge:
		//TODO: wp.triangle gets updated to adjacent triangle
		//TODO: step gets rotated over the edge
		//else if there is no other triangle over the edge:
		//TODO: wp.triangle stays the same.
		//TODO: step gets updated to slide along the edge
	}
}
