"""Pre-render a Time Stranger model's animations to small transparent frames.

Runs inside Blender (4.5+), headless:

    blender -b --python tools/prerender/render.py -- extracted/agumon.glb renders/agumon [--size 192] [--frames 3]

Input is a plain .glb (decode the viewer's meshopt-compressed files first). Every
animation in the file is rendered at --frames evenly spaced points, unlit (the
texture colours as they are, which is what pixel art wants), from a fixed
three-quarter view facing LEFT like the Vital Bracelet sprites, with one camera
framing shared by every pose so a pose never changes the creature's size.

Nothing this writes may be committed: models and renders are git-ignored here.
"""
import math
import os
import sys

import bpy
from mathutils import Vector

argv = sys.argv[sys.argv.index('--') + 1:]
SRC, OUT = argv[0], argv[1]
SIZE = int(argv[argv.index('--size') + 1]) if '--size' in argv else 192
NFRAMES = int(argv[argv.index('--frames') + 1]) if '--frames' in argv else 3
AZIMUTH = math.radians(35)     # 0 = pure profile; >0 swings the camera round to show the face
ELEVATION = math.radians(12)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=SRC)
scene = bpy.context.scene


def unlit(mat):
    """Base colour texture -> emission, its alpha -> transparency."""
    if not mat.use_nodes:
        return
    nt = mat.node_tree
    bsdf = next((n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED'), None)
    out = next((n for n in nt.nodes if n.type == 'OUTPUT_MATERIAL'), None)
    if not bsdf or not out:
        return
    col_link = next((l for l in nt.links if l.to_socket == bsdf.inputs['Base Color']), None)
    alpha_link = next((l for l in nt.links if l.to_socket == bsdf.inputs['Alpha']), None)
    emit = nt.nodes.new('ShaderNodeEmission')
    if col_link:
        nt.links.new(col_link.from_socket, emit.inputs['Color'])
    else:
        emit.inputs['Color'].default_value = bsdf.inputs['Base Color'].default_value
    clear = nt.nodes.new('ShaderNodeBsdfTransparent')
    mix = nt.nodes.new('ShaderNodeMixShader')
    if alpha_link:
        nt.links.new(alpha_link.from_socket, mix.inputs['Fac'])
    else:
        mix.inputs['Fac'].default_value = bsdf.inputs['Alpha'].default_value
    nt.links.new(clear.outputs[0], mix.inputs[1])
    nt.links.new(emit.outputs[0], mix.inputs[2])
    nt.links.new(mix.outputs[0], out.inputs['Surface'])


def fix_eye(mat):
    """The eye mesh is a shine overlay (a sheet of highlight dots, ...e01h) over
    the iris painted in the body texture. The file marks it opaque, which hides
    the iris under a white disc: drive its transparency from the texture alpha."""
    nt = mat.node_tree
    tex = next((n for n in nt.nodes if n.type == 'TEX_IMAGE'), None)
    mix = next((n for n in nt.nodes if n.type == 'MIX_SHADER'), None)
    if tex and mix:
        nt.links.new(tex.outputs['Alpha'], mix.inputs['Fac'])


for m in bpy.data.materials:
    if 'outline' in m.name:
        continue     # drawn in post instead: the game pushes this hull out in its shader
    unlit(m)
    if m.name.startswith('eye'):
        fix_eye(m)

for o in [o for o in scene.objects if o.type == 'MESH']:
    if any(s.material and 'outline' in s.material.name for s in o.material_slots):
        o.hide_render = True

arm = next((o for o in scene.objects if o.type == 'ARMATURE'), None)
meshes = [o for o in scene.objects if o.type == 'MESH']
actions = sorted(bpy.data.actions, key=lambda a: a.name)
if arm and not arm.animation_data:
    arm.animation_data_create()


def pose(action, t):
    """Put the armature at fraction t (0..1) of action; returns the frame."""
    if arm:
        arm.animation_data.action = action
        # Blender 4.4+ slotted actions: bind the action's first slot to the armature
        if hasattr(action, 'slots') and len(action.slots):
            arm.animation_data.action_slot = action.slots[0]
    f0, f1 = action.frame_range
    f = round(f0 + (f1 - f0) * t)
    scene.frame_set(f)
    return f


def world_bbox():
    dg = bpy.context.evaluated_depsgraph_get()
    lo, hi = Vector((1e9,) * 3), Vector((-1e9,) * 3)
    for o in meshes:
        ev = o.evaluated_get(dg)
        me = ev.to_mesh()
        for v in me.vertices:
            w = ev.matrix_world @ v.co
            lo = Vector(map(min, lo, w))
            hi = Vector(map(max, hi, w))
        ev.to_mesh_clear()
    return lo, hi


# Frame on the idle pose (the battle idle, "bn01", if present), with headroom for bigger poses.
idle = next((a for a in actions if a.name.endswith('_bn01')), actions[0] if actions else None)
if idle:
    pose(idle, 0.0)
lo, hi = world_bbox()
centre = (lo + hi) / 2
extent = max(hi - lo)

cam_data = bpy.data.cameras.new('cam')
cam_data.type = 'ORTHO'
cam_data.ortho_scale = extent * 1.45
cam = bpy.data.objects.new('cam', cam_data)
scene.collection.objects.link(cam)
scene.camera = cam
# Characters face -Y after import; a camera on +X therefore sees them facing screen-left.
d = extent * 4
cam.location = centre + Vector((d * math.cos(AZIMUTH) * math.cos(ELEVATION),
                                -d * math.sin(AZIMUTH) * math.cos(ELEVATION),
                                d * math.sin(ELEVATION)))
cam.rotation_euler = (centre - cam.location).to_track_quat('-Z', 'Y').to_euler()

engines = {e.identifier for e in bpy.types.RenderSettings.bl_rna.properties['engine'].enum_items}
scene.render.engine = 'BLENDER_EEVEE_NEXT' if 'BLENDER_EEVEE_NEXT' in engines else 'BLENDER_EEVEE'
scene.render.film_transparent = True
scene.render.resolution_x = scene.render.resolution_y = SIZE
scene.render.resolution_percentage = 100
scene.render.filter_size = 0.5            # crisp edges: this gets pixelated afterwards
scene.view_settings.view_transform = 'Standard'
scene.render.image_settings.file_format = 'PNG'
scene.render.image_settings.color_mode = 'RGBA'

os.makedirs(OUT, exist_ok=True)
for a in actions:
    short = a.name.split('_', 1)[1] if '_' in a.name else 'base'
    for i in range(NFRAMES):
        t = 0.0 if NFRAMES == 1 else i / (NFRAMES - 1) * 0.9
        f = pose(a, t)
        scene.render.filepath = os.path.join(OUT, f'{short}_{i}.png')
        bpy.ops.render.render(write_still=True)
    print(f'rendered {a.name} ({NFRAMES} frames)', flush=True)
