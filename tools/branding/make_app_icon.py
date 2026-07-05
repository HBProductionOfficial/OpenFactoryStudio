"""Draws the application icon (src/app/ofs.ico, data/icons/ofs.png) from plain shapes."""
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
S = 1024  # master size


def draw_master() -> Image.Image:
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    # tile with a vertical gradient
    grad = Image.new("RGBA", (S, S))
    gd = ImageDraw.Draw(grad)
    top, bottom = (52, 58, 72), (22, 25, 33)
    for y in range(S):
        t = y / (S - 1)
        gd.line([(0, y), (S, y)], fill=tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)) + (255,))
    mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle([32, 32, S - 32, S - 32], radius=200, fill=255)
    img.paste(grad, (0, 0), mask)
    accent = (255, 138, 36, 255)
    light = (240, 242, 246, 255)
    # "O" ring
    d.ellipse([120, 262, 520, 662], outline=accent, width=96)
    # "F" bars
    d.rectangle([590, 262, 682, 762], fill=light)
    d.rectangle([590, 262, 900, 352], fill=light)
    d.rectangle([590, 470, 840, 556], fill=light)
    # base bar ("factory floor")
    d.rounded_rectangle([120, 800, 900, 860], radius=30, fill=accent)
    return img


def main() -> None:
    master = draw_master()
    sizes = [16, 20, 24, 32, 40, 48, 64, 128, 256]
    ico = ROOT / "src" / "app" / "ofs.ico"
    master.save(ico, sizes=[(s, s) for s in sizes])
    png_dir = ROOT / "data" / "icons"
    png_dir.mkdir(parents=True, exist_ok=True)
    master.resize((256, 256), Image.LANCZOS).save(png_dir / "ofs.png")
    print("wrote", ico, "and", png_dir / "ofs.png")


if __name__ == "__main__":
    main()
