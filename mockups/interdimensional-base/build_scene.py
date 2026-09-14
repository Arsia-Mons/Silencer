"""Build the artist cutaway with Blender's bundled Python; no add-ons required."""

import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from source_map import read_map

SOURCE, _ = read_map()
SCENE = bpy.context.scene


def collection(name):
    result = bpy.data.collections.new(name)
    SCENE.collection.children.link(result)
    return result


def material(name, color, metal=0.0, rough=0.45, glow=0.0):
    result = bpy.data.materials.new(name)
    result.diffuse_color = (*color, 1)
    result.use_nodes = True
    shader = result.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Base Color"].default_value = (*color, 1)
    shader.inputs["Metallic"].default_value = metal
    shader.inputs["Roughness"].default_value = rough
    shader.inputs["Emission Color"].default_value = (*color, 1)
    shader.inputs["Emission Strength"].default_value = glow
    return result


def place(obj, name, group, mat=None):
    obj.name = name
    for owner in list(obj.users_collection):
        owner.objects.unlink(obj)
    group.objects.link(obj)
    if mat:
        obj.data.materials.append(mat)
    return obj


def mesh(name, vertices, faces, group, mat, bevel=0):
    data = bpy.data.meshes.new(name)
    data.from_pydata(vertices, [], faces)
    data.update()
    obj = bpy.data.objects.new(name, data)
    group.objects.link(obj)
    data.materials.append(mat)
    if bevel:
        modifier = obj.modifiers.new("Machined edges", "BEVEL")
        modifier.width = bevel
        modifier.segments = 2
        obj.modifiers.new("Weighted corner normals", "WEIGHTED_NORMAL")
    return obj


def box(name, pos, size, mat, group=None, bevel=0.025):
    group = group if group is not None else DETAIL
    x, y, z = (value / 2 for value in size)
    vertices = [(-x, -y, -z), (-x, -y, z), (-x, y, -z), (-x, y, z),
                (x, -y, -z), (x, -y, z), (x, y, -z), (x, y, z)]
    faces = [(0, 4, 6, 2), (1, 3, 7, 5), (0, 1, 5, 4),
             (2, 6, 7, 3), (0, 2, 3, 1), (4, 5, 7, 6)]
    obj = mesh(name, vertices, faces, group, mat, bevel)
    obj.location = pos
    return obj


def cylinder(name, pos, radius, depth, mat, group=None, face_front=False):
    bpy.ops.mesh.primitive_cylinder_add(vertices=24, radius=radius, depth=depth, location=pos)
    obj = place(bpy.context.object, name, group if group is not None else PROPS, mat)
    if face_front:
        obj.rotation_euler[0] = math.pi / 2
    bevel = obj.modifiers.new("Rounded rim", "BEVEL")
    bevel.width, bevel.segments = 0.02, 2
    obj.modifiers.new("Weighted normals", "WEIGHTED_NORMAL")
    return obj


def tube(name, points, radius, mat, group=None):
    data = bpy.data.curves.new(name, "CURVE")
    data.dimensions = "3D"
    data.resolution_u = 1
    data.bevel_depth, data.bevel_resolution = radius, 2
    path = data.splines.new("POLY")
    path.points.add(len(points) - 1)
    for point, value in zip(path.points, points):
        point.co = (*value, 1)
    obj = bpy.data.objects.new(name, data)
    (group if group is not None else DETAIL).objects.link(obj)
    data.materials.append(mat)
    return obj


def text(name, value, pos, size, mat, group=None):
    data = bpy.data.curves.new(name, "FONT")
    data.body, data.size = value, size
    data.align_x = "CENTER"
    data.extrude = 0.001
    obj = bpy.data.objects.new(name, data)
    (group if group is not None else DETAIL).objects.link(obj)
    data.materials.append(mat)
    obj.location = pos
    obj.rotation_euler = (math.pi / 2, 0, 0)
    return obj


def coords(x, y, depth=0):
    return ((x - 448) / 64, depth, (1094 - y) / 64)


def arch(name, x, floor, width, height, mat, depth=1.75):
    radius = width / 2
    shoulder = floor + height - radius
    points = [(x - radius, depth, floor), (x - radius, depth, shoulder)]
    points += [(x + math.cos(t) * radius, depth, shoulder + math.sin(t) * radius)
               for t in [math.pi - i * math.pi / 24 for i in range(25)]]
    points.append((x + radius, depth, floor))
    tube(name, points, 0.085, mat)
    tube(name + " / inner light", [(a, b - 0.07, c) for a, b, c in points], 0.022, CYAN)


def screen(name, x, y, z, width, height, color=None):
    color = color if color is not None else CYAN
    box(name + " / bezel", (x, y, z), (width, 0.18, height), BLACK, PROPS, 0.055)
    box(name + " / display", (x, y - 0.105, z),
        (width - 0.12, 0.025, height - 0.12), SCREEN, PROPS, 0.018)
    for row in range(3):
        box(name + f" / readout {row}", (x - width * 0.12, y - 0.13, z + height * (0.22 - row * 0.19)),
            (width * (0.46 - row * 0.07), 0.012, 0.023), color, PROPS, 0)
    box(name + " / status", (x + width * 0.29, y - 0.13, z), (0.04, 0.012, height * 0.46),
        color, PROPS, 0)


def console(name, x, floor, color, label, width=1.5):
    box(name + " / plinth", (x, 0.7, floor + 0.10), (width + 0.15, 1.45, 0.20), EDGE, PROPS)
    box(name + " / chassis", (x, 0.9, floor + 0.69), (width, 0.85, 1.25), HULL, PROPS, 0.09)
    screen(name, x, 0.41, floor + 1.00, width - 0.18, 0.7, color)
    box(name + " / keyboard shelf", (x, 0.14, floor + 0.64), (width - 0.08, 0.6, 0.12), EDGE, PROPS)
    for key in range(5):
        box(name + f" / key {key}", (x - 0.36 + key * 0.18, -0.02, floor + 0.72),
            (0.12, 0.13, 0.026), WHITE, PROPS, 0.004)
    text(name + " / identifier", label, (x, 0.44, floor + 0.25), 0.10, color, PROPS)


def area(name, pos, target, energy, color, size):
    data = bpy.data.lights.new(name, "AREA")
    data.energy, data.color, data.shape, data.size = energy, color, "DISK", size
    obj = bpy.data.objects.new(name, data)
    STUDIO.objects.link(obj)
    obj.location = pos
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat("-Z", "Y").to_euler()


def camera(name, pos, target, scale):
    data = bpy.data.cameras.new(name)
    obj = bpy.data.objects.new(name, data)
    STUDIO.objects.link(obj)
    obj.location = pos
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat("-Z", "Y").to_euler()
    data.type, data.ortho_scale, data.lens = "ORTHO", scale, 50
    data.clip_end = 400
    return obj


bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
for existing in list(bpy.data.collections):
    bpy.data.collections.remove(existing)

COLLISION = collection("01 SOURCE / exact collision extrusion")
ANCHORS = collection("02 SOURCE / actor anchors")
SHELL = collection("03 CONCEPT / rear shell")
DETAIL = collection("04 CONCEPT / architecture and trims")
PROPS = collection("05 CONCEPT / equipment")
STUDIO = collection("06 PRESENTATION / cameras and lighting")
REFERENCE = collection("07 REFERENCE / original tile plane [hidden]")

HULL = material("Hull / blue graphite", (0.095, 0.16, 0.205), 0.65)
EDGE = material("Edges / brushed titanium", (0.28, 0.39, 0.44), 0.72, 0.32)
BLACK = material("Recesses / near black", (0.012, 0.024, 0.035), 0.2)
PANEL = material("Panels / charcoal", (0.050, 0.083, 0.11), 0.55)
FLOOR = material("Decks / desaturated steel", (0.13, 0.205, 0.25), 0.65)
WHITE = material("Ceramic / ivory", (0.64, 0.75, 0.77), 0.25)
CYAN = material("Signal / ice cyan", (0.07, 0.7, 1.0), glow=3)
AMBER = material("Signal / amber", (1.0, 0.39, 0.075), glow=2)
VIOLET = material("Signal / dimensional violet", (0.48, 0.12, 0.95), glow=3)
GREEN = material("Signal / medical mint", (0.12, 0.95, 0.51), glow=2)
SCREEN = material("Display / deep teal", (0.01, 0.07, 0.10), metal=0.2, glow=0.6)
GROUND = material("Presentation / midnight", (0.012, 0.022, 0.04), rough=0.7)

# Collision silhouettes are exact. Bevels and all architectural dressing live
# separately so artists can recover the original side-on outline at any time.
for p in SOURCE["platforms"]:
    x1, _, z1 = coords(p["x1"], p["y1"])
    x2, _, z2 = coords(p["x2"], p["y2"])
    name = f"P{p['index']:02d} / {p['kind']}"
    if p["kind"] == "ladder":
        obj = mesh(name, [(x1, 0, z1), (x2, 0, z1), (x2, 0, z2), (x1, 0, z2)],
                   [(0, 1, 2, 3)], COLLISION, AMBER)
        obj.hide_render = True
        obj.hide_set(True)
        for x in (x1 + 0.06, x2 - 0.06):
            tube("Ladder / rail", [(x, 0, z2), (x, 0, z1 + 0.65)], 0.04, EDGE)
        for rung in range(17):
            z = z2 + rung * (z1 - z2) / 16
            tube("Ladder / rung", [(x1 + 0.06, 0, z), (x2 - 0.06, 0, z)], 0.027, AMBER)
    elif z1 == z2:
        obj = mesh(name, [(x1, -2, z1), (x2, -2, z1), (x2, 2, z1), (x1, 2, z1)],
                   [(0, 1, 2, 3)], COLLISION, FLOOR)
        obj.hide_render = True
        obj.hide_set(True)
    else:
        obj = box(name, ((x1 + x2) / 2, 0, (z1 + z2) / 2),
                  (x2 - x1, 4, z1 - z2), FLOOR, COLLISION, 0)
        if x2 - x1 > 2 and z1 - z2 < 1.3:
            box(name + " / front fascia", ((x1 + x2) / 2, -2.035, (z1 + z2) / 2),
                (x2 - x1 - 0.05, 0.09, (z1 - z2) * 0.72), HULL)
            for x in range(math.ceil(x1), math.floor(x2)):
                box(name + " / fascia joint", (x, -2.09, (z1 + z2) / 2),
                    (0.035, 0.018, (z1 - z2) * 0.66), EDGE, bevel=0)
            if p["index"] in (6, 7, 12, 16):
                box(name + " / deck guidance", ((x1 + x2) / 2, -1.90, z1 + 0.012),
                    (x2 - x1 - 0.12, 0.045, 0.024), CYAN, bevel=0)
                for x in range(math.ceil(x1), math.floor(x2)):
                    box(name + " / deck seam", (x, -0.05, z1 + 0.006),
                        (0.018, 3.4, 0.012), BLACK, bevel=0)
    obj["source_index"] = p["index"]
    obj["source_bounds_px"] = [p[k] for k in ("x1", "y1", "x2", "y2")]
    obj["source_types"] = [p["type1"], p["type2"]]

# These back walls are invented depth, fitted behind the original room outline.
rooms = [
    ("01 / arrival and support", 490, 525, 1088, 717),
    ("02 / operations hall", 1152, 383, 2142, 717),
    ("03 / research deck", 1152, 781, 2112, 1038),
    ("04 / data return", 2240, 390, 2496, 717),
    ("Left link", 1088, 589, 1152, 717),
    ("Right link", 2142, 589, 2240, 717),
]
for name, sx1, sy1, sx2, sy2 in rooms:
    x1, _, top = coords(sx1, sy1)
    x2, _, bottom = coords(sx2, sy2)
    box(name + " / backing", ((x1 + x2) / 2, 2.2, (top + bottom) / 2),
        (x2 - x1, 0.4, top - bottom), BLACK, SHELL, 0)
    columns = math.ceil(x2 - x1)
    rows = math.ceil(top - bottom)
    for column in range(columns):
        for row in range(rows):
            pw, ph = (x2 - x1) / columns, (top - bottom) / rows
            x, z = x1 + (column + 0.5) * pw, bottom + (row + 0.5) * ph
            box(name + f" / panel {column:02d}-{row:02d}", (x, 1.975, z),
                (pw - 0.05, 0.14, ph - 0.05), PANEL, SHELL, 0.045)
            if (column + row) % 3 == 0:
                box("Panel / inset rib", (x + pw * 0.25, 1.875, z),
                    (0.075, 0.10, ph * 0.55), HULL, SHELL)
    if x2 - x1 > 3:
        for x in range(math.ceil(x1 + 0.5), math.floor(x2), 2):
            box(name + " / structural rib", (x, 1.80, (bottom + top) / 2),
                (0.13, 0.25, top - bottom - 0.10), EDGE)
            box(name + " / ceiling lamp", (x + 0.35, 1.6, top - 0.12),
                (0.70, 0.35, 0.08), CYAN)
        box(name + " / service conduit", ((x1 + x2) / 2, 1.76, bottom + 0.25),
            (x2 - x1 - 0.1, 0.09, 0.07), EDGE)

main_floor = coords(0, 717)[2]
tech_floor = coords(0, 1038)[2]
upper_floor = coords(0, 518)[2]

for label, x, floor, width, height in [
    ("Surveillance alcove", (626 - 448) / 64, main_floor, 2.8, 2.8),
    ("Support alcove", (895 - 448) / 64, main_floor, 2.7, 2.8),
    ("Main inventory alcove", (1343 - 448) / 64, main_floor, 2.9, 2.9),
    ("Upper inventory alcove", (1917 - 448) / 64, upper_floor, 2.7, 2.0),
    ("Return chamber alcove", (2365 - 448) / 64, main_floor, 3.0, 4.1),
]:
    arch(label, x, floor, width, height, EDGE)

for index, actor in enumerate(SOURCE["actors"]):
    name = f"A{index:02d} / {actor['name']}"
    x, _, z = coords(actor["x"], actor["y"])
    root = bpy.data.objects.new(name + " [source anchor]", None)
    ANCHORS.objects.link(root)
    root.location = (x, 0, z)
    root.empty_display_type = "SPHERE"
    root.empty_display_size = 0.10
    for key, value in actor.items():
        root[key] = value
    before = set(PROPS.objects) | set(DETAIL.objects)
    kind = actor["id"]
    if kind == 67:
        cylinder(name + " / mount", (x, 1.57, z), 0.28, 0.26, EDGE, face_front=True)
        cylinder(name + " / turret", (x, 1.35, z), 0.17, 0.3, HULL, face_front=True)
        cylinder(name + " / lens", (x, 1.16, z), 0.055, 0.025, AMBER, face_front=True)
        box(name + " / bracket", (x, 1.84, z - 0.1), (0.55, 0.20, 0.12), EDGE, PROPS)
    elif kind == 66:
        labels = ("RESEARCH / 01", "UPGRADES / 02", "SYSTEMS / 03")
        colors = (CYAN, AMBER, VIOLET)
        console(name, x, z, colors[actor["type"]], labels[actor["type"]], 2.15)
        box(name + " / rear cabinet", (x, 1.6, z + 2.35), (2.6, 0.45, 1.35), HULL, PROPS, 0.1)
        screen(name + " / telemetry", x, 1.30, z + 2.45, 2.2, 0.85, colors[actor["type"]])
    elif kind == 56:
        console(name, x, z, CYAN, "INVENTORY", 1.65)
        box(name + " / equipment locker", (x + 0.90, 1.30, z + 0.8),
            (0.5, 0.75, 1.6), EDGE, PROPS, 0.09)
    elif kind == 50:
        screen(name + " / main feed", x, 1.27, z + 0.02, 2.0, 1.14)
        for offset in (-0.72, 0, 0.72):
            screen(name + " / sub-feed", x + offset, 1.24, z - 0.85, 0.64, 0.47)
        console(name + " / controller", x, main_floor, CYAN, "SURVEILLANCE", 1.8)
    elif kind in (57, 70):
        offset = -0.42 if kind == 57 else 0.42
        color = GREEN if kind == 57 else AMBER
        box(name + " / dispenser", (x + offset, 0.85, z + 0.92),
            (0.78, 0.9, 1.84), WHITE if kind == 57 else HULL, PROPS, 0.10)
        screen(name, x + offset, 0.35, z + 1.25, 0.60, 0.50, color)
        box(name + " / dispenser slot", (x + offset, 0.36, z + 0.60),
            (0.48, 0.04, 0.24), BLACK, PROPS)
        text(name + " / label", "+" if kind == 57 else "CR",
             (x + offset, 0.31, z + 1.61), 0.18, color, PROPS)
    elif kind == 65:
        # The source exit anchor sits against the left wall; the bulkhead faces
        # along the corridor rather than pretending this is a new central portal.
        box(name + " / bulkhead", (x + 0.02, 0.15, z + 1.15),
            (0.18, 2.65, 2.30), BLACK, PROPS)
        for side in (-1.12, 1.42):
            box(name + " / jamb", (x + 0.15, side, z + 1.15),
                (0.15, 0.12, 2.30), VIOLET, PROPS)
        box(name + " / header", (x + 0.15, 0.15, z + 2.3),
            (0.16, 2.65, 0.12), VIOLET, PROPS)
        text(name + " / signage", "EXIT", (x + 0.38, -1.58, z + 2.52), 0.19, VIOLET, PROPS)
    elif kind == 58:
        # Its encoded anchor is 30px above the walkable floor.
        cylinder(name + " / pedestal", (x, 0.55, main_floor + 0.16), 0.85, 0.32, EDGE)
        cylinder(name + " / pedestal light", (x, 0.55, main_floor + 0.34), 0.72, 0.045, VIOLET)
        for angle in (-40, 40, 140, 220):
            radians = math.radians(angle)
            box(name + " / containment strut",
                (x + math.cos(radians) * 0.64, 0.55 + math.sin(radians) * 0.64, main_floor + 1.0),
                (0.13, 0.13, 1.45), EDGE, PROPS)
        cylinder(name + " / data core", (x, 0.55, z + 0.83), 0.27, 1.30, VIOLET)
        for ring_z in (z + 0.18, z + 1.5):
            cylinder(name + " / collar", (x, 0.55, ring_z), 0.45, 0.12, HULL)
        text(name + " / designation", "DATA RETURN", (x, 1.65, main_floor + 3.55),
             0.18, VIOLET, PROPS)
    elif kind == 68:
        box(name + " / bezel", (x, 1.53, z + 0.55), (2.8, 0.18, 1.2), EDGE, PROPS, 0.055)
        box(name + " / display", (x, 1.42, z + 0.55), (2.6, 0.03, 1.0), SCREEN, PROPS)
        text(name + " / team identity", "SILENCER", (x, 1.39, z + 0.5), 0.28, WHITE, PROPS)
        text(name + " / subtitle", "TEAM OPERATIONS", (x, 1.38, z + 0.22), 0.09, CYAN, PROPS)
    else:
        raise ValueError(f"Unhandled source actor {kind}")
    for obj in (set(PROPS.objects) | set(DETAIL.objects)) - before:
        obj.parent = root
        obj.matrix_parent_inverse = root.matrix_world.inverted()
        # Use the known source transform rather than depending on a deferred
        # dependency-graph update of the just-created anchor.
        obj.matrix_parent_inverse.translation = (-x, 0, -z)
        obj["source_actor_index"] = index

text("Operations / room label", "OPERATIONS", (17.0, 1.67, 10.15), 0.32, WHITE)
text("Operations / sublabel", "INTERDIMENSIONAL OUTPOST", (17.0, 1.66, 9.73), 0.13, CYAN)
text("Research / room label", "TECHNOLOGY DECK", (17.0, 1.6, 4.48), 0.23, WHITE)
text("Research / sublabel", "RESEARCH   /   UPGRADES   /   SYSTEMS", (17.0, 1.59, 4.18), 0.11, CYAN)
text("Hatch / label", "LOWER DECK", (24.25, -2.11, 5.47), 0.12, AMBER)

# A human scale cue is not a source actor.
hx, hy, hz = 16.5, -0.65, main_floor
for offset in (-0.14, 0.14):
    box("Scale figure / leg", (hx + offset, hy, hz + 0.47), (0.16, 0.21, 0.80), WHITE, STUDIO, 0.05)
box("Scale figure / torso", (hx, hy, hz + 1.11), (0.46, 0.28, 0.57), WHITE, STUDIO, 0.10)
for offset in (-0.32, 0.32):
    box("Scale figure / arm", (hx + offset, hy, hz + 1.00), (0.13, 0.17, 0.6), WHITE, STUDIO, 0.055)
bpy.ops.mesh.primitive_uv_sphere_add(segments=16, ring_count=8, radius=0.17,
                                   location=(hx, hy, hz + 1.63))
place(bpy.context.object, "Scale figure / head [1.8m]", STUDIO, WHITE)

box("Display plinth", (16.5, 0, -0.45), (35.5, 7, 0.70), GROUND, STUDIO, 0.18)
box("Display plinth / front inlay", (16.5, -3.515, -0.25), (34, 0.018, 0.03), CYAN, STUDIO, 0)
text("Display title", "SILENCER  /  INTERDIMENSIONAL BASE", (16.5, -3.53, -0.58),
     0.28, WHITE, STUDIO)
box("Studio floor", (16, 0, -0.94), (200, 200, 0.25), GROUND, STUDIO, 0)

area("Key / softbox", (8, -14, 25), (15, 0, 5), 4500, (0.68, 0.84, 1.0), 18)
area("Fill / warm", (34, -8, 16), (22, 0, 6), 2600, (1.0, 0.78, 0.57), 13)
area("Front / broad", (16, -20, 8), (16, 0, 6), 2100, (0.69, 0.86, 1.0), 16)
area("Rim / cool", (12, 5, 20), (16, 0, 5), 4000, (0.20, 0.55, 1.0), 12)
for x, z, color in [(4.5, 8.3, (0.1, 0.7, 1)), (15, 10.8, (0.1, 0.7, 1)),
                     (17, 4.5, (0.3, 0.6, 1)), (30, 10.5, (0.55, 0.2, 1))]:
    area("Interior / ceiling bounce", (x, 0.4, z), (x, 0, z - 4), 110, color, 3)

hero = camera("CAM 01 / cutaway overview", (39, -72, 29), (16.5, 0, 5.4), 40.5)
front = camera("CAM 02 / source-aligned elevation", (16.5, -70, 5.5), (16.5, 0, 5.5), 38)
detail = camera("CAM 03 / operations and research", (25, -42, 18), (18, 0, 5.4), 24)
SCENE.camera = hero
SCENE.render.engine = "CYCLES"
SCENE.cycles.samples = 32
SCENE.cycles.use_denoising = True
SCENE.render.resolution_x = 2200
SCENE.render.resolution_y = 1200
SCENE.render.resolution_percentage = 100
SCENE.render.image_settings.file_format = "PNG"
SCENE.world.color = (0.16, 0.16, 0.16)
SCENE.view_settings.view_transform = "AgX"
SCENE.render.film_transparent = False
SCENE.unit_settings.system = "METRIC"
SCENE["source_file"] = SOURCE["source"]
SCENE["source_sha256"] = SOURCE["sha256"]
SCENE["interpretation"] = "Source X/Z collision and actor anchors; invented 4m depth and concept dressing."
SCENE["scale"] = "64 source pixels = 1 design metre. Source has no physical scale."

# Pack the original tile plane for in-scene alignment without external files.
image = bpy.data.images.load(str(HERE / "source-tiles.png"))
image.pack()
refmat = bpy.data.materials.new("Reference / original unlit tiles")
refmat.use_nodes = True
nodes = refmat.node_tree.nodes
nodes.clear()
tex = nodes.new("ShaderNodeTexImage")
tex.image, tex.interpolation = image, "Closest"
emission = nodes.new("ShaderNodeEmission")
output = nodes.new("ShaderNodeOutputMaterial")
refmat.node_tree.links.new(tex.outputs["Color"], emission.inputs["Color"])
refmat.node_tree.links.new(emission.outputs[0], output.inputs["Surface"])
left, _, top = coords(0, 0, 2.7)
right, _, bottom = coords(SOURCE["width_tiles"] * 64, SOURCE["height_tiles"] * 64, 2.7)
plane = mesh("Original tile composite / aligned XZ", [(left, 2.7, bottom), (right, 2.7, bottom),
             (right, 2.7, top), (left, 2.7, top)], [(0, 1, 2, 3)], REFERENCE, refmat)
uv = plane.data.uv_layers.new()
for item, value in zip(uv.data, [(0, 0), (1, 0), (1, 1), (0, 1)]):
    item.uv = value
REFERENCE.hide_render = True
REFERENCE.hide_viewport = True

# Convert procedural curves/text to ordinary meshes in the portable export.
# Keep the native Blender version editable; the conversion is export-only.
bpy.ops.object.select_all(action="DESELECT")
bpy.context.view_layer.update()
for screen_area in bpy.context.screen.areas:
    if screen_area.type == "VIEW_3D":
        screen_area.spaces.active.region_3d.view_perspective = "CAMERA"
        screen_area.spaces.active.shading.color_type = "MATERIAL"
bpy.ops.wm.save_as_mainfile(filepath=str(HERE / "interdimensional-base.blend"), compress=True)
for obj in SCENE.objects:
    if obj.type in {"MESH", "CURVE", "FONT", "EMPTY"} and not obj.hide_render:
        if REFERENCE not in obj.users_collection and STUDIO not in obj.users_collection:
            obj.select_set(True)
bpy.context.view_layer.objects.active = next(obj for obj in bpy.context.selected_objects if obj.type == "MESH")
bpy.ops.object.duplicate()
bpy.ops.object.convert(target="MESH")
bpy.ops.export_scene.gltf(filepath=str(HERE / "interdimensional-base.glb"),
                          export_format="GLB", use_selection=True,
                          export_apply=True, export_extras=True, export_cameras=False,
                          export_lights=False, export_animations=False)
# Reloading also checks that the persistent native scene can be opened.
bpy.ops.wm.open_mainfile(filepath=str(HERE / "interdimensional-base.blend"))
SCENE = bpy.context.scene

manifest = {
    "source_sha256": SOURCE["sha256"],
    "source_platform_objects": len(bpy.data.collections["01 SOURCE / exact collision extrusion"].objects),
    "source_actor_anchors": len(bpy.data.collections["02 SOURCE / actor anchors"].objects),
    "scene_objects": len(SCENE.objects),
    "model": "interdimensional-base.glb",
    "native_scene": "interdimensional-base.blend",
    "renders": ["01-cutaway.png", "02-front-elevation.png", "03-operations-detail.png"],
}
assert manifest["source_platform_objects"] == len(SOURCE["platforms"])
assert manifest["source_actor_anchors"] == len(SOURCE["actors"])
for actor in SOURCE["actors"]:
    obj = bpy.data.objects[f"A{actor['index']:02d} / {actor['name']} [source anchor]"]
    assert (obj.location - Vector(coords(actor["x"], actor["y"]))).length < 0.00001
for platform in SOURCE["platforms"]:
    obj = bpy.data.objects[f"P{platform['index']:02d} / {platform['kind']}"]
    corners = [obj.matrix_world @ Vector(corner) for corner in obj.bound_box]
    actual = [min(v.x for v in corners), max(v.z for v in corners),
              max(v.x for v in corners), min(v.z for v in corners)]
    expected = [coords(platform["x1"], 0)[0], coords(0, platform["y1"])[2],
                coords(platform["x2"], 0)[0], coords(0, platform["y2"])[2]]
    assert all(abs(a - b) < 0.00001 for a, b in zip(actual, expected)), platform
(HERE / "scene-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")

if "--no-render" not in sys.argv:
    for name, filename in [
        ("CAM 01 / cutaway overview", "01-cutaway.png"),
        ("CAM 02 / source-aligned elevation", "02-front-elevation.png"),
        ("CAM 03 / operations and research", "03-operations-detail.png"),
    ]:
        SCENE.camera = bpy.data.objects[name]
        SCENE.render.filepath = str(HERE / filename)
        bpy.ops.render.render(write_still=True)
print("MOCKUP_COMPLETE: native scene, portable model and source alignment saved")
