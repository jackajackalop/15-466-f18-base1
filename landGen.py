import bpy
import random
import copy

def createMesh(name, origin, verts, edges, faces):
    # Create mesh and object
    me = bpy.data.meshes.new(name+'Mesh')
    ob = bpy.data.objects.new(name, me)
    ob.location = origin
    ob.show_name = True
    # Link object to scene
    bpy.context.scene.objects.link(ob)
    
    # Create mesh from given verts, edges, faces. Either edges or
    # faces should be [], or you ask for problems
    me.from_pydata(verts, edges, faces)

    # Update mesh with new data
    me.update(calc_edges=True)
    return ob
def myrand():
    return random.random()*100

def vertsGen():
    verts =[]
    #vert0
    verts.append((-myrand(),-myrand(),myrand()))
    #vert1
    verts.append((-myrand(),-myrand(),-myrand()))
    #vert2
    verts.append((myrand(),-myrand(),-myrand()))
    #vert3
    verts.append((myrand(),-myrand(),myrand()))
    #vert4
    verts.append((-myrand(),myrand(),myrand()))
    #vert5
    verts.append((-myrand(),myrand(),-myrand()))
    #vert6
    verts.append((myrand(),myrand(),-myrand()))
    #vert7
    verts.append((myrand(),myrand(),myrand()))
    
    return verts

def facesGen(verts):
    faces = [[0,1,2,3], [1,2,6,5], [1,0,4,5], [2,3,7,6], [0,3,7,4], [4,5,6,7]]
    return faces

def run(origin):
    for i in range(1):
        verts1 = vertsGen()
        faces1 = facesGen(verts1)
        ob1 = createMesh('Land'+str(i), origin, verts1, [], faces1)
    return

if __name__ == "__main__":
    run((0,0,0))

