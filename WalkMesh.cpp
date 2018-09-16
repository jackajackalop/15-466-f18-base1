#include "WalkMesh.hpp"
#include <algorithm>
#include <iostream>
#include <fstream>
#include "read_chunk.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

WalkMesh::WalkMesh(std::string const &filename) {
	std::ifstream inFile(filename, std::ios::in);
	bool tri = false;
	bool normals = false;
	char oneline[256];
	while(inFile){
		inFile.getline(oneline, 256);
		if(!normals && strncmp(oneline, "normals", 7) == 0) 
			normals = true;
		else if(!tri && strncmp(oneline, "triangles", 9) == 0) {
			normals = false;
			tri = true;
		}else{
			//string splitting code from 
			//https://www.geeksforgeeks.org/tokenizing-a-string-cpp/
			char *token = strtok(oneline, " ");
			std::vector<std::string> pts;
			while(token != NULL){
				pts.emplace_back(token);
				token = strtok(NULL, " ");
			}
			if(!tri && !normals && !pts.empty()){
				float a = std::stof(pts[0]);
				float b = std::stof(pts[1]);
				float c = std::stof(pts[2]);
				vertices.emplace_back(glm::vec3(a, b, c));
			}else if(normals && !pts.empty()){
				float a = std::stof(pts[0]);
				float b = std::stof(pts[1]);
				float c = std::stof(pts[2]);
				vertex_normals.emplace_back(glm::vec3(a, b, c));
			}else if(tri && !pts.empty()){
				uint32_t a = std::stoi(pts[0]);
				uint32_t b = std::stoi(pts[1]);
				uint32_t c = std::stoi(pts[2]);
				triangles.emplace_back(glm::uvec3(a, b, c));
				next_vertex[glm::uvec2(a, b)] = c;
				next_vertex[glm::uvec2(b, c)] = a;
				next_vertex[glm::uvec2(c, a)] = b;
			}
		}
	}
	inFile.close();
}

//math is hard my brain is slow so i looked at this
//https://www.gamedev.net/forums/topic/552906-closest-point-on-triangle/
glm::vec3 closest_pt(glm::vec3 pa, glm::vec3 pb, glm::vec3 pc, glm::vec3 p){
	glm::vec3 edge0 = pb-pa;
	glm::vec3 edge1 = pc-pa;
	glm::vec3 v0 = pa-p;

	float a = glm::dot(edge0, edge0);
	float b = glm::dot(edge0, edge1);
	float c = glm::dot(edge1, edge1);
	float d = glm::dot(edge0, v0);
	float e = glm::dot(edge1, v0);

	float det = a*c - b*b;
	float s = b*e - c*d;
	float t = b*d - a*e;

	if(s+t<det){
		if(s<0.0f){
			if(t<0.0f){
				if(d<0.0f){
					s = glm::clamp(-d/a, 0.0f, 1.0f);
					t = 0.0f;
				}else{
					s = 0.0f;
					t = glm::clamp(-e/c, 0.0f, 1.0f);
				}
			}else{
				s = 0.0f;
				t = glm::clamp(-e/c, 0.0f, 1.0f);
			}
		}else if(t<0.0f){
			s = glm::clamp(-d/a, 0.0f, 1.0f);
			t = 0.0f;
		}else{
			float invDet = 1.0f/det;
			s*=invDet;
			t*=invDet;
		}
	}else{
		if(s<0.0f){
			float tmp0 = b+d;
			float tmp1 = c+e;
			if(tmp1 > tmp0){
				float numer = tmp1 - tmp0;
				float denom = a-2*b+c;
				s = glm::clamp(numer/denom, 0.0f, 1.0f);
				t = 1-s;
			}else{
				t = glm::clamp(-e/c, 0.0f, 1.0f);
				s = 0.0f;
			}
		}else if(t<0.0f){
			if(a+d>b+e){
				float numer = c+e-b-d;
				float denom = a-2*b+c;
				s = glm::clamp(numer/denom, 0.0f,1.0f);
				t = 1-s;
			}else{
				s = glm::clamp(-e/c, 0.0f, 1.0f);
				t = 0.0f;
			}

		}else{
			float numer = c+e-b-d;
			float denom = a-2*b+c;
			s = glm::clamp(numer/denom, 0.0f, 1.0f);
			t = 1.0f-s;
		}
	}

	glm::vec3 result = pa+s*edge0+t*edge1;
	return result;
}

float distance(glm::vec3 a, glm::vec3 b){
	return sqrt(pow(a[0]-b[0], 2)+ pow(a[1]-b[1], 2) + pow(a[2]+b[2], 2));
}

//referenced this explanation of quickly getting barycentric coord
//https://gamedev.stackexchange.com/questions/23743/whats-the-most-efficient-way-to-find-barycentric-coordinates
glm::vec3 get_barycentric(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 p){
	glm::vec3 v0 = b-a;
	glm::vec3 v1 = c-a;
	glm::vec3 v2 = p-a;
	float d00 = glm::dot(v0, v0);
	float d01 = glm::dot(v0, v1);
	float d11 = glm::dot(v1, v1);
	float d20 = glm::dot(v2, v0);
	float d21 = glm::dot(v2, v1);
	float denom = d00*d11-d01*d01;
	float v = (d11*d20-d01*d21)/denom;
	float w = (d00*d21-d01*d20)/denom;
	float u = 1.0f-v-w;
	return glm::vec3(u, v, w);
}

WalkMesh::WalkPoint WalkMesh::start(glm::vec3 const &world_point) const {
	WalkPoint closest;
	float minDist = -1.0f;
	for(glm::uvec3 current : triangles){
		glm::vec3 a = vertices[current[0]];
		glm::vec3 b = vertices[current[1]];
		glm::vec3 c = vertices[current[2]];

		glm::vec3 cp = closest_pt(a, b, c, world_point);
		float dist = distance(cp,world_point);
		if(minDist<0 || minDist>dist){
			minDist = dist;
			closest.triangle = current;
			closest.weights = get_barycentric(a, b, c, cp);
		}
	}
	return closest;
}


bool cross(float x){
	return !std::isfinite(x)||(x<=1 && x>=0);
}
glm::quat WalkMesh::walk(WalkPoint &wp, glm::vec3 const &step) const {
	//project step to barycentric coordinates to get weights_step

	glm::vec3 weights_step = world_point(wp)+step;
	glm::vec3 a = vertices[wp.triangle.x];
	glm::vec3 b = vertices[wp.triangle.y];
	glm::vec3 c = vertices[wp.triangle.z];
	//when does wp.weights + t * weights_step cross a triangle edge?
	glm::vec3 proj_step = closest_pt(a, b, c, weights_step);
	glm::vec3 bary = get_barycentric(a, b, c, proj_step);
	float ta = -wp.weights[0]/bary[0];
	float tb = -wp.weights[1]/bary[1];
	float tc = -wp.weights[2]/bary[2];
	glm::vec3 rot_edge;
	if (!cross(ta) && !cross(tb) && !cross(tc)) { 
		//if a triangle edge is not crossed
		//wp.weights gets moved by weights_step, nothing else needs to be done.
		wp.weights = bary;
	} else { //if a triangle edge is crossed
		//wp.weights gets moved to triangle edge, and step gets reduced
		glm::vec3 bump = glm::vec3(0.0f, 0.0f, 0.0f);
		uint32_t curr1, curr2;
		if(cross(ta)){
			curr1 = wp.triangle.z;
			curr2 = wp.triangle.y;
			bump = glm::vec3(0.001f,0.0f,0.0f);
		}else if(cross(tb)){
			curr1 = wp.triangle.x;
			curr2 = wp.triangle.z;
			bump = glm::vec3(0.0f,0.001f,0.0f);
		}else{
			curr1 = wp.triangle.y;
			curr2 = wp.triangle.x;
			bump = glm::vec3(0.0f,0.0f,0.001f);
		}
		rot_edge = vertices[curr1]-vertices[curr2];

		glm::vec3 old_normal = world_normal(wp);
		auto next_v = next_vertex.find(glm::uvec2(curr1, curr2));
		//if there is another triangle over the edge:
		if(next_v != next_vertex.end()){
			//wp.triangle gets updated to adjacent triangle
			uint32_t newx = curr1;
			uint32_t newy = curr2;
			uint32_t newz = next_v->second;
			wp.triangle = glm::vec3(newx, newy, newz);
			a = vertices[wp.triangle.x];
			b = vertices[wp.triangle.y];
			c = vertices[wp.triangle.z];
			wp.weights = closest_pt(a, b, c, proj_step);
			wp.weights = get_barycentric(a, b, c, wp.weights)
				+ bump;
			//step gets rotated over the edge

			glm::vec3 new_normal = world_normal(wp);
			if(old_normal!=new_normal){
				float numer = glm::dot(old_normal, new_normal);
				//normally would also divide by the product of the lengths 
				//of the 2, but these vectors are normalized and equal 1
				float angle = (std::acos(numer));
				glm::quat rotationQuat = glm::angleAxis(-angle,
						glm::vec3(1.0f, 0.0f, 0.0f));
				old_normal = new_normal;
				return rotationQuat;
			}
		}else{
			std::cout<<"shit lmao"<<std::endl;
			//else if there is no other triangle over the edge:
			//wp.triangle stays the same.
			//TODO: step gets updated to slide along the edge
		}
	}
	return glm::angleAxis(0.0f, rot_edge);
}

