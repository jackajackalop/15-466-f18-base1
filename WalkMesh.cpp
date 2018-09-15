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
            if(!tri && !pts.empty()){
                float a = std::stof(pts[0]);
                float b = std::stof(pts[1]);
                float c = std::stof(pts[2]);
                vertices.emplace_back(glm::vec3(a, b, c));
            }else if(!pts.empty()){
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

bool adjacent_tri(glm::uvec3 a, glm::uvec3 b){
    int count = 0;
    if(a[0]==b[0]){
        count++;
        if(a[0]==b[1]||b[0]==a[1]||a[1]==b[1]||a[1]==b[2] 
                ||a[2] == b[2] || a[2] == b[1]||a[2]==b[0]||a[0]==b[2])
            count++;
    }else if(a[0] == b[1]){
        count++;
        if(a[0]==b[0]||b[0]==a[1]||a[1]==b[1]||a[1]==b[2] 
                ||a[2] == b[2] || a[2] == b[1]||a[2]==b[0]||a[0]==b[2])
            count++;
    }else if(a[0] == b[2]){
        count++;
        if(a[0]==b[1]||b[0]==a[1]||a[1]==b[1]||a[1]==b[2] 
                ||a[2] == b[2] || a[2] == b[1]||a[2]==b[0]||a[0]==b[0])
            count++;
    }

    return (count>=2);
}

void WalkMesh::walk(WalkPoint &wp, glm::vec3 const &step) const {
    //TODO: project step to barycentric coordinates to get weights_step
    glm::vec3 world_step = step+world_point(wp);

    glm::vec3 a = vertices[wp.triangle.x];
    glm::vec3 b = vertices[wp.triangle.y];
    glm::vec3 c = vertices[wp.triangle.z];
    glm::vec3 proj_pt = closest_pt(a, b, c, world_step);
    glm::vec3 new_step = get_barycentric(a, b, c, proj_pt);

    bool crossed = false;
    int which_bary;
    if(new_step[0]<0||new_step[0]>1){
        crossed = true;
        which_bary = 0;
    }else if(new_step[1]<0||new_step[1]>1){
        crossed = true;
        which_bary = 1;    
    }else if(new_step[2]<0||new_step[2]>1){
        crossed = true;
        which_bary = 2;
    }

    if (!crossed) { //if a triangle edge is not crossed
        wp.weights = new_step;
    } else { //if a triangle edge is crossed
        //TODO: wp.weights gets moved to triangle edge, 
        //and step gets reduced
        glm::vec3 step_adj = step;
        if(new_step[which_bary]>1){
            step_adj[which_bary] = new_step[which_bary]-1.0f;
            new_step[which_bary] = 1.0f;
        }else{
            step_adj[which_bary] = 0.0f-new_step[which_bary];
            new_step[which_bary] = 0.0f;
        }
        //if there is another triangle over the edge:

        float minDist = -1.0f;
        glm::uvec3 closest_tri;
        for(glm::uvec3 current : triangles){
            glm::vec3 a = vertices[current[0]];
            glm::vec3 b = vertices[current[1]];
            glm::vec3 c = vertices[current[2]];
            glm::vec3 cp = closest_pt(a, b, c, world_step);
            float dist = distance(cp, world_step);
            if(minDist<0 || minDist>dist){
                minDist = dist;
                closest_tri = current;
            }
        }

        if( adjacent_tri(wp.triangle, closest_tri)){
            wp.triangle = closest_tri;
            //TODO: step gets rotated over the edge
        }else{ //else if there is no other triangle over the edge:
            //TODO: step gets updated to slide along the edge
        }
    }
}
