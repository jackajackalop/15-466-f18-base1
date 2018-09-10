#!/usr/bin/env python

#based on 'export-sprites.py' and 'glsprite.py' from TCHOW Rainbow; code used is released into the public domain.

#Note: Script meant to be executed from within blender, as per:
#blender --background --python export-meshes.py -- <infile.blend>[:layer] <outfile.p[n][c][t]>

import sys,re

args = []
for i in range(0,len(sys.argv)):
    if sys.argv[i] == '--':
        args = sys.argv[i+1:]

if len(args) != 2:
    print("\n\nUsage:\nblender --background --python export-meshes.py -- <infile.blend>[:layer] <outfile.p[n][c][t][l]>\nExports the meshes referenced by all objects in layer (default 1) to a binary blob, indexed by the names of the objects that reference them. If 'l' is specified in the file extension, only mesh edges will be exported.\n")
    exit(1)

infile = args[0]
layer = 1
m = re.match(r'^(.*):(\d+)$', infile)
if m:
    infile = m.group(1)
    layer = int(m.group(2))
outfile = args[1]

assert layer >= 1 and layer <= 20

print("Will export meshes referenced from layer " + str(layer) + " of '" + infile + "' to '" + outfile + "'.")

class FileType:
    def __init__(self, magic, as_lines = False):
        self.magic = magic
        self.position = (b"p" in magic)
        self.normal = (b"n" in magic)
        self.color = (b"c" in magic)
        self.texcoord = (b"t" in magic)
        self.as_lines = as_lines
        self.vertex_bytes = 0
        if self.position: self.vertex_bytes += 3 * 4
        if self.normal: self.vertex_bytes += 3 * 4
        if self.color: self.vertex_bytes += 4
        if self.texcoord: self.vertex_bytes += 2 * 4

filetype = ".txt"

import bpy
import struct

import argparse


bpy.ops.wm.open_mainfile(filepath=infile)

#meshes to write:
to_write = set()
for obj in bpy.data.objects:
    if obj.layers[layer-1] and obj.type == 'MESH':
        to_write.add(obj.data)

#data contains vertex and normal data from the meshes:
data = b''

#strings contains the mesh names:
strings = b''

#index gives offsets into the data (and names) for each mesh:
index = b''

vertex_count = 0
for obj in bpy.data.objects:
    if obj.data in to_write:
        to_write.remove(obj.data)
    else:
        continue

    mesh = obj.data
    name = mesh.name

    print("Writing '" + name + "'...")
    if bpy.context.mode == 'EDIT':
        bpy.ops.object.mode_set(mode='OBJECT') #get out of edit mode (just in case)

    #make sure object is on a visible layer:
    bpy.context.scene.layers = obj.layers
    #select the object and make it the active object:
    bpy.ops.object.select_all(action='DESELECT')
    obj.select = True
    bpy.context.scene.objects.active = obj

    #apply all modifiers (?):
    bpy.ops.object.convert(target='MESH')

    #TODO might need to get rid of this part
    #subdivide object's mesh into triangles:
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.quads_convert_to_tris(quad_method='BEAUTY', ngon_method='BEAUTY')
    bpy.ops.object.mode_set(mode='OBJECT')

    #compute normals (respecting face smoothing):
    mesh.calc_normals_split()

    #write the data chunk and index chunk to an output blob:
    blob = open(outfile, 'w+')

    #write the mesh triangles:

    #referenced this discussion here because I'm bad at searching the bpy documentation
    #https://blender.stackexchange.com/questions/1311/how-can-i-get-vertex-positions-from-a-mesh
    for vert in obj.data.vertices:
        blob.write(str(vert.co[0])+" "+str(vert.co[1])+" "+str(vert.co[2])+"\n");
    
    blob.write("triangles\n");
    
    for triangle in mesh.polygons:
        a = mesh.loops[triangle.loop_indices[0]].vertex_index;
        b = mesh.loops[triangle.loop_indices[1]].vertex_index;
        c = mesh.loops[triangle.loop_indices[2]].vertex_index;
        blob.write(str(a)+" "+str(b)+" "+str(c)+"\n");

wrote = blob.tell()
blob.close()
