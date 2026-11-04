import sys, os, re
from PIL import Image

# tools/rolls.py <assets>: a dodge-roll strip for every champion, drawn from their own side-view
# walk frame: crouch, tuck into a ball and tumble, land on a knee, stand. 60 x 80 frames facing right.
ASSETS = sys.argv[1]
FW, FH = 60, 80
SKIP = {'walk-galin.png'}   # Galin's roll is drawn by hand

def trim(im):
    return im.crop(im.getbbox())

def frame(fig, sx, sy, angle, drop):
    """fig squashed by (sx, sy), turned `angle` degrees clockwise, its feet `drop` px below the ground line."""
    w, h = max(1, round(fig.width * sx)), max(1, round(fig.height * sy))
    im = fig.resize((w, h), Image.NEAREST)
    if angle: im = trim(im.rotate(-angle, resample=Image.NEAREST, expand=True))
    tile = Image.new('RGBA', (FW, FH), (0, 0, 0, 0))
    tile.alpha_composite(im, ((FW - im.width) // 2, FH - 4 - im.height + drop))
    return tile

CHAMPS = re.findall(r'\.walk = "(walk-[^"]+)"', open(os.path.join(ASSETS, '..', 'src', 'Champions.cpp'), encoding='utf-8').read())
for name in CHAMPS:
    src = os.path.join(ASSETS, 'walk', name)
    if name in SKIP: continue
    sheet = Image.open(src).convert('RGBA')
    cw, ch = sheet.width // 5, sheet.height // 3
    if sheet.height % 3 or sheet.width % 5: continue          # not a champion walk sheet
    k = FH / ch                                                # same size as their walking figure
    stand = trim(sheet.crop((0, 2 * ch, cw, 3 * ch)))
    stride = trim(sheet.crop((cw, 2 * ch, 2 * cw, 3 * ch)))
    stand = stand.resize((max(1, round(stand.width * k)), max(1, round(stand.height * k))), Image.NEAREST)
    stride = stride.resize((max(1, round(stride.width * k)), max(1, round(stride.height * k))), Image.NEAREST)
    ball = stand.height * 0.5                                  # how big the tucked-up ball is
    poses = [
        frame(stride, 1.05, 0.80, 18, 0),    # crouch, leaning into it
        frame(stand, ball / stand.width, ball / stand.height, 90, 0),    # curled into a ball, going over
        frame(stand, ball / stand.width, ball / stand.height, 180, 0),   # upside down
        frame(stand, ball / stand.width, ball / stand.height, 270, 0),   # coming round
        frame(stride, 1.05, 0.78, 8, 0),     # on one knee
        frame(stand, 1.00, 1.00, 0, 0),      # up again
    ]
    strip = Image.new('RGBA', (FW * len(poses), FH), (0, 0, 0, 0))
    for i, p in enumerate(poses): strip.alpha_composite(p, (i * FW, 0))
    out = os.path.join(ASSETS, 'dungeon', 'roll-' + name[len('walk-'):])
    strip.save(out, optimize=True)
    print(out)
