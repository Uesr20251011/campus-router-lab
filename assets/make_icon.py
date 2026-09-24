"""Convert the supplied cover image into transparent app and Windows icons."""

from pathlib import Path

from PIL import Image, ImageFilter


HERE = Path(__file__).resolve().parent
source = Image.open(HERE / "icon-source.png").convert("RGB")

# The supplied artwork has a dark presentation canvas around its rounded card.
# Keep the card itself and its antialiased edge; make only that outer canvas transparent.
silhouette = source.convert("L").point(lambda brightness: 255 if brightness >= 90 else 0)
bounds = silhouette.getbbox()
if bounds is None:
    raise ValueError("No icon artwork found in icon-source.png")

center_x = (bounds[0] + bounds[2]) / 2
center_y = (bounds[1] + bounds[3]) / 2
side = round(max(bounds[2] - bounds[0], bounds[3] - bounds[1]) / 0.94)
crop_box = (round(center_x - side / 2), round(center_y - side / 2),
            round(center_x + side / 2), round(center_y + side / 2))

alpha = silhouette.filter(ImageFilter.MinFilter(3)).filter(ImageFilter.GaussianBlur(0.8))
artwork = source.convert("RGBA")
artwork.putalpha(alpha)
icon = artwork.crop(crop_box).resize((256, 256), Image.Resampling.LANCZOS)

icon.save(HERE / "app-icon.png")
icon.save(HERE / "app-icon.ico", sizes=[
    (16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)
])
