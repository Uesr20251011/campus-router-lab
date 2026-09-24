"""Generate the editable app symbol and its Windows icon variants."""

from pathlib import Path
from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
SVG = """<svg xmlns="http://www.w3.org/2000/svg" width="256" height="256" viewBox="0 0 256 256">
  <rect x="8" y="8" width="240" height="240" rx="58" fill="#DCF5ED"/>
  <path d="M65 177 115 101 190 67" fill="none" stroke="#4DAE9B" stroke-width="17" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M115 101 190 178" fill="none" stroke="#9B8BDD" stroke-width="14" stroke-linecap="round"/>
  <circle cx="65" cy="177" r="21" fill="#FFFFFF" stroke="#4DAE9B" stroke-width="10"/>
  <circle cx="115" cy="101" r="24" fill="#FFFFFF" stroke="#4DAE9B" stroke-width="10"/>
  <circle cx="190" cy="67" r="19" fill="#FFFFFF" stroke="#4DAE9B" stroke-width="10"/>
  <circle cx="190" cy="178" r="21" fill="#FFFFFF" stroke="#9B8BDD" stroke-width="10"/>
</svg>
"""


def point(xy, factor):
    return tuple(round(value * factor) for value in xy)


def render(size):
    factor = size / 256
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle(point((8, 8, 248, 248), factor), radius=round(58 * factor), fill="#DCF5ED")
    draw.line([point(p, factor) for p in ((65, 177), (115, 101), (190, 67))],
              fill="#4DAE9B", width=round(17 * factor), joint="curve")
    draw.line([point(p, factor) for p in ((115, 101), (190, 178))],
              fill="#9B8BDD", width=round(14 * factor))
    for x, y, radius, color in ((65, 177, 21, "#4DAE9B"),
                                (115, 101, 24, "#4DAE9B"),
                                (190, 67, 19, "#4DAE9B"),
                                (190, 178, 21, "#9B8BDD")):
        bounds = point((x-radius, y-radius, x+radius, y+radius), factor)
        draw.ellipse(bounds, fill="white", outline=color, width=round(10 * factor))
    return image


(HERE / "app-icon.svg").write_text(SVG, encoding="utf-8")
high_resolution = render(1024)
icon = high_resolution.resize((256, 256), Image.Resampling.LANCZOS)
icon.save(HERE / "app-icon.png")
icon.save(HERE / "app-icon.ico", sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
