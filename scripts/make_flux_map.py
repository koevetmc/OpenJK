import argparse
from pathlib import Path


def brush(low, high, texture="flux/concrete", slope=None):
    x0, y0, z0 = low
    x1, y1, z1 = high
    top0, top1 = (z1, z1) if slope is None else slope
    faces = (
        ((x0, y0, z0), (x0, y1, z0), (x1, y1, z0)),
        ((x0, y0, top0), (x1, y0, top1), (x1, y1, top1)),
        ((x0, y0, z0), (x1, y0, z0), (x1, y0, z1)),
        ((x0, y1, z0), (x0, y1, z1), (x1, y1, z1)),
        ((x0, y0, z0), (x0, y0, z1), (x0, y1, z1)),
        ((x1, y0, z0), (x1, y1, z0), (x1, y1, z1)),
    )
    lines = ["{"]
    for face in faces:
        points = " ".join("( %g %g %g )" % point for point in face)
        lines.append(f"{points} {texture} 0 0 0 0.5 0.5 0 0 0")
    return "\n".join(lines + ["}"])


def entity(properties, brushes=()):
    lines = ["{"] + [f'"{key}" "{value}"' for key, value in properties.items()]
    return "\n".join(lines + list(brushes) + ["}"])


def make_map():
    room = [
        brush((-1056, -800, -32), (1056, 800, 0)),
        brush((-1056, -800, 512), (1056, 800, 544)),
        brush((-1056, -800, 0), (-1024, 800, 512)),
        brush((1024, -800, 0), (1056, 800, 512)),
        brush((-1024, -800, 0), (1024, -768, 512)),
        brush((-1024, 768, 0), (1024, 800, 512)),
        brush((-600, 300, 0), (-200, 600, 96), slope=(0, 96)),
        brush((-200, 300, 0), (200, 600, 96)),
        brush((200, 300, 0), (600, 600, 96), slope=(96, 0)),
        brush((-400, -400, 0), (400, -300, 32), "flux/rail"),
    ]
    entities = [entity({"classname": "worldspawn", "message": "Flux Foundry", "ambient": "35"}, room)]
    entities.append(entity({"classname": "info_player_deathmatch", "origin": "-800 0 32", "angle": "0"}))
    entities.append(entity({"classname": "info_player_deathmatch", "origin": "800 0 32", "angle": "180"}))
    for x, y in ((-720, 0), (-720, -200), (720, 0)):
        entities.append(entity({"classname": "misc_fluxboard", "origin": f"{x} {y} 2", "spawnflags": "2"}))
    entities.append(entity({"classname": "func_fluxrail"}, [brush((-400, -5, 0), (400, 5, 24), "flux/rail")]))
    for x in (-600, 0, 600):
        entities.append(entity({"classname": "light", "origin": f"{x} 0 400", "light": "800"}))
    return "\n".join(entities) + "\n"


def main():
    parser = argparse.ArgumentParser(description="Generate the original Flux Foundry Radiant map source.")
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[1] / "assets/flux/maps/flux_foundry.map")
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(make_map(), encoding="ascii", newline="\n")
    print(args.output)


if __name__ == "__main__":
    main()
